// util.h
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>
#include <vector>

#ifdef __APPLE__
#define KEY_SYS_BACKSPACE 127
#else
#define KEY_SYS_BACKSPACE KEY_BACKSPACE
#endif

#define KEY_TAB 9
#define KEY_RETURN 10
#define KEY_SPACE 32

class Util
{
public:
  static std::string SHA256(const std::string& p_Str);
  static bool Exists(const std::string& p_Path);
  static std::string ReadFile(const std::string& p_Path);
  static void WriteFile(const std::string& p_Path, const std::string& p_Str);
  static std::wstring ReadWFile(const std::string &p_Path);
  static void WriteWFile(const std::string &p_Path, const std::wstring &p_WStr);
  static std::string BaseName(const std::string& p_Path); 
  static std::string RemoveFileExt(const std::string& p_Path);
  static std::string GetFileExt(const std::string& p_Path);
  static std::string DirName(const std::string& p_Path);
  static std::vector<std::string> ListDir(const std::string& p_Folder);
  static void MkDir(const std::string& p_Path);
  static void RmDir(const std::string& p_Path);
  static void Touch(const std::string& p_Path);
  static std::string GetApplicationDir();
  static void SetApplicationDir(const std::string& p_Path);
  static std::string GetTempDir();
  static void InitTempDir();
  static void CleanupTempDir();
  static std::string GetTempFilename(const std::string& p_Suffix);
  static void DeleteFile(const std::string& p_Path);
  static time_t MailtimeToTimet(struct mailimf_date_time* p_Dt);
  static std::string GetHtmlConvertCmd();
  static void SetHtmlConvertCmd(const std::string& p_HtmlConvertCmd);
  static std::string GetDefaultHtmlConvertCmd();
  static void SetExtViewerCmd(const std::string& p_ExtViewerCmd);
  static std::string GetDefaultExtViewerCmd();
  static void OpenInExtViewer(const std::string& p_Path);
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
  static std::string Trim(const std::string& p_Str);
  static std::vector<std::string> Trim(const std::vector<std::string>& p_Strs);
  static int GetKeyCode(const std::string& p_KeyName);

  static std::vector<std::wstring> WordWrap(std::wstring p_Text, unsigned p_LineLength);
  static std::vector<std::wstring> WordWrap(std::wstring p_Text, unsigned p_LineLength,
                                            int p_Pos, int& p_WrapLine, int& p_WrapPos);
  static std::string GetPass();
  static std::wstring Join(const std::vector<std::wstring>& p_Lines);
  static std::string Join(const std::vector<std::string>& p_Lines,
                          const std::string& p_Delim = "\n");

  static std::string ToHexString(int p_Val);
  static void DeleteToMatch(std::wstring &p_Str, const int p_StartPos, const wchar_t p_EndChar);
  static std::string GetAppVersion();
  static std::string GetOs();
  static std::string GetCompiler();

  static void RegisterSignalHandler();
  static void SignalHandler(int p_Signal);
  static std::string BacktraceSymbolsStr(void* p_Callstack[], int p_Size);

  static bool IsInteger(const std::string& p_Str);
  static long ToInteger(const std::string& p_Str);
  static std::string ExtensionForMimeType(const std::string& p_MimeType);
  static void InitStdErrRedirect(const std::string& p_Path);
  static void CleanupStdErrRedirect();
  static std::string GetEditor();

private:
  static std::string m_HtmlConvertCmd;
  static std::string m_ExtViewerCmd;
  static std::string m_ApplicationDir;
  static int m_OrgStdErr;
  static int m_NewStdErr;
};
