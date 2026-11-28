// uifilelistview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <set>
#include <string>

#include "fileutil.h"
#include "uipickerview.h"

class UiFileListView : public UiPickerView
{
public:
  explicit UiFileListView(Ui* p_Ui);

  virtual void Draw();
  virtual void KeyHandler(int p_Key);
  virtual void OnEnter();
  virtual void DrawDialog();

private:
  bool m_PersistFileSelectionDir = true;

  std::string m_CurrentDir;
  std::set<Fileinfo, FileinfoCompare> m_Files;

  int m_FileListCurrentIndex = 0;
  Fileinfo m_FileListCurrentFile;

  int m_FileListFilterPos = 0;
  std::wstring m_FileListFilterStr;
};
