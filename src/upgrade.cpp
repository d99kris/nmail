// upgrade.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "upgrade.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <unistd.h>

#include <sys/stat.h>

#include "sysutil.h"
#include "version.h"

int Upgrade::Run(const std::string& p_Version, mode_t p_Umask)
{
  // exit status 0 upgraded, 3 up to date (from install.sh), 5 failed
  static const int failed = 5;
#if defined(NMAIL_BUILD_STATIC_EXTLIBS)
  const bool isRelease = (Version::GetBuildOrigin() == "github");
#else
  const bool isRelease = false;
#endif
  if (!isRelease)
  {
    std::cerr <<
      "error: upgrade is only supported for pre-built release binaries, upgrade\n"
      "       using the package manager or method nmail was installed with.\n";
    return failed;
  }

  // installed by install.sh as <prefix>/bin/nmail
  char realPath[PATH_MAX];
  const std::string selfPath = SysUtil::GetSelfPath();
  if (selfPath.empty() || (realpath(selfPath.c_str(), realPath) == nullptr))
  {
    std::cerr << "error: cannot determine nmail binary path\n";
    return failed;
  }

  const std::string binPath = realPath;
  const std::string binDir = binPath.substr(0, binPath.rfind('/'));
  const size_t binDirPos = binDir.rfind('/');
  if ((binDirPos == std::string::npos) || (binDir.substr(binDirPos + 1) != "bin"))
  {
    std::cerr <<
      "error: nmail binary " << realPath << " is not in a bin dir,\n"
      "       re-install using install.sh\n";
    return failed;
  }

  if (access(binDir.c_str(), W_OK) != 0)
  {
    std::cerr <<
      "error: install dir " << binDir << " is not writable by the current user,\n"
      "       re-run with sudo, e.g.: sudo nmail -u\n";
    return failed;
  }

  const std::string prefix = (binDirPos == 0) ? "/" : binDir.substr(0, binDirPos);

  // NMAIL_INSTALL_URL allows testing a local install.sh, e.g. file://$PWD/utils/install.sh
  const char* envUrl = getenv("NMAIL_INSTALL_URL");
  const std::string url = ((envUrl != nullptr) && (*envUrl != '\0')) ? envUrl :
    "https://raw.githubusercontent.com/d99kris/nmail/master/utils/install.sh";

  // values passed as positional args to avoid shell quoting
  static const std::string script =
    "if command -v curl >/dev/null 2>&1; then s=\"$(curl -fsSL \"$1\")\"\n"
    "elif command -v wget >/dev/null 2>&1; then s=\"$(wget -qO- \"$1\")\"\n"
    "else echo \"error: need curl or wget to download\" >&2; exit 5; fi ||\n"
    "  { echo \"error: failed to download $1\" >&2; exit 5; }\n"
    "NMAIL_PREFIX=\"$2\" NMAIL_VERSION=\"$3\" NMAIL_CURRENT_VERSION=\"$4\" \\\n"
    "  exec bash -c \"$s\" install.sh\n";

  // exec so the running binary is not held during install, with user umask restored
  const std::string curVersion = Version::GetAppVersion();
  umask(p_Umask);
  execlp("bash", "bash", "-c", script.c_str(), "nmail-upgrade", url.c_str(), prefix.c_str(),
         p_Version.c_str(), curVersion.c_str(), (char*)nullptr);
  std::cerr << "error: failed to run bash: " << strerror(errno) << "\n";
  return failed;
}
