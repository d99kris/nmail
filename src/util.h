// util.h
//
// Copyright (c) 2019-2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <csignal>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#define KEY_TAB 9
#define KEY_RETURN 10
#define KEY_SPACE 32
#define KEY_DELETE 127

#define THREAD_REGISTER() ThreadRegister threadRegister(__PRETTY_FUNCTION__)

struct Fileinfo
{
  Fileinfo()
  : m_Name("")
  , m_Size(0)
  {
  }

  Fileinfo(const std::string& p_Name, ssize_t p_Size)
  : m_Name(p_Name)
  , m_Size(p_Size)
  {
  }

  inline bool IsDir() const
  {
    return (m_Size == -1);
  }

  inline bool IsHidden() const
  {
    return m_Name.empty() || ((m_Name.at(0) == '.') && (m_Name != ".."));
  }

  std::string m_Name;
  ssize_t m_Size = 0;
};

struct FileinfoCompare
{
  bool operator() (const Fileinfo& p_Lhs, const Fileinfo& p_Rhs) const
  {
    if (p_Lhs.IsDir() != p_Rhs.IsDir())
    {
      return p_Lhs.IsDir() > p_Rhs.IsDir();
    }
    else if (p_Lhs.IsHidden() != p_Rhs.IsHidden())
    {
      return p_Lhs.IsHidden() < p_Rhs.IsHidden();
    }
    else
    {
      return p_Lhs.m_Name < p_Rhs.m_Name;
    }
  }
};

class ThreadRegister
{
public:
  ThreadRegister(const std::string& p_Name)
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Threads.insert({ pthread_self(), p_Name });
  }

  ~ThreadRegister()
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Threads.erase(pthread_self());
  }

  static std::string GetName()
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Threads.find(pthread_self());
    return (it != m_Threads.end()) ? it->second : "";
  }

  static void SignalThreads(int p_Sig)
  {
    pthread_t self = pthread_self();
    for (auto it = m_Threads.begin(); it != m_Threads.end(); ++it)
    {
      if (it->first != self)
      {
        pthread_kill(it->first, p_Sig);
      }
    }
  }

private:
  static std::mutex m_Mutex;
  static std::map<pthread_t, std::string> m_Threads;
};

class Util
{
public:
  static bool Exists(const std::string& p_Path);
  static bool NotEmpty(const std::string& p_Path);
  static std::string ReadFile(const std::string& p_Path);
  static void WriteFile(const std::string& p_Path, const std::string& p_Str);
  static std::wstring ReadWFile(const std::string &p_Path);
  static void WriteWFile(const std::string &p_Path, const std::wstring &p_WStr);
  static std::string BaseName(const std::string& p_Path);
  static std::string RemoveFileExt(const std::string& p_Path);
  static std::string GetFileExt(const std::string& p_Path);
  static std::string DirName(const std::string& p_Path);
  static std::vector<std::string> ListDir(const std::string& p_Folder);
  static std::set<Fileinfo, FileinfoCompare> ListPaths(const std::string& p_Folder);
  static std::string GetPrefixedSize(ssize_t p_Size);
  static std::string GetCurrentWorkingDir();
  static std::string AbsolutePath(const std::string& p_Path);
  static void MkDir(const std::string& p_Path);
  static void RmDir(const std::string& p_Path);
  static void Touch(const std::string& p_Path);
  static std::string GetApplicationDir();
  static void SetApplicationDir(const std::string& p_Path);
  static std::string GetTempDir();
  static void InitTempDir();
  static void CleanupTempDir();
  static std::string GetAttachmentsTempDir();
  static void CleanupAttachmentsTempDir();
  static std::string GetPreviewTempDir();
  static void CleanupPreviewTempDir();
  static std::string GetTempFilename(const std::string& p_Suffix);
  static std::string GetTempDirectory();
  static void DeleteFile(const std::string& p_Path);
  static time_t MailtimeToTimet(struct mailimf_date_time* p_Dt);
  static std::string GetHtmlToTextConvertCmd();
  static void SetHtmlToTextConvertCmd(const std::string& p_HtmlToTextConvertCmd);
  static std::string GetDefaultHtmlToTextConvertCmd();
  static std::string GetTextToHtmlConvertCmd();
  static void SetTextToHtmlConvertCmd(const std::string& p_TextToHtmlConvertCmd);
  static std::string GetDefaultTextToHtmlConvertCmd();
  static std::string ConvertTextToHtml(const std::string& p_Text);
  static std::string GetExtViewerCmd();
  static void SetExtViewerCmd(const std::string& p_ExtViewerCmd);
  static std::string GetDefaultExtViewerCmd();
  static void ReplaceString(std::string& p_Str, const std::string& p_Search,
                            const std::string& p_Replace);
  static std::string ReduceIndent(const std::string& p_Str, int p_Cnt);
  static std::string AddIndent(const std::string& p_Str, const std::string& p_Indent);
  static std::string MakeReplySubject(const std::string& p_Str);
  static std::string MakeForwardSubject(const std::string& p_Str);
  static std::string GetHostname();
  static std::string ToString(const std::wstring& p_WStr);
  static std::wstring ToWString(const std::string& p_Str);
  static std::string TrimPadString(const std::string& p_Str, size_t p_Len);
  static std::wstring TrimPadWString(const std::wstring& p_Str, size_t p_Len);

  template <typename T>
  static inline T Bound(const T& p_Min, const T& p_Val, const T& p_Max)
  {
    return std::max(p_Min, std::min(p_Val, p_Max));
  }

  static std::string ToLower(const std::string& p_Str);
  static std::vector<std::string> Split(const std::string& p_Str, char p_Sep = ',');
  static std::vector<std::string> SplitQuoted(const std::string& p_Str);
  static std::string Trim(const std::string& p_Str);
  static std::vector<std::string> Trim(const std::vector<std::string>& p_Strs);
  static int GetKeyCode(const std::string& p_KeyName);

  static std::vector<std::wstring> WordWrap(std::wstring p_Text, unsigned p_LineLength,
                                            bool p_WrapQuoteLines);
  static std::vector<std::wstring> WordWrap(std::wstring p_Text, unsigned p_LineLength,
                                            bool p_WrapQuoteLines,
                                            int p_Pos, int& p_WrapLine, int& p_WrapPos);
  static std::string GetPass();
  static std::wstring Join(const std::vector<std::wstring>& p_Lines);
  static std::string Join(const std::vector<std::string>& p_Lines,
                          const std::string& p_Delim = "\n");

  static std::string ToHexString(int p_Val);
  static void DeleteToMatch(std::wstring &p_Str, const int p_StartPos, const wchar_t p_EndChar);
  static std::string GetAppVersion();
  static std::string GetBuildOs();
  static std::string GetCompiler();

  static void RegisterSignalHandler();
  static void SignalHandler(int p_Signal);
  static std::string BacktraceSymbolsStr(void* p_Callstack[], int p_Size);

  static bool IsInteger(const std::string& p_Str);
  static long ToInteger(const std::string& p_Str);
  static std::string ExtensionForMimeType(const std::string& p_MimeType);
  static void InitStdErrRedirect(const std::string& p_Path);
  static void CleanupStdErrRedirect();
  static void SetEditorCmd(const std::string& p_EditorCmd);
  static std::string GetEditorCmd();
  static void SetPagerCmd(const std::string& p_PagerCmd);
  static std::string GetPagerCmd();
  static void StripCR(std::wstring& p_Str);
  static std::string Strip(const std::string& p_Str, const char p_Char);
  static std::string TrimRight(const std::string& p_Str, const std::string& p_Trim);
  static std::string RunCommand(const std::string& p_Cmd);
  static std::string GetSystemOs();
  static std::string GetLinkedLibs(const std::string& p_Prog);
  static std::string GetSelfPath();
  static std::string GetLibetpanVersion();
  static std::string GetUname();
  static std::string GetSigName(int p_Signal);
  static std::string MimeToUtf8(const std::string& p_Str);
  static std::vector<std::string> MimeToUtf8(const std::vector<std::string>& p_Strs);
  static std::string ConvertEncoding(const std::string& p_SrcEnc, const std::string& p_DstEnc,
                                     const std::string& p_SrcStr);
  static std::string GetSQLiteVersion();

private:
  static std::string m_HtmlToTextConvertCmd;
  static std::string m_TextToHtmlConvertCmd;
  static std::string m_ExtViewerCmd;
  static std::string m_ApplicationDir;
  static std::string m_PagerCmd;
  static std::string m_EditorCmd;
  static int m_OrgStdErr;
  static int m_NewStdErr;
};
