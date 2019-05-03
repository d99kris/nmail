// aes.h
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

class AES
{
public:
  static std::string Encrypt(const std::string& p_Plaintext, const std::string& p_Pass);
  static std::string Decrypt(const std::string& p_Ciphertext, const std::string& p_Pass);
};
