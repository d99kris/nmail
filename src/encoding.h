// encoding.h
//
// Copyright (c) 2021-2022 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

class Encoding
{
public:
  static void ConvertToUtf8(const std::string& p_Enc, std::string& p_Str);
  static std::string ImapUtf7ToUtf8(const std::string& p_Src);
  static std::string Utf8ToImapUtf7(const std::string& p_Src);

private:
  static std::string Detect(const std::string& p_Str);
  static bool Convert(const std::string& p_SrcEnc, const std::string& p_DstEnc,
                      const std::string& p_SrcStr, std::string& p_DstStr);
};
