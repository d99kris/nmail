// uimessagelistview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <set>
#include <string>

#include "uiscreenview.h"

class UiMessageListView : public UiScreenView
{
public:
  explicit UiMessageListView(Ui* p_Ui);

  virtual void Draw();
  virtual void KeyHandler(int p_Key);
  virtual void OnEnter();
  void Cleanup();

  void UpdateUidFromIndex(bool p_UserTriggered);
  void UpdateIndexFromUid();
  void SyncCursor();
  void ClearSelection();
  void MoveSelectedMessages(const std::string& p_To, const std::pair<std::string, int32_t>& p_FolderUid);
  std::string AutoMoveSelectFolder();
  bool DeleteMessage();
  void ToggleSeen();

private:
  void DrawMessageList();
  void DrawMessageListSearch();
  void SearchMessageBasedOnCurrent(bool p_Subject);
  void SearchMessage(bool p_IsLocal, const std::string& p_Query = std::string());
  void SortFilterPreUpdate();
  void SortFilterUpdated(bool p_FilterUpdated);
  void DisableSortFilter();
  void ToggleFilter(Ui::SortFilter p_SortFilter);
  bool SetFilter(Ui::SortFilter p_SortFilter);
  void ToggleSort(Ui::SortFilter p_SortFirst, Ui::SortFilter p_SortSecond);
  void ToggleSelected();
  void ToggleSelectAll();
  int GetSelectedCount();
  void DeleteSelectedMessages();
  void ImportMessage();

private:
  std::string m_AttachmentIndicator;
  bool m_AutomoveTrashAllow = false;
  bool m_DeleteWithoutConfirm = false;
  bool m_PersistSearchQuery = false;
  bool m_PersistSelectionOnSortFilterChange = true;
  bool m_PersistSortFilter = true;
  std::string m_UnreadIndicator;
  bool m_SearchShowFolder = false;

  std::map<std::string, bool> m_MessageListUidSet;
  size_t m_MessageListSearchOffset = 0;
  size_t m_MessageListSearchMax = 0;
  std::map<std::string, std::set<uint32_t>> m_SelectedUids;
  bool m_AllSelected = false;
  bool m_IsLocalSearch = true;
};
