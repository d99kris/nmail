// crypto.h
//
// Copyright (c) 2019-2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

class Crypto
{
public:
  static void Init();
  static void Cleanup();
  static std::string GetVersion();

  static std::string AESEncrypt(const std::string& p_Plaintext, const std::string& p_Pass);
  static std::string AESDecrypt(const std::string& p_Ciphertext, const std::string& p_Pass);

  static std::string SHA256(const std::string& p_Str);

  static bool AESEncryptFile(const std::string &p_InPath, const std::string &p_OutPath, const std::string &p_Pass);
  static bool AESDecryptFile(const std::string &p_InPath, const std::string &p_OutPath, const std::string &p_Pass);
};
