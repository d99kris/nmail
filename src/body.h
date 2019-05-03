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
  std::string GetTextPlain() const;
  std::string GetTextHtml() const;
  std::string GetTextFromHtml() const;
  std::string GetText() const;

private:
  void Parse() const;
  void ParseHtml() const;
  void ParseMime(struct mailmime* p_Mime) const;
  void ParseMimeData(struct mailmime_data* p_Data, std::string p_MimeType) const;
  
private:
  std::string m_Data;

  mutable bool m_Parsed = false;
  mutable std::map<ssize_t,Part> m_Parts;
  mutable ssize_t m_TextPlainIndex = -1;
  mutable ssize_t m_TextHtmlIndex = -1;
  mutable std::string m_TextFromHtml;
};
