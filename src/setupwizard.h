// setupwizard.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

class Config;

class SetupWizard
{
public:
  static bool Run(const std::string& p_Setup);
  static std::string Label(const std::string& p_Name);
  static bool PromptMenu(const std::string& p_Title, const std::vector<std::string>& p_Options, size_t& p_Index,
                         const std::vector<std::string>& p_Notes = std::vector<std::string>());
  static bool PromptYesNo(const std::string& p_Name, bool& p_Value);
  static bool PromptPassword(const std::string& p_Name, std::string& p_Pass);

private:
  static bool ResolveSetupArg(const std::string& p_Setup, std::string& p_ProviderId, std::string& p_Auth);
  static bool SetupOAuth2(const std::string& p_Auth, const std::string& p_Note, const std::string& p_Name);
  static bool PromptServers(const std::string& p_Domain);
  static void ApplyProvider(const std::string& p_ProviderId);
  static std::string GetParam(const std::string& p_ProviderId, const std::string& p_Param);
  static std::string Sentence(const std::string& p_Text);
  static bool Prompt(const std::string& p_Name, std::string& p_Value);
  static bool PromptPort(const std::string& p_Name, std::string& p_Value);
};
