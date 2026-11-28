// uidialogview.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <chrono>
#include <mutex>
#include <string>

#include "uiviewbase.h"

class UiDialogView : public UiViewBase
{
public:
  explicit UiDialogView(Ui* p_Ui);

  void Draw();
  void SetDialogMessage(const std::string& p_DialogMessage, bool p_Warn = false);
  bool PromptYesNo(const std::string& p_Prompt);
  bool PromptString(const std::string& p_Prompt, const std::string& p_Action,
                    std::string& p_Entry);

private:
  int ReadKeyBlocking();

private:
  bool m_HelpEnabled = true;

  std::mutex m_DialogMutex;
  std::string m_DialogMessage;
  std::chrono::time_point<std::chrono::system_clock> m_DialogMessageTime;

  int m_FilenameEntryStringPos = 0;
  std::wstring m_FilenameEntryString;
};
