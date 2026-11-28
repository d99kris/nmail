// uipickerview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

#include <algorithm>

#include "strutil.h"

int Ui::GetPickerWidth()
{
  // picker width within the two-column margins on each side, min 20
  const int maxWidth = m_ScreenWidth - 4;
  int width = maxWidth;
  if ((m_PickerWidth > 0) && (m_PickerWidth <= 1))
  {
    width = (int)((m_PickerWidth * maxWidth) + 0.5);
  }
  else if (m_PickerWidth >= 2)
  {
    width = (int)m_PickerWidth;
  }

  return StrUtil::Bound(std::min(20, maxWidth), width, maxWidth);
}
