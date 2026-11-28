// uifolderlistview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>
#include <utility>

#include "uipickerview.h"

class UiFolderListView : public UiPickerView
{
public:
  explicit UiFolderListView(Ui* p_Ui);

  virtual void Draw();
  virtual void KeyHandler(int p_Key);
  virtual void OnEnter();
  virtual void DrawDialog();

private:
  bool m_PersistFolderFilter = true;

  int m_FolderListCurrentIndex = 0;
  std::string m_FolderListCurrentFolder;
  std::pair<std::string, int32_t> m_MoveFolderUid;

  int m_PersistedFolderListCurrentIndex = 0;
  std::string m_PersistedFolderListCurrentFolder;

  int m_FolderListFilterPos = 0;
  std::wstring m_FolderListFilterStr;

  int m_PersistedFolderListFilterPos = 0;
  std::wstring m_PersistedFolderListFilterStr;
};
