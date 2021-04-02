// encoding.h
//
// Copyright (c) 2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

class Encoding
{
public:
  static void ConvertToUtf8(const std::string& p_Enc, std::string& p_Str);

private:
  static std::string Detect(const std::string& p_Str);
  static bool Convert(const std::string& p_SrcEnc, const std::string& p_DstEnc,
                      const std::string& p_SrcStr, std::string& p_DstStr);
};
