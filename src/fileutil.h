// fileutil.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <set>
#include <string>
#include <vector>

#include <sys/types.h>

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
  bool operator()(const Fileinfo& p_Lhs, const Fileinfo& p_Rhs) const
  {
    if (p_Lhs.IsHidden() != p_Rhs.IsHidden())
    {
      return p_Lhs.IsHidden() < p_Rhs.IsHidden();
    }
    else if (p_Lhs.IsDir() != p_Rhs.IsDir())
    {
      return p_Lhs.IsDir() > p_Rhs.IsDir();
    }
    else
    {
      return p_Lhs.m_Name < p_Rhs.m_Name;
    }
  }
};

class FileUtil
{
public:
  static bool Exists(const std::string& p_Path);
  static bool NotEmpty(const std::string& p_Path);
  static bool IsReadableFile(const std::string& p_Path);
  static std::string ReadFile(const std::string& p_Path);
  static void WriteFile(const std::string& p_Path, const std::string& p_Str);
  static std::wstring ReadWFile(const std::string& p_Path);
  static void WriteWFile(const std::string& p_Path, const std::wstring& p_WStr);
  static std::string BaseName(const std::string& p_Path);
  static std::string ExpandPath(const std::string& p_Path);
  static std::vector<std::string> SplitPaths(const std::string& p_Str);
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
  static void Move(const std::string& p_From, const std::string& p_To);
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
  static std::string ExtensionForMimeType(const std::string& p_MimeType);
  static std::string MimeTypeForExtension(const std::string& p_Ext);
  static void CopyFile(const std::string& p_SrcPath, const std::string& p_DstPath);
  static void CopyFiles(const std::string& p_SrcDir, const std::string& p_DstDir);
  static std::string EscapePath(const std::string& p_Str);
  static std::string SanitizeFilename(const std::string& p_Str);
  static std::string GetDownloadsDir();
  static bool IsDir(const std::string& p_Path);
  static std::string GetNonExistentPath(const std::string& p_Path);

private:
};
