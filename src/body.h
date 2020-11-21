// body.h
//
// Copyright (c) 2019-2020 Kristofer Berggren
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
  std::string m_Filename;
  std::string m_ContentId;
  std::string m_Charset;
};

class Body
{
public:
  void SetData(const std::string& p_Data);
  std::string GetData() const;
  std::string GetTextPlain();
  std::string GetTextHtml();
  std::map<ssize_t, Part> GetParts();

private:
  void Parse();
  void ParseText();
  void ParseHtml();
  void ParseMime(struct mailmime* p_Mime);
  void ParseMimeData(struct mailmime* p_Mime, std::string p_MimeType);
  void RemoveInvalidHeaders();

private:
  std::string m_Data;

  bool m_Parsed = false;
  std::map<ssize_t, Part> m_Parts;
  ssize_t m_TextPlainIndex = -1;
  ssize_t m_TextHtmlIndex = -1;
  std::string m_TextHtml;
  std::string m_TextPlain;
};

std::ostream& operator<<(std::ostream& p_Stream, const Body& p_Body);
