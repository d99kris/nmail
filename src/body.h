// body.h
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <string>

struct Part
{
  std::string m_MimeType;
  std::string m_Data;
};

class Body
{
public:
  void SetData(const std::string& p_Data);
  std::string GetData() const;
  std::string GetTextPlain();
  std::string GetTextHtml();
  std::string GetTextFromHtml();
  std::string GetText();

private:
  void Parse();
  void ParseHtml();
  void ParseMime(struct mailmime* p_Mime);
  void ParseMimeData(struct mailmime_data* p_Data, std::string p_MimeType);
  
private:
  std::string m_Data;

  bool m_Parsed = false;
  std::map<ssize_t,Part> m_Parts;
  ssize_t m_TextPlainIndex = -1;
  ssize_t m_TextHtmlIndex = -1;
  std::string m_TextFromHtml;
};
