// uifolderlistview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uifolderlistview.h"

#include <algorithm>
#include <climits>

#include "loghelp.h"
#include "strutil.h"
#include "uiconfig.h"
#include "uimessagelistview.h"

UiFolderListView::UiFolderListView(Ui* p_Ui)
  : UiPickerView(p_Ui)
{
  m_PersistFolderFilter = UiConfig::GetBool("persist_folder_filter");
}

void UiFolderListView::Draw()
{
  bool requestFolders = false;
  {
    UiModel::Lock model(m_Model);
    if (!model->m_HasRequestedFolders)
    {
      model->m_HasRequestedFolders = true;
      requestFolders = true;
    }
  }

  if (requestFolders)
  {
    ImapManager::Request request;
    request.m_GetFolders = true;
    LOG_DEBUG("async req folders");
    m_Model->GetImapManager()->AsyncRequest(request);
  }

  werase(m_MainWin);

  std::set<std::string> folders;

  bool hasFolders = false;
  if (m_FolderListFilterStr.empty())
  {
    UiModel::Lock model(m_Model);
    hasFolders = !model->m_Folders.empty();
    folders = model->m_Folders;
  }
  else
  {
    UiModel::Lock model(m_Model);
    hasFolders = !model->m_Folders.empty();
    for (const auto& folder : model->m_Folders)
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
      UiModel::Lock model(m_Model);
      for (const auto& folder : model->m_Folders)
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

void UiFolderListView::KeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(Ui::StateViewMessageList);
  }
  else if ((p_Key == m_KeyReturn) || (p_Key == m_KeyEnter) ||
           ((p_Key == m_KeyRight) && (m_FolderListFilterPos == (int)m_FolderListFilterStr.size())))
  {
    if (m_FolderListCurrentFolder != "")
    {
      if (m_State == Ui::StateGotoFolder)
      {
        SetCurrentFolder(m_FolderListCurrentFolder);
        SetState(Ui::StateViewMessageList);
        m_MessageListView->UpdateIndexFromUid();
      }
      else if (m_State == Ui::StateMoveToFolder)
      {
        const std::string& folder = m_MoveFolderUid.first;
        if (m_FolderListCurrentFolder != folder)
        {
          m_MessageListView->MoveSelectedMessages(m_FolderListCurrentFolder, m_MoveFolderUid);
          SetLastStateOrMessageList();
        }
        else
        {
          SetDialogMessage("Move to same folder ignored");
          m_MessageListView->UpdateUidFromIndex(true /* p_UserTriggered */);
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

      m_MessageListView->ClearSelection();
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

void UiFolderListView::OnEnter()
{
  if (m_State == Ui::StateGotoFolder)
  {
    curs_set(1);
    m_FolderListFilterPos = 0;
    m_FolderListFilterStr.clear();
    m_FolderListCurrentFolder = m_CurrentFolder;
    m_FolderListCurrentIndex = INT_MAX;
  }
  else if (m_State == Ui::StateMoveToFolder)
  {
    curs_set(1);
    // the cursor may be resynced while the list is shown, the message to move shall not change
    m_MoveFolderUid = m_CurrentFolderUid;
    if (m_IsAutoMove)
    {
      m_FolderListFilterPos = 0;
      m_FolderListFilterStr.clear();
      m_FolderListCurrentFolder = m_MessageListView->AutoMoveSelectFolder();
      m_FolderListCurrentIndex = INT_MAX;
    }
    else if (m_PersistFolderFilter)
    {
      m_FolderListFilterPos = m_PersistedFolderListFilterPos;
      m_FolderListFilterStr = m_PersistedFolderListFilterStr;
      if (!m_PersistedFolderListCurrentFolder.empty())
      {
        m_FolderListCurrentFolder = m_PersistedFolderListCurrentFolder;
        m_FolderListCurrentIndex = m_PersistedFolderListCurrentIndex;
      }
      else
      {
        m_FolderListCurrentFolder = m_CurrentFolder;
        m_FolderListCurrentIndex = INT_MAX;
      }
    }
    else
    {
      m_FolderListFilterPos = 0;
      m_FolderListFilterStr.clear();
      m_FolderListCurrentFolder = m_CurrentFolder;
      m_FolderListCurrentIndex = INT_MAX;
    }
  }
}

void UiFolderListView::DrawDialog()
{
  DrawSearchDialog(m_FolderListFilterStr, m_FolderListFilterPos);
}
