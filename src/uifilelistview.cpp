// uifilelistview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uifilelistview.h"

#include <algorithm>

#include "fileutil.h"
#include "strutil.h"
#include "uicomposeview.h"
#include "uiconfig.h"

UiFileListView::UiFileListView(Ui* p_Ui)
  : UiPickerView(p_Ui)
{
  m_PersistFileSelectionDir = UiConfig::GetBool("persist_file_selection_dir");
}

void UiFileListView::Draw()
{
  werase(m_MainWin);

  std::set<Fileinfo, FileinfoCompare> files;

  if (m_FileListFilterStr.empty())
  {
    files = m_Files;
  }
  else
  {
    for (const auto& file : m_Files)
    {
      if (StrUtil::ToFold(file.m_Name).find(StrUtil::ToFold(StrUtil::ToString(m_FileListFilterStr)))
          != std::string::npos)
      {
        files.insert(file);
      }
    }
  }

  int maxWidth = GetPickerWidth();
  std::wstring dirLabel = L"Dir: ";
  int maxDirLen = maxWidth - dirLabel.size();
  std::wstring dirPath = StrUtil::ToWString(m_CurrentDir);
  int dirPathRight = ((int)dirPath.size() < maxDirLen) ? 0 : (dirPath.size() - maxDirLen);
  dirLabel += dirPath.substr(dirPathRight);
  mvwaddnwstr(m_MainWin, 0, 2, dirLabel.c_str(), dirLabel.size());

  int count = files.size();
  if (count > 0)
  {
    m_FileListCurrentIndex = StrUtil::Bound(0, m_FileListCurrentIndex, (int)files.size() - 1);

    int posOffs = 2;
    int itemsMax = m_MainWinHeight - 1 - posOffs;
    int idxOffs = StrUtil::Bound(0, (int)(m_FileListCurrentIndex - ((itemsMax - 1) / 2)),
                                 std::max(0, (int)files.size() - (int)itemsMax));
    int idxMax = idxOffs + std::min(itemsMax, (int)files.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const Fileinfo& fileinfo = *std::next(files.begin(), i);

      int maxNameLen = maxWidth - 2 - 7; // two spaces and (dir) / 1023 KB
      std::wstring name = StrUtil::ToWString(fileinfo.m_Name);
      name = StrUtil::TrimPadWString(name, maxNameLen);

      if (fileinfo.IsDir())
      {
        name += L"    (dir)";
      }
      else
      {
        // max 7 - ex: "1023 KB"
        std::string size = FileUtil::GetPrefixedSize(fileinfo.m_Size);
        size = std::string(7 - size.size(), ' ') + size;
        name += L"  " + StrUtil::ToWString(size);
      }

      const bool isHidden = fileinfo.IsHidden();
      if (isHidden)
      {
        wattron(m_MainWin, m_AttrsHiddenFile);
      }

      if (i == m_FileListCurrentIndex)
      {
        wattron(m_MainWin, m_AttrsHighlightedText);
        m_FileListCurrentFile = fileinfo;
      }

      mvwaddnwstr(m_MainWin, i - idxOffs + posOffs, 2, name.c_str(), name.size());

      if (i == m_FileListCurrentIndex)
      {
        wattroff(m_MainWin, m_AttrsHighlightedText);
      }

      if (isHidden)
      {
        wattroff(m_MainWin, m_AttrsHiddenFile);
      }
    }
  }

  RefreshWin(m_MainWin);
}

void UiFileListView::KeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(m_LastMessageState);
  }
  else if ((p_Key == m_KeyReturn) || (p_Key == m_KeyEnter) ||
           ((p_Key == m_KeyRight) && (m_FileListFilterPos == (int)m_FileListFilterStr.size())))
  {
    if (m_FileListCurrentFile.IsDir())
    {
      m_FileListFilterPos = 0;
      m_FileListFilterStr.clear();
      m_CurrentDir = FileUtil::AbsolutePath(m_CurrentDir + "/" + m_FileListCurrentFile.m_Name);
      m_Files = FileUtil::ListPaths(m_CurrentDir);
      m_FileListCurrentIndex = 0;
      m_FileListCurrentFile.m_Name = "";
    }
    else
    {
      std::string newFilePath = FileUtil::AbsolutePath(m_CurrentDir + "/" + m_FileListCurrentFile.m_Name);
      m_ComposeView->AddAttachmentPath(newFilePath);
      SetState(m_LastMessageState);
    }
  }
  else if ((m_FileListFilterPos == 0) && (p_Key == m_KeyLeft))
  {
    m_FileListFilterPos = 0;
    m_FileListFilterStr.clear();
    std::string lastDirName = FileUtil::BaseName(m_CurrentDir);
    m_CurrentDir = FileUtil::AbsolutePath(m_CurrentDir + "/..");
    m_Files = FileUtil::ListPaths(m_CurrentDir);
    if (lastDirName != "/")
    {
      Fileinfo lastDirFileinfo(lastDirName, -1);
      auto it = m_Files.find(lastDirFileinfo);
      if (it != m_Files.end())
      {
        m_FileListCurrentIndex = std::distance(m_Files.begin(), it);
        m_FileListCurrentFile = *it;
      }
    }
    else
    {
      m_FileListCurrentIndex = 0;
      m_FileListCurrentFile.m_Name = "";
    }
  }
  else if ((m_FileListFilterPos == 0) && ((p_Key == m_KeyBackspace) || (p_Key == m_KeyBackspaceAlt)))
  {
    // backspace with empty filter goes up one directory level
    m_FileListFilterPos = 0;
    m_FileListFilterStr.clear();
    m_CurrentDir = FileUtil::AbsolutePath(m_CurrentDir + "/..");
    m_Files = FileUtil::ListPaths(m_CurrentDir);
    m_FileListCurrentIndex = 0;
    m_FileListCurrentFile.m_Name = "";
  }
  else if (HandleListKey(p_Key, m_FileListCurrentIndex))
  {
    // none
  }
  else if (HandleLineKey(p_Key, m_FileListFilterStr, m_FileListFilterPos))
  {
    // none
  }
  else if (HandleTextKey(p_Key, m_FileListFilterStr, m_FileListFilterPos))
  {
    // none
  }

  DrawAll();
}

void UiFileListView::OnEnter()
{
  curs_set(1);
  m_FileListFilterPos = 0;
  m_FileListFilterStr.clear();
  if (m_CurrentDir.empty() || !m_PersistFileSelectionDir)
  {
    m_CurrentDir = FileUtil::GetCurrentWorkingDir();
  }

  m_Files = FileUtil::ListPaths(m_CurrentDir);
  m_FileListCurrentIndex = 0;
  m_FileListCurrentFile.m_Name = "";
}

void UiFileListView::DrawDialog()
{
  DrawSearchDialog(m_FileListFilterStr, m_FileListFilterPos);
}
