// util.cpp
//
// Copyright (c) 2019-2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "util.h"

#include <algorithm>
#include <csignal>
#include <map>
#include <set>

#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <libgen.h>
#include <termios.h>
#include <unistd.h>

#ifdef __APPLE__
#include <libproc.h>
#endif

#ifdef __linux__
#include <linux/limits.h>
#endif

#include <libetpan/libetpan.h>
#include <ncursesw/ncurses.h>
#include <sqlite3.h>

#include "apathy/path.hpp"

#include "loghelp.h"
#include "serialized.h"

std::mutex ThreadRegister::m_Mutex;
std::map<pthread_t, std::string> ThreadRegister::m_Threads;

std::string Util::m_HtmlToTextConvertCmd;
std::string Util::m_TextToHtmlConvertCmd;
std::string Util::m_ExtViewerCmd;
std::string Util::m_ApplicationDir;
std::string Util::m_PagerCmd;
std::string Util::m_EditorCmd;
int Util::m_OrgStdErr = -1;
int Util::m_NewStdErr = -1;

bool Util::Exists(const std::string &p_Path)
{
  struct stat sb;
  return (stat(p_Path.c_str(), &sb) == 0);
}

bool Util::NotEmpty(const std::string& p_Path)
{
  struct stat sb;
  return (stat(p_Path.c_str(), &sb) == 0) && (sb.st_size > 0);
}

std::string Util::ReadFile(const std::string &p_Path)
{
  std::ifstream file(p_Path, std::ios::binary);
  std::stringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

void Util::WriteFile(const std::string &p_Path, const std::string &p_Str)
{
  MkDir(DirName(p_Path));
  std::ofstream file(p_Path, std::ios::binary);
  file << p_Str;
}

std::wstring Util::ReadWFile(const std::string &p_Path)
{
  std::locale::global(std::locale(""));
  std::wifstream file(p_Path, std::ios::binary);
  std::wstringstream wss;
  wss << file.rdbuf();
  return wss.str();
}

void Util::WriteWFile(const std::string &p_Path, const std::wstring &p_WStr)
{
  MkDir(DirName(p_Path));
  std::locale::global(std::locale(""));
  std::wofstream file(p_Path, std::ios::binary);
  file << p_WStr;
}

std::string Util::BaseName(const std::string &p_Path)
{
  char* path = strdup(p_Path.c_str());
  char* bname = basename(path);
  std::string rv(bname);
  free(path);
  return rv;
}

std::string Util::RemoveFileExt(const std::string& p_Path)
{
  size_t lastPeriod = p_Path.find_last_of(".");
  if (lastPeriod == std::string::npos) return p_Path;

  return p_Path.substr(0, lastPeriod);
}

std::string Util::GetFileExt(const std::string& p_Path)
{
  size_t lastPeriod = p_Path.find_last_of(".");
  if (lastPeriod == std::string::npos) return "";

  return p_Path.substr(lastPeriod);
}

std::string Util::DirName(const std::string &p_Path)
{
  char *buf = strdup(p_Path.c_str());
  std::string rv = std::string(dirname(buf));
  free(buf);
  return rv;
}

std::vector<std::string> Util::ListDir(const std::string& p_Folder)
{
  std::vector<std::string> files;
  const std::vector<apathy::Path>& paths = apathy::Path::listdir(p_Folder);
  for (auto& path : paths)
  {
    files.push_back(path.filename());
  }
  return files;
}

std::set<Fileinfo, FileinfoCompare> Util::ListPaths(const std::string& p_Folder)
{
  std::set<Fileinfo, FileinfoCompare> fileinfos;
  fileinfos.insert(Fileinfo("..", -1));

  const std::vector<apathy::Path>& paths = apathy::Path::listdir(p_Folder);
  for (auto& path : paths)
  {
    Fileinfo fileinfo(path.filename(), path.is_directory() ? -1 : path.size());
    fileinfos.insert(fileinfo);
  }
  return fileinfos;
}

std::string Util::GetPrefixedSize(ssize_t p_Size)
{
  std::vector<std::string> prefixes({ "B", "KB", "MB", "GB", "TB", "PB" });
  size_t i = 0;
  for (i = 0; (i < prefixes.size()) && (p_Size >= 1024); i++, (p_Size /= 1024))
  {
  }

  return std::to_string(p_Size) + " " + prefixes.at(i);
}

std::string Util::GetCurrentWorkingDir()
{
  return apathy::Path::cwd().absolute().sanitize().string();
}

std::string Util::AbsolutePath(const std::string& p_Path)
{
  return apathy::Path(p_Path).absolute().sanitize().string();
}

void Util::MkDir(const std::string &p_Path)
{
  apathy::Path::makedirs(p_Path);
}

void Util::RmDir(const std::string &p_Path)
{
  if (!p_Path.empty())
  {
    apathy::Path::rmdirs(apathy::Path(p_Path));
  }
}

void Util::Touch(const std::string &p_Path)
{
  utimensat(0, p_Path.c_str(), NULL, 0);
}

std::string Util::GetApplicationDir()
{
  return m_ApplicationDir;
}

void Util::SetApplicationDir(const std::string &p_Path)
{
  m_ApplicationDir = p_Path + "/";
}

std::string Util::GetTempDir()
{
  return GetApplicationDir() + std::string("temp/");
}

void Util::InitTempDir()
{
  Util::RmDir(GetTempDir());
  Util::MkDir(GetTempDir());
}

void Util::CleanupTempDir()
{
  Util::RmDir(GetTempDir());
}

std::string Util::GetAttachmentsTempDir()
{
  return GetTempDir() + std::string("attachments/");
}

void Util::CleanupAttachmentsTempDir()
{
  Util::RmDir(GetAttachmentsTempDir());
}

std::string Util::GetPreviewTempDir()
{
  return GetTempDir() + std::string("preview/");
}

void Util::CleanupPreviewTempDir()
{
  Util::RmDir(GetPreviewTempDir());
}

std::string Util::GetTempFilename(const std::string &p_Suffix)
{
  std::string name = GetTempDir() + std::string("tmpfile.XX" "XX" "XX") + p_Suffix;
  char* cname = strdup(name.c_str());
  int fd = mkstemps(cname, p_Suffix.length());
  if (fd != -1)
  {
    close(fd);
  }

  name = std::string(cname);
  free(cname);
  return name;
}

// returns a unique temporary directory under GetTemp() dir
std::string Util::GetTempDirectory()
{
  std::string name = GetTempDir() + std::string("tmpdir.XX" "XX" "XX");
  char* cname = strdup(name.c_str());
  char* newcname = mkdtemp(cname);

  if (newcname != NULL)
  {
    name = std::string(cname);
  }
  else
  {
    name = "";
  }
  
  free(cname);
  return name;
}

void Util::DeleteFile(const std::string &p_Path)
{
  unlink(p_Path.c_str());
}

time_t Util::MailtimeToTimet(mailimf_date_time *p_Dt)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "%04i-%02i-%02i %02i:%02i:%02i",
           p_Dt->dt_year, p_Dt->dt_month, p_Dt->dt_day, p_Dt->dt_hour,
           p_Dt->dt_min, p_Dt->dt_sec);
  int offs = p_Dt->dt_zone;

  struct tm tm;
  memset(&tm, 0, sizeof(struct tm));
  strptime(buf, "%Y-%m-%d %H:%M:%S", &tm);
  tm.tm_isdst = -1;

  time_t t = timegm(&tm);
  int offs_h = offs / 100;
  int offs_m = offs % 100;
  t -= offs_h * 3600;
  t -= offs_m * 60;

  return t;
}

std::string Util::GetHtmlToTextConvertCmd()
{
  if (!m_HtmlToTextConvertCmd.empty()) return m_HtmlToTextConvertCmd;

  static std::string defaultHtmlToTextConvertCmd = GetDefaultHtmlToTextConvertCmd();
  
  return defaultHtmlToTextConvertCmd;
}

void Util::SetHtmlToTextConvertCmd(const std::string &p_HtmlToTextConvertCmd)
{
  m_HtmlToTextConvertCmd = p_HtmlToTextConvertCmd;
}

std::string Util::GetDefaultHtmlToTextConvertCmd()
{
  std::string result;
  const std::string& commandOutPath = Util::GetTempFilename(".txt");
  const std::string& command =
    std::string("which lynx elinks links 2> /dev/null | head -1 > ") + commandOutPath;
  if (system(command.c_str()) == 0)
  {
    std::string output = Util::ReadFile(commandOutPath);
    output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
    if (!output.empty())
    {
      if (output.find("/lynx") != std::string::npos)
      {
        result = "lynx -assume_charset=utf-8 -display_charset=utf-8 -dump";
      }
      else if (output.find("/elinks") != std::string::npos)
      {
        result = "elinks -dump-charset utf-8 -dump";
      }
      else if (output.find("/links") != std::string::npos)
      {
        result = "links -codepage utf-8 -dump";
      }
    }
  }

  Util::DeleteFile(commandOutPath);

  return result;
}

std::string Util::GetTextToHtmlConvertCmd()
{
  if (!m_TextToHtmlConvertCmd.empty()) return m_TextToHtmlConvertCmd;

  static std::string defaultTextToHtmlConvertCmd = GetDefaultTextToHtmlConvertCmd();

  return defaultTextToHtmlConvertCmd;
}

void Util::SetTextToHtmlConvertCmd(const std::string& p_TextToHtmlConvertCmd)
{
  m_TextToHtmlConvertCmd = p_TextToHtmlConvertCmd;
}

std::string Util::GetDefaultTextToHtmlConvertCmd()
{
  return "markdown";
}

std::string Util::ConvertTextToHtml(const std::string& p_Text)
{
  std::string text = p_Text;
  ReplaceString(text, "\n", "  \n"); // prepend line-breaks with double spaces to enforce them
  const std::string& tempPath = GetTempFilename(".md");
  Util::WriteFile(tempPath, text);
  const std::string& textToHtmlCmd = GetTextToHtmlConvertCmd();
  const std::string& cmd = textToHtmlCmd + " " + tempPath;
  const std::string& htmlText = RunCommand(cmd);
  Util::DeleteFile(tempPath);

  return htmlText;
}

std::string Util::GetExtViewerCmd()
{
  if (!m_ExtViewerCmd.empty()) return m_ExtViewerCmd;

  static std::string defaultExtViewerCmd = GetDefaultExtViewerCmd();
  
  return defaultExtViewerCmd;
}

void Util::SetExtViewerCmd(const std::string& p_ExtViewerCmd)
{
  m_ExtViewerCmd = p_ExtViewerCmd;
}

std::string Util::GetDefaultExtViewerCmd()
{
#if defined(__APPLE__)
  return "open";
#elif defined(__linux__)
  return "xdg-open";
#else
  return "";
#endif
}

int Util::OpenInExtViewer(const std::string& p_Path)
{
  std::string cmd = GetExtViewerCmd() + " \"" + p_Path + "\"";
  return system(cmd.c_str());
}

void Util::ReplaceString(std::string &p_Str, const std::string &p_Search,
                         const std::string &p_Replace)
{
  size_t pos = 0;
  while ((pos = p_Str.find(p_Search, pos)) != std::string::npos)
  {
    p_Str.replace(pos, p_Search.length(), p_Replace);
    pos += p_Replace.length();
  }
}

std::string Util::ReduceIndent(const std::string &p_Str, int p_Cnt)
{
  std::string tmpstr = "\n" + p_Str;
  std::string findstr = "\n ";
  std::string replacestr = "\n";
  for (int i = 0; i < p_Cnt; ++i)
  {
    ReplaceString(tmpstr, findstr, replacestr);
  }

  return tmpstr.substr(1);
}

std::string Util::AddIndent(const std::string &p_Str, const std::string &p_Indent)
{
  std::string tmpstr = "\n" + p_Str;
  std::string findstr = "\n";
  std::string replacestr = "\n" + p_Indent;
  ReplaceString(tmpstr, findstr, replacestr);

  return tmpstr.substr(1);
}

std::string Util::MakeReplySubject(const std::string &p_Str)
{
  std::set<std::string> replyPrefixes = { "re:", "sv:" };
  std::string oldPrefix = ToLower(p_Str.substr(0, 3));
  if (replyPrefixes.find(oldPrefix) == replyPrefixes.end())
  {
    return "Re: " + p_Str;
  }
  return p_Str;
}

std::string Util::MakeForwardSubject(const std::string &p_Str)
{
  std::set<std::string> replyPrefixes = { "fw", "fwd", "vb" };
  std::vector<std::string> oldSubjectSplit= Split(p_Str, ':');
  bool hasFwdPrefix = false;
  if (oldSubjectSplit.size() > 1)
  {
    std::string oldPrefix = ToLower(oldSubjectSplit.at(0));
    if (replyPrefixes.find(oldPrefix) != replyPrefixes.end())
    {
      hasFwdPrefix = true;
    }
  }

  return hasFwdPrefix ? p_Str : ("Fwd: " + p_Str);
}

std::string Util::GetHostname()
{
  char hostname[256]; // @todo: use HOST_NAME_MAX?
  gethostname(hostname, sizeof(hostname));
  return std::string(hostname);
}

std::string Util::ToString(const std::wstring& p_WStr)
{
  size_t len = std::wcstombs(nullptr, p_WStr.c_str(), 0);
  if (len != static_cast<std::size_t>(-1))
  {  
    std::vector<char> cstr(len + 1);
    std::wcstombs(&cstr[0], p_WStr.c_str(), len);
    std::string str(&cstr[0], len);
    return str;
  }
  else
  {
    std::string str = std::string(p_WStr.begin(), p_WStr.end());
    return str;
  }
}

std::wstring Util::ToWString(const std::string& p_Str)
{
  size_t len = mbstowcs(nullptr, p_Str.c_str(), 0);
  if (len != static_cast<std::size_t>(-1))
  {
    std::vector<wchar_t> wcstr(len + 1);
    std::mbstowcs(&wcstr[0], p_Str.c_str(), len);
    std::wstring wstr(&wcstr[0], len);
    return wstr;
  }
  else
  {
    std::wstring wstr = std::wstring(p_Str.begin(), p_Str.end());
    return wstr;
  }
}

std::string Util::TrimPadString(const std::string &p_Str, size_t p_Len)
{
  std::string str = p_Str;
  if (str.size() > p_Len)
  {
    str = str.substr(0, p_Len);
  }
  else if (str.size() < p_Len)
  {
    str = str + std::string(p_Len - str.size(), ' ');
  }
  return str;
}

std::wstring Util::TrimPadWString(const std::wstring &p_Str, size_t p_Len)
{
  std::wstring str = p_Str;
  if (str.size() > p_Len)
  {
    str = str.substr(0, p_Len);
  }
  else if (str.size() < p_Len)
  {
    str = str + std::wstring(p_Len - str.size(), ' ');
  }
  return str;
}

std::string Util::ToLower(const std::string &p_Str)
{
  std::string lower = p_Str;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  return lower;
}

std::vector<std::string> Util::Split(const std::string &p_Str, char p_Sep)
{
  std::vector<std::string> vec;
  if (!p_Str.empty())
  {
    std::stringstream ss(p_Str);
    while (ss.good())
    {
      std::string str;
      getline(ss, str, p_Sep);
      vec.push_back(str);
    }
  }
  return vec;
}

std::vector<std::string> Util::SplitQuoted(const std::string& p_Str)
{
  std::vector<std::string> words;
  size_t quoteCnt = 0;
  std::string segment;
  std::stringstream ss(p_Str);

  while (std::getline(ss, segment, '\"'))
  {
    ++quoteCnt;
    if (quoteCnt % 2 == 0)
    {
      segment = Trim(segment);
      if (!segment.empty())
      {
        words.push_back(segment);
      }
    }
    else
    {
      std::stringstream segmentStream(segment);
      while (std::getline(segmentStream, segment, ','))
      {
        segment = Trim(segment);
        if (!segment.empty())
        {
          words.push_back(segment);
        }
      }
    }
  }

  return words;
}

std::string Util::Trim(const std::string &p_Str)
{
  std::string space = " ";
  const auto strBegin = p_Str.find_first_not_of(space);
  if (strBegin == std::string::npos) return "";

  const auto strEnd = p_Str.find_last_not_of(space);
  const auto strRange = strEnd - strBegin + 1;

  return p_Str.substr(strBegin, strRange);
}

std::vector<std::string> Util::Trim(const std::vector<std::string>& p_Strs)
{
  std::vector<std::string> trimStrs;
  for (auto& str : p_Strs)
  {
    trimStrs.push_back(Trim(str));
  }

  return trimStrs;
}

int Util::GetKeyCode(const std::string& p_KeyName)
{
  static std::map<std::string, int> keyCodes =
    {
      // additional keys
      { "KEY_TAB", KEY_TAB},
      { "KEY_RETURN", KEY_RETURN},
      { "KEY_SPACE", KEY_SPACE},

      // ctrl keys
      { "KEY_CTRL@", 0},
      { "KEY_CTRLA", 1},
      { "KEY_CTRLB", 2},
      { "KEY_CTRLC", 3},
      { "KEY_CTRLD", 4},
      { "KEY_CTRLE", 5},
      { "KEY_CTRLF", 6},
      { "KEY_CTRLG", 7},
      { "KEY_CTRLH", 8},
      { "KEY_CTRLI", 9},
      { "KEY_CTRLJ", 10},
      { "KEY_CTRLK", 11},
      { "KEY_CTRLL", 12},
      { "KEY_CTRLM", 13},
      { "KEY_CTRLN", 14},
      { "KEY_CTRLO", 15},
      { "KEY_CTRLP", 16},
      { "KEY_CTRLQ", 17},
      { "KEY_CTRLR", 18},
      { "KEY_CTRLS", 19},
      { "KEY_CTRLT", 20},
      { "KEY_CTRLU", 21},
      { "KEY_CTRLV", 22},
      { "KEY_CTRLW", 23},
      { "KEY_CTRLX", 24},
      { "KEY_CTRLY", 25},
      { "KEY_CTRLZ", 26},
      { "KEY_CTRL[", 27},
      { "KEY_CTRL\\", 28},
      { "KEY_CTRL]", 29},
      { "KEY_CTRL^", 30},
      { "KEY_CTRL_", 31},

      // ncurses keys
      { "KEY_DOWN", KEY_DOWN },
      { "KEY_UP", KEY_UP },
      { "KEY_LEFT", KEY_LEFT },
      { "KEY_RIGHT", KEY_RIGHT },
      { "KEY_HOME", KEY_HOME },
      { "KEY_BACKSPACE", KEY_BACKSPACE },
      { "KEY_F0", KEY_F0 },
      { "KEY_F1", KEY_F(1) },
      { "KEY_F2", KEY_F(2) },
      { "KEY_F3", KEY_F(3) },
      { "KEY_F4", KEY_F(4) },
      { "KEY_F5", KEY_F(5) },
      { "KEY_F6", KEY_F(6) },
      { "KEY_F7", KEY_F(7) },
      { "KEY_F8", KEY_F(8) },
      { "KEY_F9", KEY_F(9) },
      { "KEY_F10", KEY_F(10) },
      { "KEY_F11", KEY_F(11) },
      { "KEY_F12", KEY_F(12) },
      { "KEY_DL", KEY_DL },
      { "KEY_IL", KEY_IL },
      { "KEY_DC", KEY_DC },
      { "KEY_IC", KEY_IC },
      { "KEY_EIC", KEY_EIC },
      { "KEY_CLEAR", KEY_CLEAR },
      { "KEY_EOS", KEY_EOS },
      { "KEY_EOL", KEY_EOL },
      { "KEY_SF", KEY_SF },
      { "KEY_SR", KEY_SR },
      { "KEY_NPAGE", KEY_NPAGE },
      { "KEY_PPAGE", KEY_PPAGE },
      { "KEY_STAB", KEY_STAB },
      { "KEY_CTAB", KEY_CTAB },
      { "KEY_CATAB", KEY_CATAB },
      { "KEY_ENTER", KEY_ENTER },
      { "KEY_PRINT", KEY_PRINT },
      { "KEY_LL", KEY_LL },
      { "KEY_A1", KEY_A1 },
      { "KEY_A3", KEY_A3 },
      { "KEY_B2", KEY_B2 },
      { "KEY_C1", KEY_C1 },
      { "KEY_C3", KEY_C3 },
      { "KEY_BTAB", KEY_BTAB },
      { "KEY_BEG", KEY_BEG },
      { "KEY_CANCEL", KEY_CANCEL },
      { "KEY_CLOSE", KEY_CLOSE },
      { "KEY_COMMAND", KEY_COMMAND },
      { "KEY_COPY", KEY_COPY },
      { "KEY_CREATE", KEY_CREATE },
      { "KEY_END", KEY_END },
      { "KEY_EXIT", KEY_EXIT },
      { "KEY_FIND", KEY_FIND },
      { "KEY_HELP", KEY_HELP },
      { "KEY_MARK", KEY_MARK },
      { "KEY_MESSAGE", KEY_MESSAGE },
      { "KEY_MOVE", KEY_MOVE },
      { "KEY_NEXT", KEY_NEXT },
      { "KEY_OPEN", KEY_OPEN },
      { "KEY_OPTIONS", KEY_OPTIONS },
      { "KEY_PREVIOUS", KEY_PREVIOUS },
      { "KEY_REDO", KEY_REDO },
      { "KEY_REFERENCE", KEY_REFERENCE },
      { "KEY_REFRESH", KEY_REFRESH },
      { "KEY_REPLACE", KEY_REPLACE },
      { "KEY_RESTART", KEY_RESTART },
      { "KEY_RESUME", KEY_RESUME },
      { "KEY_SAVE", KEY_SAVE },
      { "KEY_SBEG", KEY_SBEG },
      { "KEY_SCANCEL", KEY_SCANCEL },
      { "KEY_SCOMMAND", KEY_SCOMMAND },
      { "KEY_SCOPY", KEY_SCOPY },
      { "KEY_SCREATE", KEY_SCREATE },
      { "KEY_SDC", KEY_SDC },
      { "KEY_SDL", KEY_SDL },
      { "KEY_SELECT", KEY_SELECT },
      { "KEY_SEND", KEY_SEND },
      { "KEY_SEOL", KEY_SEOL },
      { "KEY_SEXIT", KEY_SEXIT },
      { "KEY_SFIND", KEY_SFIND },
      { "KEY_SHELP", KEY_SHELP },
      { "KEY_SHOME", KEY_SHOME },
      { "KEY_SIC", KEY_SIC },
      { "KEY_SLEFT", KEY_SLEFT },
      { "KEY_SMESSAGE", KEY_SMESSAGE },
      { "KEY_SMOVE", KEY_SMOVE },
      { "KEY_SNEXT", KEY_SNEXT },
      { "KEY_SOPTIONS", KEY_SOPTIONS },
      { "KEY_SPREVIOUS", KEY_SPREVIOUS },
      { "KEY_SPRINT", KEY_SPRINT },
      { "KEY_SREDO", KEY_SREDO },
      { "KEY_SREPLACE", KEY_SREPLACE },
      { "KEY_SRIGHT", KEY_SRIGHT },
      { "KEY_SRSUME", KEY_SRSUME },
      { "KEY_SSAVE", KEY_SSAVE },
      { "KEY_SSUSPEND", KEY_SSUSPEND },
      { "KEY_SUNDO", KEY_SUNDO },
      { "KEY_SUSPEND", KEY_SUSPEND },
      { "KEY_UNDO", KEY_UNDO },
      { "KEY_MOUSE", KEY_MOUSE },
      { "KEY_RESIZE", KEY_RESIZE },
      { "KEY_EVENT", KEY_EVENT },
    };

  int keyCode = -1;
  std::map<std::string, int>::iterator it = keyCodes.find(p_KeyName);
  if (it != keyCodes.end())
  {
    keyCode = it->second;
  }
  else if ((p_KeyName.size() > 2) && (p_KeyName.substr(0, 2) == "0x"))
  {
    keyCode = strtol(p_KeyName.c_str(), 0, 16);
  }
  else if (p_KeyName.size() == 1)
  {
    keyCode = (int)p_KeyName.at(0);
  }
  else
  {
    LOG_WARNING("warning: unknown key \"%s\"", p_KeyName.c_str());
  }

  return keyCode;
}

std::vector<std::wstring> Util::WordWrap(std::wstring p_Text, unsigned p_LineLength,
                                         bool p_WrapQuoteLines)
{
  int pos = 0;
  int wrapLine = 0;
  int wrapPos = 0;
  return WordWrap(p_Text, p_LineLength, p_WrapQuoteLines, pos, wrapLine, wrapPos);
}

std::vector<std::wstring> Util::WordWrap(std::wstring p_Text, unsigned p_LineLength,
                                         bool p_WrapQuoteLines,
                                         int p_Pos, int& p_WrapLine, int& p_WrapPos)
{
  std::wostringstream wrapped;
  std::vector<std::wstring> lines;

  p_WrapLine = 0;
  p_WrapPos = 0;

  const unsigned wrapLineLength = p_LineLength - 1; // lines with spaces allowed to width - 1
  const unsigned overflowLineLength = p_LineLength; // overflowing/long lines allowed to full width

  {
    std::wstring line;
    std::wistringstream textss(p_Text);
    while (std::getline(textss, line))
    {
      std::wstring linePart = line;
      while (true)
      {
        if ((linePart.size() >= wrapLineLength) &&
            (p_WrapQuoteLines || (linePart.rfind(L">", 0) != 0)))
        {
          size_t spacePos = linePart.rfind(L' ', wrapLineLength);
          if (spacePos != std::wstring::npos)
          {
            lines.push_back(linePart.substr(0, spacePos));
            if (linePart.size() > (spacePos + 1))
            {
              linePart = linePart.substr(spacePos + 1);
            }
            else
            {
              linePart.clear();
            }
          }
          else
          {
            lines.push_back(linePart.substr(0, overflowLineLength));
            if (linePart.size() > overflowLineLength)
            {
              linePart = linePart.substr(overflowLineLength);
            }
            else
            {
              linePart.clear();
            }
          }
        }
        else
        {
          lines.push_back(linePart);
          linePart.clear();
          break;
        }
      }
    }
  }

  for (auto& line : lines)
  {
    if (p_Pos > 0)
    {
      int lineLength = std::min((unsigned)line.size() + 1, overflowLineLength);
      if (lineLength <= p_Pos)
      {
        p_Pos -= lineLength;
        ++p_WrapLine;
      }
      else
      {
        p_WrapPos = p_Pos;
        p_Pos = 0;
      }
    }
  }

  lines.push_back(L"");

  return lines;
}

std::string Util::GetPass()
{
  std::string pass;
  struct termios told, tnew;

  if (tcgetattr(STDIN_FILENO, &told) == 0)
  {
    memcpy(&tnew, &told, sizeof(struct termios));
    tnew.c_lflag &= ~ECHO;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tnew) == 0)
    {
      std::getline(std::cin, pass);
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &told);
      std::cout << std::endl;
    }
  }

  return pass;
}

std::wstring Util::Join(const std::vector<std::wstring>& p_Lines)
{
  std::wstring str;
  bool first = true;
  for (auto& line : p_Lines)
  {
    if (!first)
    {
      str += L"\n";
    }
    else
    {
      first = false;
    }
      
    str += line;
  }
  return str;
}

std::string Util::Join(const std::vector<std::string>& p_Lines, const std::string& p_Delim)
{
  std::string str;
  bool first = true;
  for (auto& line : p_Lines)
  {
    if (!first)
    {
      str += p_Delim;
    }
    else
    {
      first = false;
    }
      
    str += line;
  }
  return str;
}

std::string Util::ToHexString(int p_Val)
{
  std::stringstream stream;
  stream << std::hex << std::uppercase << p_Val;
  return "0x" + stream.str();
}

void Util::DeleteToMatch(std::wstring &p_Str, const int p_StartPos, const wchar_t p_EndChar)
{
  size_t endPos = p_Str.find(p_EndChar, p_StartPos);
  p_Str.erase(p_StartPos, (endPos == std::wstring::npos) ? endPos : (endPos - p_StartPos + 1));
  return;
}

std::string Util::GetAppVersion()
{
#ifdef PROJECT_VERSION
  std::string version = "v" PROJECT_VERSION;
#else
  std::string version = "";
#endif
  return version;
}

std::string Util::GetBuildOs()
{
#if defined(_WIN32)
  return "Windows";
#elif defined(__APPLE__)
  return "macOS";
#elif defined(__linux__)
  return "Linux";
#elif defined(BSD)
  return "BSD";
#else
  return "Unknown OS";
#endif
}

std::string Util::GetCompiler()
{
#if defined(_MSC_VER)
  return "msvc-" + std::to_string(_MSC_VER);
#elif defined(__clang__)
  return "clang-" + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__)
    + "." + std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
  return "gcc-" + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__)
    + "." + std::to_string(__GNUC_PATCHLEVEL__);
#else
  return "Unknown Compiler";
#endif
}

std::string Util::GetSigName(int p_Signal)
{
  static std::map<int, std::string> signames =
  {
    { SIGABRT, "SIGABRT" },
    { SIGSEGV, "SIGSEGV" },
    { SIGBUS,  "SIGBUS" },
    { SIGILL,  "SIGILL" },
    { SIGFPE,  "SIGFPE" },
    { SIGTRAP, "SIGTRAP" },
    { SIGUSR1, "SIGUSR1" },
  };

  auto it = signames.find(p_Signal);
  return (it != signames.end()) ? it->second : std::to_string(p_Signal);
}

void Util::RegisterSignalHandler()
{
  signal(SIGABRT, SignalHandler);
  signal(SIGSEGV, SignalHandler);
  signal(SIGBUS,  SignalHandler);
  signal(SIGILL,  SignalHandler);
  signal(SIGFPE,  SignalHandler);
  signal(SIGTRAP, SignalHandler); 
  signal(SIGUSR1, SignalHandler); 
}

static std::mutex s_SignalMutex;

void Util::SignalHandler(int p_Signal)
{
  const std::string& threadLabel = "\nthread " + ThreadRegister::GetName() + "\n";
  void *callstack[64];
  int size = backtrace(callstack, sizeof(callstack));
  const std::string& callstackStr = BacktraceSymbolsStr(callstack, size);

  if (p_Signal != SIGUSR1)
  {
    {
      std::lock_guard<std::mutex> lock(s_SignalMutex);

      const std::string& logMsg = "unexpected termination: " + GetSigName(p_Signal);
      LOG_ERROR("%s", logMsg.c_str());
      LOG_DUMP(threadLabel.c_str());
      LOG_DUMP(callstackStr.c_str());

      CleanupStdErrRedirect();
      system("reset");
      std::cerr << logMsg << "\n" << callstackStr << "\n";
    }

    if (Log::GetDebugEnabled())
    {
      ThreadRegister::SignalThreads(SIGUSR1);
      sleep(1);
    }

    exit(1);
  }
  else
  {
    std::lock_guard<std::mutex> lock(s_SignalMutex);
    LOG_DUMP(threadLabel.c_str());
    LOG_DUMP(callstackStr.c_str());
  }
}

std::string Util::BacktraceSymbolsStr(void* p_Callstack[], int p_Size)
{
  std::stringstream ss;
  for (int i = 0; i < p_Size; ++i)
  {
    ss << std::left << std::setw(2) << std::setfill(' ') << i << "  ";
    ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << std::right
       << (unsigned long long) p_Callstack[i] << "  ";
    
    Dl_info dlinfo;
    if (dladdr(p_Callstack[i], &dlinfo) && dlinfo.dli_sname)
    {
      if (dlinfo.dli_sname[0] == '_')
      {
        int status = -1;
        char* demangled = NULL;
        demangled = abi::__cxa_demangle(dlinfo.dli_sname, NULL, 0, &status);
        if (demangled && (status == 0))
        {
          ss << demangled;
          free(demangled);
        }
        else
        {
          ss << dlinfo.dli_sname;
        }
      }
      else
      {
        ss << dlinfo.dli_sname;
      }
    }
    ss << "\n";
  }

  return ss.str();
}

bool Util::IsInteger(const std::string& p_Str)
{
  return (p_Str.find_first_not_of("0123456789") == std::string::npos);
}

long Util::ToInteger(const std::string& p_Str)
{
  return strtol(p_Str.c_str(), NULL, 10);
}

std::string Util::ExtensionForMimeType(const std::string& p_MimeType)
{
  static const std::map<std::string, std::string> typeToExt =
  {
    { "image/png", ".png" },
    { "text/html", ".html" },
    { "text/plain", ".txt" },
  };

  auto it = typeToExt.find(p_MimeType);
  if (it != typeToExt.end())
  {
    return it->second;
  }

  return "";
}

void Util::InitStdErrRedirect(const std::string& p_Path)
{
  m_NewStdErr = open(p_Path.c_str(), O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
  if (m_NewStdErr != -1)
  {
    m_OrgStdErr = dup(fileno(stderr));
    dup2(m_NewStdErr, fileno(stderr));
  }
}

void Util::CleanupStdErrRedirect()
{
  if (m_NewStdErr != -1)
  {
    fflush(stderr);
    close(m_NewStdErr);
    dup2(m_OrgStdErr, fileno(stderr));
    close(m_OrgStdErr);
  }
}

void Util::SetEditorCmd(const std::string& p_EditorCmd)
{
  m_EditorCmd = p_EditorCmd;
}

std::string Util::GetEditorCmd()
{
  if (!m_EditorCmd.empty()) return m_EditorCmd;

  return std::string(getenv("EDITOR") ? getenv("EDITOR") : "nano");
}

void Util::SetPagerCmd(const std::string& p_PagerCmd)
{
  m_PagerCmd = p_PagerCmd;
}

std::string Util::GetPagerCmd()
{
  if (!m_PagerCmd.empty()) return m_PagerCmd;

  return std::string(getenv("PAGER") ? getenv("PAGER") : "less");
}

void Util::StripCR(std::wstring& p_Str)
{
  p_Str.erase(std::remove(p_Str.begin(), p_Str.end(), L'\r'), p_Str.end());
}

std::string Util::Strip(const std::string& p_Str, const char p_Char)
{
  std::string str = p_Str;
  str.erase(std::remove(str.begin(), str.end(), p_Char), str.end());
  return str;
}

std::string Util::TrimRight(const std::string& p_Str, const std::string& p_Trim)
{
  const auto strEnd = p_Str.find_last_not_of(p_Trim);

  return p_Str.substr(0, strEnd + 1);
}

std::string Util::RunCommand(const std::string& p_Cmd)
{
  std::string output;
  std::string outPath = Util::GetTempFilename(".txt");
  std::string command = p_Cmd + " 2> /dev/null > " + outPath;
  if (system(command.c_str()) == 0)
  {
    output = Util::ReadFile(outPath);
  }
  else
  {
    LOG_WARNING("external command failed: %s", command.c_str());
  }

  Util::DeleteFile(outPath);
  return output;
}

std::string Util::GetSystemOs()
{
#if defined (__APPLE__)
  const std::string& name = RunCommand("sw_vers -productName | tr -d '\n'");
  const std::string& version = RunCommand("sw_vers -productVersion | tr -d '\n'");
  return name + " " + version;
#elif defined (__linux__)
  return RunCommand("grep PRETTY_NAME /etc/os-release 2> /dev/null | "
                    "cut -d= -f2 | sed -e \"s/\\\"//g\" | tr -d '\n'");
#else
  return "";
#endif
}

std::string Util::GetLinkedLibs(const std::string& p_Prog)
{
#if defined (__APPLE__)
  return RunCommand("otool -L " + p_Prog + " 2> /dev/null | tail -n +2 | awk '{$1=$1};1'");
#elif defined (__linux__)
  return RunCommand("ldd " + p_Prog + " 2> /dev/null | awk '{$1=$1};1'");
#else
  (void)p_Prog;
  return "";
#endif
}

std::string Util::GetSelfPath()
{
#if defined (__APPLE__)
  char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_pidpath(getpid(), pathbuf, sizeof(pathbuf)) > 0)
  {
    return std::string(pathbuf);
  }
#elif defined (__linux__)
  char pathbuf[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", pathbuf, sizeof(pathbuf));
  if (count > 0)
  {
    return std::string(pathbuf, count);
  }
#endif
  return "";
}

std::string Util::GetLibetpanVersion()
{
  std::string version;
#if defined(LIBETPAN_VERSION_MAJOR) && defined(LIBETPAN_VERSION_MINOR)
  version += std::to_string(LIBETPAN_VERSION_MAJOR) + "." + std::to_string(LIBETPAN_VERSION_MINOR);
#ifdef LIBETPAN_API_CURRENT
  version += " API " + std::to_string(LIBETPAN_API_CURRENT);
#endif
#endif
  return version;
}

std::string Util::GetUname()
{
#if defined (__APPLE__) || defined (__linux__)
  return RunCommand("uname -a 2> /dev/null");
#else
  return "";
#endif
}

std::string Util::MimeToUtf8(const std::string &p_Str)
{
  const char* charset = "UTF-8";
  char* cdecoded = NULL;
  size_t curtoken = 0;
  int rv = mailmime_encoded_phrase_parse(charset, p_Str.c_str(), p_Str.size(), &curtoken,
                                         charset, &cdecoded);
  if ((rv == MAILIMF_NO_ERROR) && (cdecoded != NULL))
  {
    std::string decoded(cdecoded);
    free(cdecoded);
    return decoded;
  }
  else
  {
    return p_Str;
  }
}

std::vector<std::string> Util::MimeToUtf8(const std::vector<std::string>& p_Strs)
{
  std::vector<std::string> strs;
  for (auto& str : p_Strs)
  {
    strs.push_back(MimeToUtf8(str));
  }
  return strs;
}

std::string Util::ConvertEncoding(const std::string& p_SrcEnc, const std::string& p_DstEnc,
                                  const std::string& p_SrcStr)
{
  std::string str; 
  char* convStr = NULL;
  size_t convLen = 0;
  if ((charconv_buffer(p_DstEnc.c_str(), p_SrcEnc.c_str(), p_SrcStr.c_str(), p_SrcStr.size(),
                       &convStr, &convLen) == MAIL_CHARCONV_NO_ERROR) && (convStr != NULL))
  {
    str = std::string(convStr, convLen);
    charconv_buffer_free(convStr);
  }
  else
  {
    str = p_SrcStr;
    LOG_ERROR("failed converting %s to %s", p_SrcEnc.c_str(), p_DstEnc.c_str());
  }

  return str;
}

std::string Util::GetSQLiteVersion()
{
  return std::string(SQLITE_VERSION);
}
