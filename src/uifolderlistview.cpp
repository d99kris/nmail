// uifolderlistview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

#include <algorithm>
#include <climits>

#include "loghelp.h"
#include "strutil.h"

void Ui::DrawFolderList()
{
  if (!m_HasRequestedFolders)
  {
    ImapManager::Request request;
    request.m_GetFolders = true;
    LOG_DEBUG("async req folders");
    m_HasRequestedFolders = true;
    m_ImapManager->AsyncRequest(request);
  }

  werase(m_MainWin);

  std::set<std::string> folders;

  bool hasFolders = false;
  if (m_FolderListFilterStr.empty())
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    hasFolders = !m_Folders.empty();
    folders = m_Folders;
  }
  else
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    hasFolders = !m_Folders.empty();
    for (const auto& folder : m_Folders)
    {
      if (StrUtil::ToFold(folder).find(StrUtil::ToFold(StrUtil::ToString(m_FolderListFilterStr)))
          != std::string::npos)
      {
        folders.insert(folder);
      }
    }
  }

  int width = GetPickerWidth();
  if (m_PickerWidth != 0)
  {
    // longest name at 4/5 of width, min 4 padding, unfiltered to keep width stable
    int maxNameWidth = 0;
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      for (const auto& folder : m_Folders)
      {
        maxNameWidth = std::max(maxNameWidth, StrUtil::WStringWidth(StrUtil::ToWString(folder)));
      }
    }

    const int fitWidth = std::max(((maxNameWidth * 5) + 3) / 4, maxNameWidth + 4);
    width = StrUtil::Bound(std::min(14, width), fitWidth, width);
  }

  int count = folders.size();
  if (count > 0)
  {
    if (m_FolderListCurrentIndex == INT_MAX)
    {
      for (int i = 0; i < count; ++i)
      {
        const std::string& folder = *std::next(folders.begin(), i);
        if (folder == m_FolderListCurrentFolder)
        {
          m_FolderListCurrentIndex = i;
        }
      }
    }

    m_FolderListCurrentIndex = StrUtil::Bound(0, m_FolderListCurrentIndex, (int)folders.size() - 1);

    int itemsMax = m_MainWinHeight - 1;
    int idxOffs = StrUtil::Bound(0, (int)(m_FolderListCurrentIndex - ((itemsMax - 1) / 2)),
                                 std::max(0, (int)folders.size() - (int)itemsMax));
    int idxMax = idxOffs + std::min(itemsMax, (int)folders.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const std::string& folder = *std::next(folders.begin(), i);

      if (i == m_FolderListCurrentIndex)
      {
        wattron(m_MainWin, m_AttrsHighlightedText);
        m_FolderListCurrentFolder = folder;
      }

      std::wstring wfolder = StrUtil::TrimEllipsisWString(StrUtil::ToWString(folder), width);
      if (m_PickerWidth != 0)
      {
        wfolder = StrUtil::TrimPadWString(wfolder, width);
      }

      mvwaddnwstr(m_MainWin, i - idxOffs, 2, wfolder.c_str(), wfolder.size());

      if (i == m_FolderListCurrentIndex)
      {
        wattroff(m_MainWin, m_AttrsHighlightedText);
      }
    }
  }
  else
  {
    if (hasFolders)
    {
      m_FolderListCurrentFolder = "";
    }
  }

  RefreshWin(m_MainWin);
}

void Ui::ViewFolderListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(StateViewMessageList);
  }
  else if ((p_Key == m_KeyReturn) || (p_Key == m_KeyEnter) ||
           ((p_Key == m_KeyRight) && (m_FolderListFilterPos == (int)m_FolderListFilterStr.size())))
  {
    if (m_FolderListCurrentFolder != "")
    {
      if (m_State == StateGotoFolder)
      {
        SetCurrentFolder(m_FolderListCurrentFolder);
        SetState(StateViewMessageList);
        UpdateIndexFromUid();
      }
      else if (m_State == StateMoveToFolder)
      {
        const std::string& folder = m_CurrentFolderUid.first;
        if (m_FolderListCurrentFolder != folder)
        {
          MoveSelectedMessages(m_FolderListCurrentFolder);
          SetLastStateOrMessageList();
        }
        else
        {
          SetDialogMessage("Move to same folder ignored");
          UpdateUidFromIndex(true /* p_UserTriggered */);
          SetState(m_LastState);
        }

        if (m_PersistFolderFilter)
        {
          m_PersistedFolderListFilterPos = m_FolderListFilterPos;
          m_PersistedFolderListFilterStr = m_FolderListFilterStr;
          m_PersistedFolderListCurrentFolder = m_FolderListCurrentFolder;
          m_PersistedFolderListCurrentIndex = m_FolderListCurrentIndex;
        }
      }

      ClearSelection();
    }
  }
  else if (HandleListKey(p_Key, m_FolderListCurrentIndex))
  {
    // none
  }
  else if (HandleLineKey(p_Key, m_FolderListFilterStr, m_FolderListFilterPos))
  {
    // none
  }
  else if (HandleTextKey(p_Key, m_FolderListFilterStr, m_FolderListFilterPos))
  {
    // none
  }

  DrawAll();
}
