// fileutil.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "fileutil.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include <libgen.h>
#include <unistd.h>

#if !defined(__OpenBSD__)
#include <wordexp.h>
#endif

#include <sys/stat.h>

#include "apathy/path.hpp"

#include "apputil.h"
#include "loghelp.h"
#include "mainconfig.h"
#include "strutil.h"

static std::string ConfigDownloadsDir()
{
  const std::string configDir = MainConfig::GetStr("downloads_dir");
  if (!configDir.empty())
  {
    const std::string downloadsDir = FileUtil::ExpandPath(configDir);
    if (FileUtil::IsDir(downloadsDir))
    {
      return downloadsDir + "/";
    }
  }

  return "";
}

bool FileUtil::Exists(const std::string& p_Path)
{
  struct stat sb;
  return (stat(p_Path.c_str(), &sb) == 0);
}

bool FileUtil::NotEmpty(const std::string& p_Path)
{
  struct stat sb;
  return (stat(p_Path.c_str(), &sb) == 0) && (sb.st_size > 0);
}

bool FileUtil::IsReadableFile(const std::string& p_Path)
{
  std::ifstream file(p_Path.c_str());
  return file.good();
}

std::string FileUtil::ReadFile(const std::string& p_Path)
{
  std::ifstream file(p_Path, std::ios::binary);
  std::stringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

void FileUtil::WriteFile(const std::string& p_Path, const std::string& p_Str)
{
  MkDir(DirName(p_Path));
  std::ofstream file(p_Path, std::ios::binary);
  file << p_Str;
}

std::wstring FileUtil::ReadWFile(const std::string& p_Path)
{
  return StrUtil::ToWString(ReadFile(p_Path));
}

void FileUtil::WriteWFile(const std::string& p_Path, const std::wstring& p_WStr)
{
  WriteFile(p_Path, StrUtil::ToString(p_WStr));
}

std::string FileUtil::BaseName(const std::string& p_Path)
{
  char* path = strdup(p_Path.c_str());
  char* bname = basename(path);
  std::string rv(bname);
  free(path);
  return rv;
}

std::string FileUtil::ExpandPath(const std::string& p_Path)
{
  if (p_Path.empty()) return p_Path;

  if ((p_Path.at(0) != '~') && ((p_Path.at(0) != '$'))) return p_Path;

#if defined(__OpenBSD__)
  // OpenBSD lacks wordexp(), so expand leading ~ manually
  const char* home = getenv("HOME");
  if ((p_Path.at(0) != '~') || ((p_Path.size() > 1) && (p_Path.at(1) != '/')) || (home == nullptr)) return p_Path;

  std::string rv = std::string(home) + p_Path.substr(1);
#else
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
#endif
  return rv;
}

std::vector<std::string> FileUtil::SplitPaths(const std::string& p_Str)
{
  std::vector<std::string> expPaths;
  std::vector<std::string> paths = StrUtil::SplitQuoted(p_Str, true);
  for (auto& path : paths)
  {
    expPaths.push_back(ExpandPath(path));
  }
  return expPaths;
}

std::string FileUtil::RemoveFileExt(const std::string& p_Path)
{
  size_t lastPeriod = p_Path.find_last_of(".");
  if (lastPeriod == std::string::npos) return p_Path;

  return p_Path.substr(0, lastPeriod);
}

std::string FileUtil::GetFileExt(const std::string& p_Path)
{
  size_t lastPeriod = p_Path.find_last_of(".");
  if (lastPeriod == std::string::npos) return "";

  return p_Path.substr(lastPeriod);
}

std::string FileUtil::DirName(const std::string& p_Path)
{
  char* buf = strdup(p_Path.c_str());
  std::string rv = std::string(dirname(buf));
  free(buf);
  return rv;
}

std::vector<std::string> FileUtil::ListDir(const std::string& p_Folder)
{
  std::vector<std::string> files;
  const std::vector<apathy::Path>& paths = apathy::Path::listdir(p_Folder);
  for (auto& path : paths)
  {
    files.push_back(path.filename());
  }
  return files;
}

std::set<Fileinfo, FileinfoCompare> FileUtil::ListPaths(const std::string& p_Folder)
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

std::string FileUtil::GetPrefixedSize(ssize_t p_Size)
{
  std::vector<std::string> prefixes({ "B", "KB", "MB", "GB", "TB", "PB" });
  size_t i = 0;
  for (i = 0; (i < prefixes.size()) && (p_Size >= 1024); i++, (p_Size /= 1024))
  {
  }

  return std::to_string(p_Size) + " " + prefixes.at(i);
}

std::string FileUtil::GetCurrentWorkingDir()
{
  return apathy::Path::cwd().absolute().sanitize().string();
}

std::string FileUtil::AbsolutePath(const std::string& p_Path)
{
  return apathy::Path(p_Path).absolute().sanitize().string();
}

void FileUtil::MkDir(const std::string& p_Path)
{
  if (!apathy::Path::makedirs(p_Path))
  {
    LOG_WARNING("mkdir failed: %s", p_Path.c_str());
  }
}

void FileUtil::RmDir(const std::string& p_Path)
{
  if (!p_Path.empty())
  {
    apathy::Path::rmdirs(apathy::Path(p_Path));
  }
}

void FileUtil::Move(const std::string& p_From, const std::string& p_To)
{
  if (!apathy::Path::move(p_From, p_To))
  {
    LOG_WARNING("move failed: %s -> %s", p_From.c_str(), p_To.c_str());
  }
}

std::string FileUtil::GetTempDir()
{
  static std::string tempDir =
    "temp" + (AppUtil::GetReadOnly() ? ("." + std::to_string(getpid())) : "") + "/";
  return AppUtil::GetApplicationDir() + tempDir;
}

void FileUtil::InitTempDir()
{
  FileUtil::RmDir(GetTempDir());
  FileUtil::MkDir(GetTempDir());
}

void FileUtil::CleanupTempDir()
{
  FileUtil::RmDir(GetTempDir());
}

std::string FileUtil::GetAttachmentsTempDir()
{
  return GetTempDir() + std::string("attachments/");
}

void FileUtil::CleanupAttachmentsTempDir()
{
  FileUtil::RmDir(GetAttachmentsTempDir());
}

std::string FileUtil::GetPreviewTempDir()
{
  return GetTempDir() + std::string("preview/");
}

void FileUtil::CleanupPreviewTempDir()
{
  FileUtil::RmDir(GetPreviewTempDir());
}

std::string FileUtil::GetTempFilename(const std::string& p_Suffix)
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
std::string FileUtil::GetTempDirectory()
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

void FileUtil::DeleteFile(const std::string& p_Path)
{
  unlink(p_Path.c_str());
}

std::string FileUtil::ExtensionForMimeType(const std::string& p_MimeType)
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

std::string FileUtil::MimeTypeForExtension(const std::string& p_Ext)
{
  static const std::map<std::string, std::string> extToType =
  {
    { ".eml", "message/rfc822" },
  };

  auto it = extToType.find(p_Ext);
  if (it != extToType.end())
  {
    return it->second;
  }

  return "application/octet-stream";
}

void FileUtil::CopyFile(const std::string& p_SrcPath, const std::string& p_DstPath)
{
  std::ifstream srcFile(p_SrcPath, std::ios::binary);
  std::ofstream dstFile(p_DstPath, std::ios::binary);
  dstFile << srcFile.rdbuf();
}

void FileUtil::CopyFiles(const std::string& p_SrcDir, const std::string& p_DstDir)
{
  const std::vector<std::string>& files = FileUtil::ListDir(p_SrcDir);
  for (const auto& file : files)
  {
    CopyFile(p_SrcDir + "/" + file, p_DstDir + "/" + file);
  }
}

std::string FileUtil::EscapePath(const std::string& p_Str)
{
  std::string text = p_Str;
  StrUtil::ReplaceString(text, "\"", "\\\"");
  if ((text.find(",") != std::string::npos) || (text.find("\"") != std::string::npos))
  {
    text = "\"" + text + "\"";
  }

  return text;
}

std::string FileUtil::SanitizeFilename(const std::string& p_Str)
{
  // basename without control chars, empty if not usable
  std::string name = p_Str;
  const size_t lastSep = name.find_last_of("/\\");
  if (lastSep != std::string::npos)
  {
    name = name.substr(lastSep + 1);
  }

  name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char p_Char)
  {
    return (p_Char < 0x20) || (p_Char == 0x7f);
  }), name.end());

  return ((name == ".") || (name == "..")) ? "" : name;
}

std::string FileUtil::GetDownloadsDir()
{
  // setting is read from main config at first use
  static const std::string downloadsDir = ConfigDownloadsDir();
  return downloadsDir;
}

bool FileUtil::IsDir(const std::string& p_Path)
{
  return apathy::Path(p_Path).is_directory();
}

std::string FileUtil::GetNonExistentPath(const std::string& p_Path)
{
  int suffixIdx = 1;
  std::string path = p_Path;
  while (Exists(path))
  {
    path = p_Path + "." + std::to_string(suffixIdx++);
    if (suffixIdx >= 1000000) return "";
  }

  return path;
}
