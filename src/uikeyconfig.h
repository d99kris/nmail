// uikeyconfig.h
//
// Copyright (c) 2025 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

#include "config.h"

class UiKeyConfig
{
public:
  static void Init(bool p_MapKeys);
  static void Cleanup();
  static std::string GetStr(const std::string& p_Param);
  static int GetKey(const std::string& p_Param);
  static int GetKeyCode(const std::string& p_KeyName);
  static std::string GetKeyName(int p_KeyCode);
  static int GetOffsettedKeyCode(int p_KeyCode, bool p_IsFunctionKey);
  static std::map<std::string, std::string> GetMap();

private:
  static void InitKeyCodes(bool p_MapKeys);
  static int GetOffsettedKeyCode(int p_KeyCode);
  static int GetVirtualKeyCodeFromOct(const std::string& p_KeyOct);
  static int ReserveVirtualKeyCode();
  static int GetFunctionKeyOffset();
  static void DetectConflicts();
  static void MigrateFromUiConfig(const std::string& p_UiConfigPath, const std::string& p_KeyConfigPath);

private:
  static Config m_Config;
  static std::map<std::string, int> m_KeyCodes;
};
