// uipartlistview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include "body.h"
#include "uiscreenview.h"

class UiPartListView : public UiScreenView
{
public:
  explicit UiPartListView(Ui* p_Ui);

  virtual void Draw();
  virtual void KeyHandler(int p_Key);
  virtual void OnEnter();

private:
  bool m_ShowEmbeddedImages = true;

  int m_PartListCurrentIndex = 0;
  PartInfo m_PartListCurrentPartInfo;
};
