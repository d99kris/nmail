// uipickerview.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uipickerview.h"

#include <algorithm>
#include <sstream>

#include "loghelp.h"
#include "strutil.h"
#include "textwrap.h"
#include "uiconfig.h"

double UiPickerView::m_PickerWidth = 58;

UiPickerView::UiPickerView(Ui* p_Ui)
  : UiScreenView(p_Ui)
{
}

void UiPickerView::Init()
{
  // fraction (0.618 or 61.8%), columns (2 or more), or 0 for classic layout
  std::string pickerWidthStr = StrUtil::Trim(UiConfig::GetStr("picker_width"));
  const bool isPercent = !pickerWidthStr.empty() && (pickerWidthStr.back() == '%');
  if (isPercent)
  {
    pickerWidthStr.pop_back();
  }

  std::istringstream iss(pickerWidthStr);
  iss.imbue(std::locale::classic());
  double pickerWidth = -1;
  iss >> pickerWidth;
  bool isValid = !iss.fail() && iss.eof() && (pickerWidth >= 0);
  if (isValid && isPercent)
  {
    isValid = (pickerWidth > 0) && (pickerWidth <= 100);
    pickerWidth /= 100;
  }
  else if (isValid && (pickerWidth > 1))
  {
    isValid = (pickerWidth >= 2) && (pickerWidth == (int)pickerWidth);
  }

  if (isValid)
  {
    m_PickerWidth = pickerWidth;
  }
  else
  {
    LOG_WARNING("invalid picker_width \"%s\"", UiConfig::GetStr("picker_width").c_str());
  }
}

void UiPickerView::DrawSearchDialog(const std::wstring& p_FilterStr, int p_FilterPos)
{
  werase(m_DialogWin);
  const std::string& dispStr = StrUtil::ToString(p_FilterStr);
  mvwprintw(m_DialogWin, 0, 0, "   Search: %s", dispStr.c_str());

  leaveok(m_DialogWin, false);
  wmove(m_DialogWin, 0, 11 + TextWrap::GetWidth(p_FilterStr.substr(0, p_FilterPos)));
  RefreshWin(m_DialogWin);
  leaveok(m_DialogWin, true);
}

int UiPickerView::GetPickerWidth()
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
