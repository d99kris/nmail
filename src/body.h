// body.h
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <string>

#include <libetpan/mailmime_types.h>

struct Part
{
  std::string m_MimeType;
  std::string m_Data;
  std::string m_Filename;
  std::string m_ContentId;
  std::string m_Charset;
  bool m_IsAttachment = false;
};

class Body
{
public:
  void FromMime(mailmime* p_Mime);
  void FromHeader(const std::string& p_Data);
  void SetData(const std::string& p_Data);
  std::string GetData() const;
  std::string GetTextPlain();
  std::string GetTextHtml();
  std::string GetHtml();
  std::map<ssize_t, Part> GetParts();
  bool HasAttachments();

private:
  void Parse();
  void ParseText();
  void ParseHtml();
  void ParseMime(struct mailmime* p_Mime, int p_Depth);
  void ParseMimeData(struct mailmime* p_Mime, std::string p_MimeType);
  void ParseMimeFields(mailmime* p_Mime, std::string& p_Filename, std::string& p_ContentId,
                       std::string& p_Charset, bool& p_IsAttachment);
  void RemoveInvalidHeaders();

private:
  std::string m_Data;

  bool m_Parsed = false;
  std::map<ssize_t, Part> m_Parts;
  size_t m_NumParts = 0;
  ssize_t m_TextPlainIndex = -1;
  ssize_t m_TextHtmlIndex = -1;
  std::string m_TextHtml;
  std::string m_TextPlain;
  std::string m_Html;
};

std::ostream& operator<<(std::ostream& p_Stream, const Body& p_Body);
