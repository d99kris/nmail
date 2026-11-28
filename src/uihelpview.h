// uihelpview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>
#include <vector>

#include "uiviewbase.h"

class UiHelpView : public UiViewBase
{
public:
  explicit UiHelpView(Ui* p_Ui);

  void Draw();
  void DrawHelpText(const std::vector<std::vector<std::string>>& p_HelpText, int p_KeyWidth = 0);
  std::string GetKeyDisplay(int p_Key);
  void ResetMessagesListOffset();
  void NextMessagesListOffset();
  void ResetMessageOffset();
  void NextMessageOffset();

private:
  bool m_HelpEnabled = true;
  int m_HelpViewMessagesListOffset = 0;
  int m_HelpViewMessagesListSize = 0;
  int m_HelpViewMessageOffset = 0;
  int m_HelpViewMessageSize = 0;
};
