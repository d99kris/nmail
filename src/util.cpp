// util.cpp
//
// Copyright (c) 2019-2022 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "util.h"

#include <algorithm>
#include <csignal>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <set>

#include <cxxabi.h>
#include <dlfcn.h>
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#include <libgen.h>
#include <termios.h>
#include <unistd.h>
#include <wordexp.h>

#ifdef __APPLE__
#include <libproc.h>
#endif

#ifdef __linux__
#include <linux/limits.h>
#endif

#include <libetpan/libetpan.h>
#include <ncurses.h>
#include <sqlite3.h>

#include "apathy/path.hpp"

#include "loghelp.h"
#include "ui.h"

std::mutex ThreadRegister::m_Mutex;
std::map<pthread_t, std::string> ThreadRegister::m_Threads;

std::string Util::m_HtmlToTextConvertCmd;
std::string Util::m_TextToHtmlConvertCmd;
std::string Util::m_PartsViewerCmd;
std::string Util::m_HtmlViewerCmd;
std::string Util::m_MsgViewerCmd;
std::string Util::m_ApplicationDir;
std::string Util::m_PagerCmd;
std::string Util::m_EditorCmd;
std::string Util::m_DownloadsDir;
int Util::m_OrgStdErr = -1;
int Util::m_NewStdErr = -1;
bool Util::m_UseServerTimestamps = false;
std::string Util::m_FilePickerCmd;
bool Util::m_AddressBookEncrypt = false;
bool Util::m_SendHostname = false;
std::string Util::m_LocalizedSubjectPrefixes;

bool Util::Exists(const std::string& p_Path)
{
  struct stat sb;
  return (stat(p_Path.c_str(), &sb) == 0);
}

bool Util::NotEmpty(const std::string& p_Path)
{
  struct stat sb;
  return (stat(p_Path.c_str(), &sb) == 0) && (sb.st_size > 0);
}

bool Util::IsReadableFile(const std::string& p_Path)
{
  std::ifstream file(p_Path.c_str());
  return file.good();
}

std::string Util::ReadFile(const std::string& p_Path)
{
  std::ifstream file(p_Path, std::ios::binary);
  std::stringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

void Util::WriteFile(const std::string& p_Path, const std::string& p_Str)
{
  MkDir(DirName(p_Path));
  std::ofstream file(p_Path, std::ios::binary);
  file << p_Str;
}

std::wstring Util::ReadWFile(const std::string& p_Path)
{
  return ToWString(ReadFile(p_Path));
}

void Util::WriteWFile(const std::string& p_Path, const std::wstring& p_WStr)
{
  WriteFile(p_Path, ToString(p_WStr));
}

std::string Util::BaseName(const std::string& p_Path)
{
  char* path = strdup(p_Path.c_str());
  char* bname = basename(path);
  std::string rv(bname);
  free(path);
  return rv;
}

std::string Util::ExpandPath(const std::string& p_Path)
{
  if (p_Path.empty()) return p_Path;

  if ((p_Path.at(0) != '~') && ((p_Path.at(0) != '$'))) return p_Path;

  wordexp_t exp;
  std::string rv;
  if ((wordexp(p_Path.c_str(), &exp, WRDE_NOCMD) == 0) && (exp.we_wordc > 0))
  {
    rv = std::string(exp.we_wordv[0]);
    for (size_t i = 1; i < exp.we_wordc; ++i)
    {
      rv += " " + std::string(exp.we_wordv[i]);
    }
    wordfree(&exp);
  }
  else
  {
    rv = p_Path;
  }

  return rv;
}

std::vector<std::string> Util::SplitPaths(const std::string& p_Str)
{
  std::vector<std::string> expPaths;
  std::vector<std::string> paths = SplitQuoted(p_Str, true);
  for (auto& path : paths)
  {
    expPaths.push_back(ExpandPath(path));
  }
  return expPaths;
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

std::string Util::DirName(const std::string& p_Path)
{
  char* buf = strdup(p_Path.c_str());
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

void Util::MkDir(const std::string& p_Path)
{
  apathy::Path::makedirs(p_Path);
}

void Util::RmDir(const std::string& p_Path)
{
  if (!p_Path.empty())
  {
    apathy::Path::rmdirs(apathy::Path(p_Path));
  }
}

void Util::Move(const std::string& p_From, const std::string& p_To)
{
  apathy::Path::move(p_From, p_To);
}

void Util::Touch(const std::string& p_Path)
{
  utimensat(0, p_Path.c_str(), NULL, 0);
}

std::string Util::GetApplicationDir()
{
  return m_ApplicationDir;
}

void Util::SetApplicationDir(const std::string& p_Path)
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

std::string Util::GetTempFilename(const std::string& p_Suffix)
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

void Util::DeleteFile(const std::string& p_Path)
{
  unlink(p_Path.c_str());
}

time_t Util::MailtimeToTimet(mailimf_date_time* p_Dt)
{
  int year = p_Dt->dt_year;
  if (year < 100)
  {
    // assume two-digit year is 20xx
    year += 2000;
  }

  char buf[128];
  snprintf(buf, sizeof(buf), "%04i-%02i-%02i %02i:%02i:%02i",
           year, p_Dt->dt_month, p_Dt->dt_day, p_Dt->dt_hour,
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

void Util::MailimapTimeToMailimfTime(mailimap_date_time* p_Src, mailimf_date_time* p_Dst)
{
  p_Dst->dt_day = p_Src->dt_day;
  p_Dst->dt_month = p_Src->dt_month;
  p_Dst->dt_year = p_Src->dt_year;
  p_Dst->dt_hour = p_Src->dt_hour;
  p_Dst->dt_min = p_Src->dt_min;
  p_Dst->dt_sec = p_Src->dt_sec;
  p_Dst->dt_zone = p_Src->dt_zone;
}

std::string Util::GetHtmlToTextConvertCmd()
{
  if (!m_HtmlToTextConvertCmd.empty()) return m_HtmlToTextConvertCmd;

  static std::string defaultHtmlToTextConvertCmd = GetDefaultHtmlToTextConvertCmd();

  return defaultHtmlToTextConvertCmd;
}

void Util::SetHtmlToTextConvertCmd(const std::string& p_HtmlToTextConvertCmd)
{
  m_HtmlToTextConvertCmd = p_HtmlToTextConvertCmd;
}

std::string Util::GetDefaultHtmlToTextConvertCmd()
{
  std::string result;
  const std::string& commandOutPath = Util::GetTempFilename(".txt");
  const std::string& command =
    std::string("which pandoc w3m lynx elinks 2> /dev/null | head -1 > ") + commandOutPath;
  if (system(command.c_str()) == 0)
  {
    std::string output = Util::ReadFile(commandOutPath);
    output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
    if (!output.empty())
    {
      if (output.find("/pandoc") != std::string::npos)
      {
        result = "pandoc -f html -t plain+literate_haskell --wrap=preserve";
      }
      else if (output.find("/w3m") != std::string::npos)
      {
        result = "w3m -T text/html -I utf-8 -dump";
      }
      else if (output.find("/lynx") != std::string::npos)
      {
        result = "lynx -assume_charset=utf-8 -display_charset=utf-8 -nomargins -dump -stdin";
      }
      else if (output.find("/elinks") != std::string::npos)
      {
        result = "elinks -dump-charset utf-8 -dump";
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
  std::string result;
  const std::string& commandOutPath = Util::GetTempFilename(".txt");
  const std::string& command =
    std::string("which pandoc markdown 2> /dev/null | head -1 > ") + commandOutPath;
  if (system(command.c_str()) == 0)
  {
    std::string output = Util::ReadFile(commandOutPath);
    output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
    if (!output.empty())
    {
      if (output.find("/pandoc") != std::string::npos)
      {
        result = "pandoc -s -f gfm -t html";
      }
      else if (output.find("/markdown") != std::string::npos)
      {
        result = "markdown";
      }
    }
  }

  Util::DeleteFile(commandOutPath);

  return result;
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

std::string Util::GetPartsViewerCmd()
{
  if (!m_PartsViewerCmd.empty()) return m_PartsViewerCmd;

  static std::string defaultPartsViewerCmd = GetDefaultPartsViewerCmd();

  return defaultPartsViewerCmd;
}

void Util::SetPartsViewerCmd(const std::string& p_PartsViewerCmd)
{
  m_PartsViewerCmd = p_PartsViewerCmd;
}

std::string Util::GetDefaultPartsViewerCmd()
{
#if defined(__APPLE__)
  return "open";
#elif defined(__linux__)
  return "xdg-open >/dev/null 2>&1";
#else
  return "";
#endif
}

bool Util::IsDefaultPartsViewerCmd()
{
  return m_PartsViewerCmd.empty();
}

std::string Util::GetHtmlViewerCmd()
{
  if (!m_HtmlViewerCmd.empty()) return m_HtmlViewerCmd;

  static std::string defaultHtmlViewerCmd = GetDefaultHtmlViewerCmd();

  return defaultHtmlViewerCmd;
}

void Util::SetHtmlViewerCmd(const std::string& p_HtmlViewerCmd)
{
  m_HtmlViewerCmd = p_HtmlViewerCmd;
}

std::string Util::GetDefaultHtmlViewerCmd()
{
#if defined(__APPLE__)
  return "open";
#elif defined(__linux__)
  return "xdg-open >/dev/null 2>&1";
#else
  return "";
#endif
}

bool Util::IsDefaultHtmlViewerCmd()
{
  return m_HtmlViewerCmd.empty();
}

std::string Util::GetMsgViewerCmd()
{
  if (!m_MsgViewerCmd.empty()) return m_MsgViewerCmd;

  static std::string defaultMsgViewerCmd = GetDefaultMsgViewerCmd();

  return defaultMsgViewerCmd;
}

void Util::SetMsgViewerCmd(const std::string& p_MsgViewerCmd)
{
  m_MsgViewerCmd = p_MsgViewerCmd;
}

std::string Util::GetDefaultMsgViewerCmd()
{
#if defined(__APPLE__)
  return "open";
#elif defined(__linux__)
  return "xdg-open >/dev/null 2>&1";
#else
  return "";
#endif
}

bool Util::IsDefaultMsgViewerCmd()
{
  return m_MsgViewerCmd.empty();
}

void Util::ReplaceString(std::string& p_Str, const std::string& p_Search,
                         const std::string& p_Replace)
{
  (void)ReplaceStringCount(p_Str, p_Search, p_Replace);
}

size_t Util::ReplaceStringCount(std::string& p_Str, const std::string& p_Search,
                                const std::string& p_Replace)
{
  size_t cnt = 0;
  size_t pos = 0;
  while ((pos = p_Str.find(p_Search, pos)) != std::string::npos)
  {
    p_Str.replace(pos, p_Search.length(), p_Replace);
    pos += p_Replace.length();
    ++cnt;
  }

  return cnt;
}

bool Util::ReplaceStringFirst(std::string& p_Str, const std::string& p_Search,
                              const std::string& p_Replace)
{
  size_t pos = 0;
  if ((pos = p_Str.find(p_Search, pos)) != std::string::npos)
  {
    p_Str.replace(pos, p_Search.length(), p_Replace);
    return true;
  }

  return false;
}

std::string Util::ReduceIndent(const std::string& p_Str, int p_Cnt)
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

std::string Util::AddIndent(const std::string& p_Str, const std::string& p_Indent)
{
  std::string tmpstr = "\n" + p_Str;
  std::string findstr = "\n";
  std::string replacestr = "\n" + p_Indent;
  ReplaceString(tmpstr, findstr, replacestr);

  return tmpstr.substr(1);
}

std::string Util::MakeReplySubject(const std::string& p_Str)
{
  std::string subject = p_Str;
  NormalizeSubject(subject, false /*p_ToLower*/);
  return ("Re: " + subject);
}

std::string Util::MakeForwardSubject(const std::string& p_Str)
{
  std::string subject = p_Str;
  NormalizeSubject(subject, false /*p_ToLower*/);
  return ("Fwd: " + subject);
}

bool Util::GetSendHostname()
{
  return m_SendHostname;
}

void Util::SetSendHostname(bool p_SendHostname)
{
  m_SendHostname = p_SendHostname;
}

std::string Util::ToString(const std::wstring& p_WStr)
{
  try
  {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>{ }.to_bytes(p_WStr);
  }
  catch (...)
  {
    LOG_WARNING("failed to convert from utf-16");
    std::wstring wstr = p_WStr;
    wstr.erase(std::remove_if(wstr.begin(), wstr.end(), [](wchar_t wch) { return !isascii(wch); }), wstr.end());
    std::string str = std::string(wstr.begin(), wstr.end());
    return str;
  }
}

std::wstring Util::ToWString(const std::string& p_Str)
{
  try
  {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>{ }.from_bytes(p_Str);
  }
  catch (...)
  {
    LOG_WARNING("failed to convert from utf-8");
    std::string str = p_Str;
    str.erase(std::remove_if(str.begin(), str.end(), [](unsigned char ch) { return !isascii(ch); }), str.end());
    std::wstring wstr = std::wstring(str.begin(), str.end());
    return wstr;
  }
}

std::string Util::TrimPadString(const std::string& p_Str, int p_Len)
{
  p_Len = std::max(p_Len, 0);
  std::string str = p_Str;
  if ((int)str.size() > p_Len)
  {
    str = str.substr(0, p_Len);
  }
  else if ((int)str.size() < p_Len)
  {
    str = str + std::string(p_Len - str.size(), ' ');
  }
  return str;
}

std::wstring Util::TrimPadWString(const std::wstring& p_Str, int p_Len)
{
  p_Len = std::max(p_Len, 0);
  std::wstring str = p_Str;
  if (WStringWidth(str) > p_Len)
  {
    str = str.substr(0, p_Len);
    int subLen = p_Len;
    while (WStringWidth(str) > p_Len)
    {
      str = str.substr(0, --subLen);
    }
  }
  else if (WStringWidth(str) < p_Len)
  {
    str = str + std::wstring(p_Len - WStringWidth(str), ' ');
  }
  return str;
}

int Util::WStringWidth(const std::wstring& p_WStr)
{
  int width = wcswidth(p_WStr.c_str(), p_WStr.size());
  return (width != -1) ? width : p_WStr.size();
}

std::string Util::ToLower(const std::string& p_Str)
{
  std::string lower = p_Str;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  return lower;
}

std::wstring Util::ToLower(const std::wstring& p_WStr)
{
  std::wstring lower = p_WStr;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  return lower;
}

std::vector<std::string> Util::Split(const std::string& p_Str, char p_Sep)
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

std::vector<std::string> Util::SplitQuoted(const std::string& p_Str, bool p_Unquote)
{
  std::vector<std::string> vec;
  if (!p_Str.empty())
  {
    std::stringstream ss(p_Str);
    while (ss >> std::ws)
    {
      std::string str;
      if (ss.peek() == '"')
      {
        if (p_Unquote)
        {
          ss >> std::quoted(str);
        }
        else
        {
          ss >> str;
        }
        std::string extra;
        std::getline(ss, extra, ',');
        str += extra;
      }
      else
      {
        std::getline(ss, str, ',');
      }

      str = Trim(str);
      if (!str.empty())
      {
        vec.push_back(str);
      }
    }
  }

  return vec;
}

std::string Util::Trim(const std::string& p_Str)
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

int Util::ReserveVirtualKeyCode()
{
  static int keyCode = 0x8000;
  return keyCode++;
}

int Util::GetKeyCode(const std::string& p_KeyName)
{
  static std::map<std::string, int> keyCodes =
  {
    // additional keys
    { "KEY_TAB", KEY_TAB },
    { "KEY_RETURN", KEY_RETURN },
    { "KEY_SPACE", KEY_SPACE },

    // ctrl keys
    { "KEY_CTRL@", 0 },
    { "KEY_CTRLA", 1 },
    { "KEY_CTRLB", 2 },
    { "KEY_CTRLC", 3 },
    { "KEY_CTRLD", 4 },
    { "KEY_CTRLE", 5 },
    { "KEY_CTRLF", 6 },
    { "KEY_CTRLG", 7 },
    { "KEY_CTRLH", 8 },
    { "KEY_CTRLI", 9 },
    { "KEY_CTRLJ", 10 },
    { "KEY_CTRLK", 11 },
    { "KEY_CTRLL", 12 },
    { "KEY_CTRLM", 13 },
    { "KEY_CTRLN", 14 },
    { "KEY_CTRLO", 15 },
    { "KEY_CTRLP", 16 },
    { "KEY_CTRLQ", 17 },
    { "KEY_CTRLR", 18 },
    { "KEY_CTRLS", 19 },
    { "KEY_CTRLT", 20 },
    { "KEY_CTRLU", 21 },
    { "KEY_CTRLV", 22 },
    { "KEY_CTRLW", 23 },
    { "KEY_CTRLX", 24 },
    { "KEY_CTRLY", 25 },
    { "KEY_CTRLZ", 26 },
    { "KEY_CTRL[", 27 },
    { "KEY_CTRL\\", 28 },
    { "KEY_CTRL]", 29 },
    { "KEY_CTRL^", 30 },
    { "KEY_CTRL_", 31 },

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
  };

  int keyCode = -1;
  std::map<std::string, int>::iterator it = keyCodes.find(p_KeyName);
  if (it != keyCodes.end())
  {
    keyCode = it->second;
    LOG_TRACE("map '%s' to code 0x%x", p_KeyName.c_str(), keyCode);
  }
  else if ((p_KeyName.size() > 2) && (p_KeyName.substr(0, 2) == "0x"))
  {
    keyCode = strtol(p_KeyName.c_str(), 0, 16);
    LOG_TRACE("map '%s' to code 0x%x", p_KeyName.c_str(), keyCode);
  }
  else if ((p_KeyName.size() == 1) && isprint((int)p_KeyName.at(0)))
  {
    keyCode = (int)p_KeyName.at(0);
    LOG_TRACE("map '%s' to code 0x%x", p_KeyName.c_str(), keyCode);
  }
  else if ((p_KeyName.size() > 1) && (p_KeyName.substr(0, 1) == "\\"))
  {
    keyCode = ReserveVirtualKeyCode();
    std::string keyStr = Util::FromOctString(p_KeyName);
    define_key(keyStr.c_str(), keyCode);
    LOG_TRACE("map '%s' to code 0x%x", p_KeyName.c_str(), keyCode);
  }
  else
  {
    LOG_WARNING("warning: unknown key \"%s\"", p_KeyName.c_str());
  }

  return keyCode;
}

std::vector<std::wstring> Util::WordWrap(std::wstring p_Text, unsigned p_LineLength,
                                         bool p_ProcessFormatFlowed, bool p_OutputFormatFlowed,
                                         bool p_QuoteWrap, int p_ExpandTabSize)
{
  int pos = 0;
  int wrapLine = 0;
  int wrapPos = 0;
  return WordWrap(p_Text, p_LineLength, p_ProcessFormatFlowed, p_OutputFormatFlowed, p_QuoteWrap, p_ExpandTabSize, pos,
                  wrapLine,
                  wrapPos);
}

std::vector<std::wstring> Util::WordWrap(std::wstring p_Text, unsigned p_LineLength,
                                         bool p_ProcessFormatFlowed, bool p_OutputFormatFlowed,
                                         bool p_QuoteWrap, int p_ExpandTabSize,
                                         int p_Pos, int& p_WrapLine, int& p_WrapPos)
{
  std::wostringstream wrapped;
  std::vector<std::wstring> lines;

  p_WrapLine = 0;
  p_WrapPos = 0;

  const unsigned wrapLineLength = p_LineLength - 1; // lines with spaces allowed to width - 1
  const unsigned overflowLineLength = p_LineLength; // overflowing lines allowed to full width

  if (p_ProcessFormatFlowed)
  {
    bool prevLineFlowed = false;
    std::wstring line;
    std::wstring prevQuotePrefix;
    std::wstring quotePrefix;
    std::wstring prevUnquotedLine;
    std::wstring unquotedLine;
    std::wistringstream textss(p_Text);
    std::wostringstream outss;
    bool reflowUnquoted = true;
    while (std::getline(textss, line))
    {
      line.erase(std::remove(line.begin(), line.end(), L'\r'), line.end());

      if (!GetQuotePrefix(line, quotePrefix, unquotedLine))
      {
        if (reflowUnquoted)
        {
          if ((quotePrefix != prevQuotePrefix) || !prevLineFlowed)
          {
            outss << L"\n" << line;
          }
          else
          {
            if (!prevLineFlowed)
            {
              outss << L" ";
            }
            outss << line;
          }

          size_t unquotedLen = unquotedLine.size();
          prevLineFlowed = ((unquotedLen > 0) && (unquotedLine[unquotedLen - 1] == L' '));
        }
        else
        {
          outss << L"\n" << line;
        }
      }
      else
      {
        quotePrefix.erase(std::remove(quotePrefix.begin(), quotePrefix.end(), L' '), quotePrefix.end());

        if (quotePrefix != prevQuotePrefix)
        {
          outss << L"\n" << quotePrefix << L" " << unquotedLine;
        }
        else
        {
          if (unquotedLine.empty())
          {
            outss << L"\n" << quotePrefix << L" ";
          }
          else
          {
            if (prevUnquotedLine.empty())
            {
              outss << L"\n" << quotePrefix << L" ";
            }
            else
            {
              size_t prevUnquotedLen = prevUnquotedLine.size();
              if (prevUnquotedLine[prevUnquotedLen - 1] != L' ')
              {
                outss << L" ";
              }
            }

            outss << unquotedLine;
          }
        }
      }

      prevQuotePrefix = quotePrefix;
      prevUnquotedLine = unquotedLine;
    }

    p_Text = outss.str().substr(1);
  }

  if (p_ExpandTabSize > 0)
  {
    size_t pos = 0;
    const std::wstring wsearch = L"\t";
    while ((pos = p_Text.find(wsearch, pos)) != std::wstring::npos)
    {
      size_t lineStart = p_Text.rfind(L'\n', pos);
      if (lineStart == std::wstring::npos)
      {
        lineStart = 0;
      }

      const size_t tabColumn = pos - lineStart - 1;
      const int tabSpaces = (p_ExpandTabSize - (tabColumn % p_ExpandTabSize));
      std::wstring replace(tabSpaces, L' ');

      p_Text.replace(pos, wsearch.length(), replace);
      pos += replace.length();
    }
  }

  if (true)
  {
    std::wstring line;
    std::wstring prevQuotePrefix;
    std::wistringstream textss(p_Text);
    const std::wstring flowedSuffix = p_OutputFormatFlowed ? L" " : L"";
    const size_t quotePrefixMaxLen = p_LineLength / 2;
    while (std::getline(textss, line))
    {
      std::wstring linePart = line;

      std::wstring quotePrefix;
      std::wstring tmpLine;
      size_t quotePrefixLen = 0;
      const bool hasQuotePrefix = p_QuoteWrap && GetQuotePrefix(linePart, quotePrefix, tmpLine);
      if (hasQuotePrefix)
      {
        quotePrefix.erase(std::remove(quotePrefix.begin(), quotePrefix.end(), L' '), quotePrefix.end());
        quotePrefix += L' ';
        quotePrefixLen = quotePrefix.size();
        if (quotePrefixLen > quotePrefixMaxLen)
        {
          quotePrefix = quotePrefix.substr(quotePrefixLen - quotePrefixMaxLen);
          quotePrefixLen = quotePrefix.size();
        }

        linePart = quotePrefix + tmpLine;
      }

      while (true)
      {
        std::wstring tmpPrefix;
        if (hasQuotePrefix && !GetQuotePrefix(linePart, tmpPrefix, tmpLine))
        {
          linePart = quotePrefix + linePart;
        }

        if (linePart.size() > wrapLineLength)
        {
          size_t spacePos = linePart.rfind(L' ', wrapLineLength);
          if ((spacePos != std::wstring::npos) && (spacePos > quotePrefixLen))
          {
            lines.push_back(linePart.substr(0, spacePos) + flowedSuffix);
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

std::string Util::FromOctString(const std::string& p_Str)
{
  std::string rv;
  std::vector<std::string> parts = Split(p_Str, '\\');
  for (auto& part : parts)
  {
    if (part.empty()) continue;

    int val = 0;
    std::istringstream(part) >> std::oct >> val;
    rv += (char)val;
  }

  return rv;
}

void Util::HexToRGB(const std::string p_Str, uint32_t& p_R, uint32_t& p_G, uint32_t& p_B)
{
  std::stringstream ss(p_Str);
  int val;
  ss >> std::hex >> val;

  p_R = (val / 0x10000);
  p_G = (val / 0x100) % 0x100;
  p_B = (val % 0x100);
}

void Util::DeleteToMatch(std::wstring& p_Str, const int p_StartPos, const wchar_t p_EndChar)
{
  size_t endPos = p_Str.find(p_EndChar, p_StartPos);
  p_Str.erase(p_StartPos, (endPos == std::wstring::npos) ? endPos : (endPos - p_StartPos + 1));
}

void Util::DeleteToNextMatch(std::wstring& p_Str, int& p_CurPos, const wchar_t p_EndChar)
{
  size_t endPos = p_Str.find(p_EndChar, p_CurPos + 1);
  p_Str.erase(p_CurPos, (endPos == std::wstring::npos) ? endPos : (endPos - p_CurPos));
}

void Util::DeleteToPrevMatch(std::wstring& p_Str, int& p_CurPos, const wchar_t p_EndChar)
{
  size_t startPos = p_Str.rfind(p_EndChar, (p_CurPos > 1) ? (p_CurPos - 2) : p_CurPos);
  startPos = (startPos == std::wstring::npos) ? 0 : startPos + 1;
  p_Str.erase(startPos, (p_CurPos - startPos));
  p_CurPos -= (p_CurPos - startPos);
}

std::map<int, std::string> Util::GetCrashingSignals()
{
  static const std::map<int, std::string> crashingSignals =
  {
    { SIGABRT, "SIGABRT" },
    { SIGBUS, "SIGBUS" },
    { SIGFPE, "SIGFPE" },
    { SIGILL, "SIGILL" },
    { SIGQUIT, "SIGQUIT" },
    { SIGSEGV, "SIGSEGV" },
    { SIGSYS, "SIGSYS" },
    { SIGTRAP, "SIGTRAP" },
    { SIGUSR1, "SIGUSR1" },
  };
  return crashingSignals;
}

std::map<int, std::string> Util::GetTerminatingSignals()
{
  static const std::map<int, std::string> terminatingSignals =
  {
    { SIGALRM, "SIGALRM" },
    { SIGHUP, "SIGHUP" },
    { SIGPROF, "SIGPROF" },
    { SIGTERM, "SIGTERM" },
    { SIGUSR2, "SIGUSR2" },
    { SIGVTALRM, "SIGVTALRM" },
    { SIGXCPU, "SIGXCPU" },
    { SIGXFSZ, "SIGXFSZ" },
  };
  return terminatingSignals;
}

std::map<int, std::string> Util::GetIgnoredSignals()
{
  static const std::map<int, std::string> ignoredSignals =
  {
    { SIGINT, "SIGINT" },
    { SIGPIPE, "SIGPIPE" },
  };
  return ignoredSignals;
}

std::string Util::GetSigName(int p_Signal)
{
  const std::map<int, std::string>& signames = GetTerminatingSignals();
  auto it = signames.find(p_Signal);
  return (it != signames.end()) ? it->second : std::to_string(p_Signal);
}

void Util::RegisterSignalHandlers()
{
  const std::map<int, std::string>& crashingSignals = GetCrashingSignals();
  for (const auto& crashingSignal : crashingSignals)
  {
    signal(crashingSignal.first, SignalCrashHandler);
  }

  const std::map<int, std::string>& terminatingSignals = GetTerminatingSignals();
  for (const auto& terminatingSignal : terminatingSignals)
  {
    signal(terminatingSignal.first, SignalTerminateHandler);
  }
}

void Util::RegisterIgnoredSignalHandlers()
{
  const std::map<int, std::string>& ignoredSignals = GetIgnoredSignals();
  for (const auto& ignoredSignal : ignoredSignals)
  {
    signal(ignoredSignal.first, SIG_IGN);
  }
}

void Util::RestoreIgnoredSignalHandlers()
{
  const std::map<int, std::string>& ignoredSignals = GetIgnoredSignals();
  for (const auto& ignoredSignal : ignoredSignals)
  {
    signal(ignoredSignal.first, SIG_DFL);
  }
}

static std::mutex s_SignalMutex;

void Util::SignalCrashHandler(int p_Signal)
{
  const std::string& threadLabel = "\nthread " + ThreadRegister::GetName() + "\n";
  void* callstack[64];
#ifdef HAVE_EXECINFO_H
  int size = backtrace(callstack, sizeof(callstack));
#else
  int size = 0;
#endif
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
      LOG_IF_NONZERO(system("reset"));
      std::cerr << logMsg << "\n" << callstackStr << "\n";
    }

    if (Log::GetTraceEnabled())
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

void Util::SignalTerminateHandler(int p_Signal)
{
  const std::string& logMsg = "termination requested: " + GetSigName(p_Signal);
  LOG_WARNING("%s", logMsg.c_str());
  Ui::SetRunning(false);
}

std::string Util::BacktraceSymbolsStr(void* p_Callstack[], int p_Size)
{
  std::stringstream ss;
  for (int i = 0; i < p_Size; ++i)
  {
    ss << std::left << std::setw(2) << std::setfill(' ') << i << "  ";
    ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << std::right
       << (unsigned long long)p_Callstack[i] << "  ";

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

std::string Util::TrimLeft(const std::string& p_Str, const std::string& p_Trim)
{
  std::string str = p_Str;
  str.erase(0, str.find_first_not_of(p_Trim));
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
    DetectCommandNotPresent(command);
  }

  Util::DeleteFile(outPath);
  return output;
}

void Util::DetectCommandNotPresent(const std::string& p_Cmd)
{
  std::vector<std::string> cmdVec = Split(p_Cmd, ' ');
  const std::string programName = (cmdVec.size() > 0) ? cmdVec.at(0) : "";
  const std::string whichCmd = "which " + programName + " > /dev/null 2>&1";
  if (system(whichCmd.c_str()) != 0)
  {
    LOG_WARNING("program not found, please ensure '%s' is installed", programName.c_str());
  }
}

std::string Util::GetSystemOs()
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

std::string Util::GetLinkedLibs(const std::string& p_Prog)
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

std::string Util::GetSelfPath()
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
#if defined(__APPLE__) || defined(__linux__)
  return RunCommand("uname -a 2> /dev/null");
#else
  return "";
#endif
}

std::string Util::MimeToUtf8(const std::string& p_Str)
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
    Util::ReplaceString(decoded, "\r", "");
    Util::ReplaceString(decoded, "\n", "");
    return decoded;
  }
  else
  {
    return p_Str;
  }
}

std::string Util::GetSQLiteVersion()
{
  return std::string(SQLITE_VERSION);
}

int Util::GetColor(const std::string& p_Str)
{
  static const std::map<std::string, int> standardColors = []()
  {
    std::map<std::string, int> colors;
    const std::map<std::string, int> basicColors =
    {
      { "black", COLOR_BLACK },
      { "red", COLOR_RED },
      { "green", COLOR_GREEN },
      { "yellow", COLOR_YELLOW },
      { "blue", COLOR_BLUE },
      { "magenta", COLOR_MAGENTA },
      { "cyan", COLOR_CYAN },
      { "white", COLOR_WHITE },
    };
    colors.insert(basicColors.begin(), basicColors.end());

    if (COLORS > 8)
    {
      const int BRIGHT = 8;
      const std::map<std::string, int> extendedColors =
      {
        { "gray", BRIGHT | COLOR_BLACK },
        { "bright_black", BRIGHT | COLOR_BLACK },
        { "bright_red", BRIGHT | COLOR_RED },
        { "bright_green", BRIGHT | COLOR_GREEN },
        { "bright_yellow", BRIGHT | COLOR_YELLOW },
        { "bright_blue", BRIGHT | COLOR_BLUE },
        { "bright_magenta", BRIGHT | COLOR_MAGENTA },
        { "bright_cyan", BRIGHT | COLOR_CYAN },
        { "bright_white", BRIGHT | COLOR_WHITE },
      };
      colors.insert(extendedColors.begin(), extendedColors.end());
    }

    return colors;
  }();

  if (p_Str.empty() || (p_Str == "normal")) return -1;

  // hex
  if ((p_Str.size() == 8) && (p_Str.substr(0, 2) == "0x"))
  {
    if (!can_change_color())
    {
      LOG_WARNING("terminal cannot set custom hex colors, skipping \"%s\"", p_Str.c_str());
      return -1;
    }

    uint32_t r = 0, g = 0, b = 0;
    Util::HexToRGB(p_Str, r, g, b);
    if ((r <= 255) && (g <= 255) && (b <= 255))
    {
      static int colorId = 31;
      colorId++;
      if (colorId > COLORS)
      {
        LOG_WARNING("max number of colors (%d) already defined, skipping \"%s\"", p_Str.c_str());
        return -1;
      }

      init_color(colorId, ((r * 1000) / 255), ((g * 1000) / 255), ((b * 1000) / 255));
      return colorId;
    }

    LOG_WARNING("invalid color hex code \"%s\"", p_Str.c_str());
    return -1;
  }

  // name
  std::map<std::string, int>::const_iterator standardColor = standardColors.find(p_Str);
  if (standardColor != standardColors.end())
  {
    return standardColor->second;
  }

  // id
  if (Util::IsInteger(p_Str))
  {
    int32_t id = Util::ToInteger(p_Str);
    return id;
  }

  if (p_Str == "reverse")
  {
    LOG_WARNING("both fg and bg must be set to \"reverse\"", p_Str.c_str());
    return -1;
  }

  LOG_WARNING("unsupported color string \"%s\"", p_Str.c_str());
  return -1;
}

int Util::GetColorAttrs(const std::string& p_FgStr, const std::string& p_BgStr)
{
  if (p_FgStr.empty() && p_BgStr.empty()) return A_NORMAL;

  if ((p_FgStr == "normal") && (p_BgStr == "normal")) return A_NORMAL;

  if ((p_FgStr == "reverse") && (p_BgStr == "reverse")) return A_REVERSE;

  const int fgColor = GetColor(p_FgStr);
  const int bgColor = GetColor(p_BgStr);
  if ((fgColor == -1) && (bgColor == -1)) return A_NORMAL;

  static int colorPairId = 0;
  colorPairId++;
  init_pair(colorPairId, fgColor, bgColor);
  return COLOR_PAIR(colorPairId);
}

void Util::SetUseServerTimestamps(bool p_Enable)
{
  m_UseServerTimestamps = p_Enable;
}

bool Util::GetUseServerTimestamps()
{
  return m_UseServerTimestamps;
}

void Util::CopyFiles(const std::string& p_SrcDir, const std::string& p_DstDir)
{
  const std::vector<std::string>& files = Util::ListDir(p_SrcDir);
  for (const auto& file : files)
  {
    std::ifstream srcFile(p_SrcDir + "/" + file, std::ios::binary);
    std::ofstream dstFile(p_DstDir + "/" + file, std::ios::binary);
    dstFile << srcFile.rdbuf();
  }
}

void Util::BitInvertString(std::string& p_String)
{
  for (auto& ch : p_String)
  {
    ch = ~ch;
  }
}

void Util::NormalizeName(std::string& p_String)
{
  std::transform(p_String.begin(), p_String.end(), p_String.begin(), ::tolower);
}

void Util::NormalizeSubject(std::string& p_String, bool p_ToLower)
{
  static const std::regex re = [&]()
  {
    std::vector<std::string> prefixes = { "re", "fwd?" };
    std::vector<std::string> customPrefixes = Split(m_LocalizedSubjectPrefixes, ',');
    prefixes.insert(prefixes.end(), customPrefixes.begin(), customPrefixes.end());
    std::string prefixesJoined = Join(prefixes, "|");
    return std::regex("^((" + prefixesJoined + ") *(:) *)+", std::regex_constants::icase);
  }();

  p_String = std::regex_replace(p_String, re, "");
  if (p_ToLower)
  {
    std::transform(p_String.begin(), p_String.end(), p_String.begin(), ::tolower);
  }
}

void Util::SetLocalizedSubjectPrefixes(const std::string& p_Prefixes)
{
  m_LocalizedSubjectPrefixes = p_Prefixes;
}

std::string Util::ZeroPad(uint32_t p_Num, int32_t p_Len)
{
  std::string str = std::to_string(p_Num);
  int32_t zeroCount = std::max(0, p_Len - (int)str.length());
  str = std::string(zeroCount, '0') + str;
  return str;
}

bool Util::GetQuotePrefix(const std::wstring& p_String, std::wstring& p_Prefix, std::wstring& p_Line)
{
  std::wsmatch sm;
  std::wregex re(L"^(( *> *)+)");
  if (std::regex_search(p_String, sm, re))
  {
    p_Prefix = sm.str();
    p_Line = sm.suffix();
    return true;
  }
  else
  {
    p_Prefix.clear();
    p_Line = p_String;
    return false;
  }
}

std::string Util::ToHex(const std::string& p_String)
{
  std::ostringstream oss;
  for (const char& ch : p_String)
  {
    char buf[3] = { 0 };
    snprintf(buf, sizeof(buf), "%02X", (unsigned char)ch);
    oss << buf;
  }

  return oss.str();
}

std::string Util::FromHex(const std::string& p_String)
{
  std::string result;
  std::istringstream iss(p_String);
  char buf[3] = { 0 };
  while (iss.read(buf, 2))
  {
    result += static_cast<char>(strtol(buf, NULL, 16) & 0xff);
  }

  return result;
}

void Util::SetFilePickerCmd(const std::string& p_FilePickerCmd)
{
  m_FilePickerCmd = p_FilePickerCmd;
}

std::string Util::GetFilePickerCmd()
{
  return m_FilePickerCmd;
}

void Util::SetAddressBookEncrypt(bool p_AddressBookEncrypt)
{
  m_AddressBookEncrypt = p_AddressBookEncrypt;
}

bool Util::GetAddressBookEncrypt()
{
  return m_AddressBookEncrypt;
}

std::string Util::EscapePath(const std::string& p_Str)
{
  std::string text = p_Str;
  ReplaceString(text, "\"", "\\\"");
  if ((text.find(",") != std::string::npos) || (text.find("\"") != std::string::npos))
  {
    text = "\"" + text + "\"";
  }

  return text;
}

std::vector<std::string> Util::SplitAddrs(const std::string& p_Str)
{
  std::vector<std::string> vec = SplitQuoted(p_Str, false);
  return vec;
}

std::vector<std::string> Util::SplitAddrsUnquote(const std::string& p_Str)
{
  std::vector<std::string> vec = SplitQuoted(p_Str, true);
  return vec;
}

std::string Util::EscapeName(const std::string& p_Str)
{
  if (p_Str.empty()) return p_Str;

  if (p_Str.at(0) == '"') return p_Str;

  std::string text = p_Str;
  if ((text.find(",") != std::string::npos) || (text.find("\"") != std::string::npos))
  {
    ReplaceString(text, "\"", "\\\"");
    text = "\"" + text + "\"";
  }

  return text;
}

void Util::RemoveChar(std::string& p_Str, char p_Char)
{
  p_Str.erase(std::remove(p_Str.begin(), p_Str.end(), p_Char), p_Str.end());
}

std::string Util::GetDomainName(const std::string& p_HostAddress)
{
  char key = '.';
  std::size_t lastDot = p_HostAddress.rfind(key);
  if ((lastDot != std::string::npos) && (lastDot > 0))
  {
    std::size_t secondLastDot = p_HostAddress.rfind(key, lastDot - 1);
    if (secondLastDot != std::string::npos)
    {
      return p_HostAddress.substr(secondLastDot + 1);
    }
  }

  return p_HostAddress;
}

std::string Util::GetDownloadsDir()
{
  return m_DownloadsDir;
}

void Util::SetDownloadsDir(const std::string& p_DownloadsDir)
{
  if (!p_DownloadsDir.empty())
  {
    std::string downloadsDir = Util::ExpandPath(p_DownloadsDir);
    if (Util::IsDir(downloadsDir))
    {
      m_DownloadsDir = downloadsDir + "/";
    }
  }
}

bool Util::IsDir(const std::string& p_Path)
{
  return apathy::Path(p_Path).is_directory();
}
