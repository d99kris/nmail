// uipickerview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

#include "uiscreenview.h"

class UiPickerView : public UiScreenView
{
public:
  explicit UiPickerView(Ui* p_Ui);

  static void Init();
  virtual void DrawDialog() = 0;

protected:
  void DrawSearchDialog(const std::wstring& p_FilterStr, int p_FilterPos);
  int GetPickerWidth();

protected:
  static double m_PickerWidth;
};
