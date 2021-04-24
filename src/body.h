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

struct PartInfo
{
  std::string m_MimeType;
  std::string m_Filename;
  std::string m_ContentId;
  std::string m_Charset;
  size_t m_Size = 0;
  bool m_IsAttachment = false;
  bool m_IsFormatFlowed = false;

  template<class Archive>
  void serialize(Archive& p_Archive)
  {
    p_Archive(m_MimeType,
              m_Filename,
              m_ContentId,
              m_Charset,
              m_Size,
              m_IsAttachment,
              m_IsFormatFlowed);
  }
};

class Body
{
public:
  void FromMime(mailmime* p_Mime);
  void FromHeader(const std::string& p_Data);
  void SetData(const std::string& p_Data);
  std::string GetData() const;
  std::string GetTextPlain() const;
  std::string GetTextHtml() const;
  std::string GetHtml() const;
  std::map<ssize_t, PartInfo> GetPartInfos() const;
  std::map<ssize_t, std::string> GetPartDatas();
  bool HasAttachments() const;
  bool IsFormatFlowed() const;

  inline bool ParseIfNeeded(bool p_ForceParse = false)
  {
    if ((m_ParseVersion == GetCurrentParseVersion()) && !p_ForceParse) return false;

    Parse();
    return true;
  }

  inline bool ParseHtmlIfNeeded()
  {
    if (m_HtmlParsed) return false;

    ParseHtml();
    return true;
  }

  template<class Archive>
  void serialize(Archive& p_Archive)
  {
    p_Archive(m_Data,
              m_ParseVersion,
              m_PartInfos,
              m_NumParts,
              m_TextPlainIndex,
              m_TextHtmlIndex,
              m_TextHtml,
              m_TextPlain,
              m_Html,
              m_HtmlParsed);
  }

private:
  void Parse();
  void ParseText();
  void StoreHtml();
  void ParseHtml();
  void ParseMime(struct mailmime* p_Mime, int p_Depth);
  void ParseMimeData(struct mailmime* p_Mime, std::string p_MimeType);
  void ParseMimeFields(mailmime* p_Mime, std::string& p_Filename, std::string& p_ContentId,
                       std::string& p_Charset, bool& p_IsAttachment);
  void ParseMimeContentType(struct mailmime_content* p_MimeContentType, bool& p_IsFormatFlowed);
  void RemoveInvalidHeaders();

  size_t GetCurrentParseVersion();

private:
  std::string m_Data;

  size_t m_ParseVersion = 0;
  std::map<ssize_t, PartInfo> m_PartInfos;
  size_t m_NumParts = 0;
  ssize_t m_TextPlainIndex = -1;
  ssize_t m_TextHtmlIndex = -1;
  std::string m_TextHtml;
  std::string m_TextPlain;
  std::string m_Html;
  bool m_HtmlParsed = false;

  std::map<ssize_t, std::string> m_PartDatas;
  bool m_PartDatasParsed = false;
};

std::ostream& operator<<(std::ostream& p_Stream, const Body& p_Body);
