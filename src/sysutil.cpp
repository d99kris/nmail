// sysutil.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "sysutil.h"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <netdb.h>
#include <termios.h>
#include <unistd.h>

#if defined(__OpenBSD__) || defined(__FreeBSD__)
#include <sys/utsname.h>
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef __APPLE__
#include <libproc.h>
#endif

#ifdef __linux__
#include <linux/limits.h>
#endif

#include "libetpan_help.h"
#include <libetpan/mailimap.h>
#include <sqlite3.h>

#include "fileutil.h"
#include "loghelp.h"
#include "strutil.h"
#include "version.h"

std::string SysUtil::ExtractEmbeddedScript(const std::string& p_Name,
                                           const unsigned char* p_Data, size_t p_Size)
{
  // Write the embedded helper script to the (transient) temp dir and return its
  // path. Invoked via its interpreter (python3/bash <path>), so no exec bit is
  // needed. Re-extracted on each use as the temp dir is managed by nmail.
  const std::string scriptPath = FileUtil::GetTempDir() + p_Name;
  FileUtil::WriteFile(scriptPath, std::string(reinterpret_cast<const char*>(p_Data), p_Size));
  return scriptPath;
}

std::string SysUtil::RunCommand(const std::string& p_Cmd)
{
  std::string output;
  std::string outPath = FileUtil::GetTempFilename(".txt");
  std::string command = p_Cmd + " 2> /dev/null > " + outPath;
  if (SysUtil::System(command) == 0)
  {
    output = FileUtil::ReadFile(outPath);
  }
  else
  {
    LOG_WARNING("external command failed: %s", command.c_str());
    DetectCommandNotPresent(command);
  }

  FileUtil::DeleteFile(outPath);
  return output;
}

void SysUtil::DetectCommandNotPresent(const std::string& p_Cmd)
{
  std::vector<std::string> cmdVec = StrUtil::Split(p_Cmd, ' ');
  const std::string programName = (cmdVec.size() > 0) ? cmdVec.at(0) : "";
  const std::string whichCmd = "which " + programName + " > /dev/null 2>&1";
  if (SysUtil::System(whichCmd) != 0)
  {
    LOG_WARNING("program not found, please ensure '%s' is installed", programName.c_str());
  }
}

std::string SysUtil::GetSystemOs()
{
#if defined(__APPLE__)
  const std::string& name = RunCommand("sw_vers -productName | tr -d '\n'");
  const std::string& version = RunCommand("sw_vers -productVersion | tr -d '\n'");
  return name + " " + version;
#elif defined(__linux__)
  return RunCommand("grep PRETTY_NAME /etc/os-release 2> /dev/null | "
                    "cut -d= -f2 | sed -e \"s/\\\"//g\" | tr -d '\n'");
#else
  return "";
#endif
}

std::string SysUtil::GetLinkedLibs(const std::string& p_Prog)
{
#if defined(__APPLE__)
  return RunCommand("otool -L " + p_Prog + " 2> /dev/null | tail -n +2 | awk '{$1=$1};1'");
#elif defined(__linux__)
  return RunCommand("ldd " + p_Prog + " 2> /dev/null | awk '{$1=$1};1'");
#else
  (void)p_Prog;
  return "";
#endif
}

std::string SysUtil::GetSelfPath()
{
#if defined(__APPLE__)
  char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_pidpath(getpid(), pathbuf, sizeof(pathbuf)) > 0)
  {
    return std::string(pathbuf);
  }
#elif defined(__linux__)
  char pathbuf[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", pathbuf, sizeof(pathbuf));
  if (count > 0)
  {
    return std::string(pathbuf, count);
  }
#endif
  return "";
}

std::string SysUtil::GetLibetpanVersion()
{
  std::string version;
#if defined(LIBETPAN_VERSION_MAJOR) && defined(LIBETPAN_VERSION_MINOR)
  version += std::to_string(LIBETPAN_VERSION_MAJOR) + "." + std::to_string(LIBETPAN_VERSION_MINOR);
#if defined(LIBETPAN_VERSION_MICRO)
  version += "." + std::to_string(LIBETPAN_VERSION_MICRO);
#endif
#ifdef LIBETPAN_API_CURRENT
  version += " API " + std::to_string(LIBETPAN_API_CURRENT);
#endif
#endif
#if defined(LIBETPAN_CUSTOM)
  version += " Custom";
#else
  version += " System";
#endif
  return version;
}

std::string SysUtil::GetUname()
{
#if defined(__APPLE__) || defined(__linux__)
  return RunCommand("uname -a 2> /dev/null");
#else
  return "";
#endif
}

std::string SysUtil::GetSQLiteVersion()
{
  return std::string(SQLITE_VERSION);
}

namespace
{
  // Removes any "(...)" groups, including a single space preceding an opening
  // parenthesis, e.g. "debian gnu/linux 12 (bookworm) arm64" -> "debian gnu/linux 12 arm64".
  std::string StripParentheses(const std::string& p_Str)
  {
    std::string result = p_Str;
    size_t open = std::string::npos;
    while ((open = result.find('(')) != std::string::npos)
    {
      const size_t close = result.find(')', open);
      if (close == std::string::npos)
      {
        break;
      }

      size_t start = open;
      if ((start > 0) && (result[start - 1] == ' '))
      {
        --start;
      }

      result.erase(start, close - start + 1);
    }

    return result;
  }
}

std::string SysUtil::GetBuildInfo()
{
  // Build origin (github / local, or packager-provided). Sourced from Version so
  // the volatile buildinfo.h stays out of this heavy translation unit.
  std::string origin = Version::GetBuildOrigin();

  // Build type
#if defined(NMAIL_BUILD_RELEASE)
  std::string type = "release";
#elif defined(NMAIL_BUILD_DEBUG)
  std::string type = "debug";
#elif defined(NMAIL_BUILD_RELWITHDEBINFO)
  std::string type = "reldbg";
#else
  std::string type = "unknown";
#endif

  // External linkage
#if defined(NMAIL_BUILD_STATIC_EXTLIBS)
  std::string linkage = "static";
#else
  std::string linkage = "dynamic";
#endif

  // Build git sha (from Version, see origin note above)
  std::string sha = Version::GetBuildGitSha();

  return StrUtil::ToLower(origin + " " + type + " " + linkage + " " + sha);
}

std::string SysUtil::GetCompiler(bool p_Verbose)
{
#if defined(__VERSION__)
#if !defined(__clang__) && defined(__GNUC__)
  std::string compiler = "GCC " __VERSION__;
#else
  std::string compiler = __VERSION__;
#endif
#else
  std::string compiler = "Unknown compiler";
#endif

#if defined(__linux__)
#if defined(__GLIBC__) && defined(__GLIBC_MINOR__)
  std::stringstream sslibc;
  sslibc << "glibc " << __GLIBC__ << "." << __GLIBC_MINOR__;
  std::string libc = sslibc.str();
#elif defined(NMAIL_BUILD_MUSL)
  // musl provides no version macro; NMAIL_BUILD_MUSL is set by CMake from the
  // compiler target triple (see CMakeLists.txt).
  std::string libc = "musl";
#elif defined(__ANDROID__)
  std::string libc = "bionic";
#else
  std::string libc = "unknown";
#endif
#else
  // macOS libc (part of libSystem) has no conventional short name; leave empty.
  std::string libc;
#endif

  std::string compilerLibc = compiler + (!libc.empty() ? " " + libc : "");
  if (!p_Verbose)
  {
    compilerLibc = StripParentheses(compilerLibc);
  }

  return StrUtil::ToLower(compilerLibc);
}

std::string SysUtil::GetOsArch(bool p_Verbose)
{
  static const std::string os = []()
  {
#if defined(__linux__)
    std::string str = FileUtil::ReadFile("/etc/os-release");
    std::string prettyName = StrUtil::ExtractString(str, "PRETTY_NAME=\"", "\"");
    return prettyName.empty() ? "Linux" : prettyName;
#elif defined(__APPLE__)
    std::string str = FileUtil::ReadFile("/System/Library/CoreServices/SystemVersion.plist");
    std::string name = StrUtil::ExtractString(str, "<key>ProductName</key>\n\t<string>", "</string>");
    std::string ver = StrUtil::ExtractString(str, "<key>ProductVersion</key>\n\t<string>", "</string>");
    return name + " " + ver;
#elif defined(__OpenBSD__)
    struct utsname uts;
    return (uname(&uts) == 0) ? (std::string(uts.sysname) + " " + uts.release) : std::string("OpenBSD");
#elif defined(__FreeBSD__)
    struct utsname uts;
    return (uname(&uts) == 0) ? (std::string(uts.sysname) + " " + uts.release) : std::string("FreeBSD");
#else
    return "Unknown";
#endif
  }();

  static const std::string arch = []()
  {
#if defined(__arm__)
    return "arm";
#elif defined(__aarch64__)
    return "arm64";
#elif defined(__x86_64__) || defined(__amd64__)
    return "x86_64";
#elif defined(__i386__)
    return "i386";
#else
    return std::to_string(sizeof(void*) * 8) + "-bit";
#endif
  }();

  std::string osArch = os + " " + arch;
  if (!p_Verbose)
  {
    osArch = StripParentheses(osArch);
  }

  return StrUtil::ToLower(osArch);
}

bool SysUtil::IsIpAddress(const std::string& p_Str)
{
  struct addrinfo addrhints;
  memset(&addrhints, 0, sizeof(struct addrinfo));
  addrhints.ai_family = AF_UNSPEC;
  addrhints.ai_flags = AI_NUMERICHOST;
  struct addrinfo* addr = nullptr;
  bool rv = (getaddrinfo(p_Str.c_str(), NULL, &addrhints, &addr) == 0);
  if (rv && (addr != nullptr))
  {
    freeaddrinfo(addr);
  }

  return rv;
}

bool SysUtil::IsProcessRunning(pid_t p_Pid)
{
  const bool noSuchProcess = (kill(p_Pid, 0) != 0) && (errno == ESRCH);
  return !noSuchProcess;
}

bool SysUtil::IsSelfProcess(pid_t p_Pid)
{
  static pid_t selfPid = getpid();
  return (p_Pid == selfPid);
}

int SysUtil::System(const std::string& p_Cmd)
{
#if defined(HAVE_TERMUX)
  static const std::string shPath = "/data/data/com.termux/files/usr/bin/sh";
#else
  static const std::string shPath = "/bin/sh";
#endif

  // Block SIGCHLD and ignore SIGINT/SIGQUIT in parent per POSIX system() spec
  struct sigaction saIgnore;
  struct sigaction saOrigInt;
  struct sigaction saOrigQuit;
  saIgnore.sa_handler = SIG_IGN;
  sigemptyset(&saIgnore.sa_mask);
  saIgnore.sa_flags = 0;
  sigaction(SIGINT, &saIgnore, &saOrigInt);
  sigaction(SIGQUIT, &saIgnore, &saOrigQuit);

  sigset_t blockChld;
  sigset_t origMask;
  sigemptyset(&blockChld);
  sigaddset(&blockChld, SIGCHLD);
  sigprocmask(SIG_BLOCK, &blockChld, &origMask);

  pid_t pid = fork();
  if (pid == 0)
  {
    // Child: restore original signal dispositions and mask
    sigaction(SIGINT, &saOrigInt, nullptr);
    sigaction(SIGQUIT, &saOrigQuit, nullptr);
    sigprocmask(SIG_SETMASK, &origMask, nullptr);

    execl(shPath.c_str(), "sh", "-c", p_Cmd.c_str(), (char*)nullptr);
    _exit(127);
  }

  int status = -1;
  if (pid > 0)
  {
    // Retry waitpid on EINTR (e.g. from SIGWINCH during terminal resize)
    while (waitpid(pid, &status, 0) < 0)
    {
      if (errno != EINTR)
      {
        status = -1;
        break;
      }
    }
  }

  // Restore original signal dispositions and mask
  sigaction(SIGINT, &saOrigInt, nullptr);
  sigaction(SIGQUIT, &saOrigQuit, nullptr);
  sigprocmask(SIG_SETMASK, &origMask, nullptr);

  return status;
}

static struct termios s_GetPassTermios;

static void GetPassSignalHandler(int p_Signal)
{
  // restore terminal echo before terminating
  tcsetattr(STDIN_FILENO, TCSANOW, &s_GetPassTermios);
  if (write(STDOUT_FILENO, "\n", 1) != 1)
  {
    // ignore, only async-signal-safe calls allowed here
  }

  signal(p_Signal, SIG_DFL);
  raise(p_Signal);
}

std::string SysUtil::GetPass()
{
  std::string pass;
  struct termios told, tnew;

  if (tcgetattr(STDIN_FILENO, &told) == 0)
  {
    memcpy(&tnew, &told, sizeof(struct termios));
    tnew.c_lflag &= ~ECHO;
    s_GetPassTermios = told;
    void (*oldHandler)(int) = signal(SIGINT, GetPassSignalHandler);
    if (oldHandler == SIG_IGN)
    {
      signal(SIGINT, SIG_IGN);
    }

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tnew) == 0)
    {
      std::getline(std::cin, pass);
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &told);
      std::cout << std::endl;
    }

    signal(SIGINT, oldHandler);
  }

  return pass;
}
