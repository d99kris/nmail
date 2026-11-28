// uiscreenview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include "uiviewbase.h"

class UiScreenView : public UiViewBase
{
public:
  explicit UiScreenView(Ui* p_Ui);

  virtual void Draw() = 0;
  virtual void DrawDialog();
  virtual void KeyHandler(int p_Key) = 0;
  virtual void OnEnter() = 0;
  virtual bool IsDrawnLast();

protected:
  bool m_InvalidInputNotify = true;
};
