// body.cpp
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "body.h"

#include <libetpan/libetpan.h>

#include "encoding.h"
#include "header.h"
#include "log.h"
#include "loghelp.h"
#include "util.h"

void Body::FromMime(mailmime* p_Mime)
{
  // when using this function regular SetData/GetData cannot be used
  // @todo: consider making it a constructor
  ParseMime(p_Mime, 0);
  m_Parsed = true;
}

void Body::FromHeader(const std::string& p_Data)
{
  if (!m_Parsed)
  {
    struct mailmime* mime = NULL;
    size_t current_index = 0;
    mailmime_parse(p_Data.c_str(), p_Data.size(), &current_index, &mime);

    if (mime != NULL)
    {
      ParseMime(mime, 0);
      mailmime_free(mime);
    }

    m_Parsed = true;
  }
}

void Body::SetData(const std::string& p_Data)
{
  m_Data = p_Data;
  RemoveInvalidHeaders();
}

std::string Body::GetData() const
{
  return m_Data;
}

std::string Body::GetTextPlain()
{
  Parse();

  if (!m_TextPlain.empty())
  {
    return m_TextPlain;
  }
  else
  {
    ParseHtml();
    return m_TextHtml;
  }
}

std::string Body::GetTextHtml()
{
  Parse();

  ParseHtml();
  if (!m_TextHtml.empty())
  {
    return m_TextHtml;
  }
  else
  {
    return m_TextPlain;
  }
}

std::string Body::GetHtml()
{
  Parse();

  if (!m_Html.empty())
  {
    return m_Html;
  }
  else
  {
    return "<pre>" + m_TextPlain + "</pre>";
  }
}

std::map<ssize_t, Part> Body::GetParts()
{
  Parse();
  return m_Parts;
}

bool Body::HasAttachments()
{
  Parse();
  for (const auto& part : m_Parts)
  {
    if (part.second.m_IsAttachment)
    {
      return true;
    }
  }

  return false;
}

bool Body::IsFormatFlowed()
{
  Parse();
  // @todo: consider caching lookup
  if ((m_TextPlainIndex != -1) && m_Parts.count(m_TextPlainIndex))
  {
    const Part& part = m_Parts.at(m_TextPlainIndex);
    return part.m_IsFormatFlowed;
  }

  return false;
}

void Body::Parse()
{
  if (!m_Parsed)
  {
    struct mailmime* mime = NULL;
    size_t current_index = 0;
    mailmime_parse(m_Data.c_str(), m_Data.size(), &current_index, &mime);

    if (mime != NULL)
    {
      ParseMime(mime, 0);
      mailmime_free(mime);
    }

    ParseText();

    m_Parsed = true;
  }
}

void Body::ParseText()
{
  if ((m_TextPlainIndex != -1) && m_Parts.count(m_TextPlainIndex))
  {
    const Part& part = m_Parts.at(m_TextPlainIndex);
    std::string partEnc = part.m_Charset;
    std::string partText = part.m_Data;
    Encoding::ConvertToUtf8(partEnc, partText);
    m_TextPlain = partText;
  }
}

void Body::ParseHtml()
{
  if (!m_HtmlParsed)
  {
    if ((m_TextHtmlIndex != -1) && m_Parts.count(m_TextHtmlIndex))
    {
      const Part& part = m_Parts.at(m_TextHtmlIndex);
      std::string partEnc = part.m_Charset;
      std::string partHtml = part.m_Data;
      m_Html = partHtml;
      Encoding::ConvertToUtf8(partEnc, partHtml);

      // @todo: more elegant removal of meta-tags
      Util::ReplaceString(partHtml, "<meta ", "<beta ");
      Util::ReplaceString(partHtml, "<META ", "<BETA ");

      const std::string& textHtmlPath = Util::GetTempFilename(".html");
      Util::WriteFile(textHtmlPath, partHtml);

      const std::string& cmd = "cat " + textHtmlPath + " | " + Util::GetHtmlToTextConvertCmd();
      m_TextHtml = Util::RunCommand(cmd);

      Util::DeleteFile(textHtmlPath);
    }

    m_HtmlParsed = true;
  }
}

void Body::ParseMime(mailmime* p_Mime, int p_Depth)
{
  struct mailmime_content* content_type = p_Mime->mm_content_type;
  struct mailmime_type* type = content_type->ct_type;
  std::string mimeType;
  switch (type->tp_type)
  {
    case MAILMIME_TYPE_DISCRETE_TYPE:
      switch (type->tp_data.tp_discrete_type->dt_type)
      {
        case MAILMIME_DISCRETE_TYPE_TEXT:
          mimeType = "text";
          break;

        case MAILMIME_DISCRETE_TYPE_IMAGE:
          mimeType = "image";
          break;

        case MAILMIME_DISCRETE_TYPE_AUDIO:
          mimeType = "audio";
          break;

        case MAILMIME_DISCRETE_TYPE_VIDEO:
          mimeType = "video";
          break;

        case MAILMIME_DISCRETE_TYPE_APPLICATION:
          mimeType = "application";
          break;

        case MAILMIME_DISCRETE_TYPE_EXTENSION:
          mimeType = std::string(type->tp_data.tp_discrete_type->dt_extension);
          break;

        default:
          break;
      }
      break;

    case MAILMIME_TYPE_COMPOSITE_TYPE:
      switch (type->tp_data.tp_composite_type->ct_type)
      {
        case MAILMIME_COMPOSITE_TYPE_MESSAGE:
          mimeType = "message";
          break;

        case MAILMIME_COMPOSITE_TYPE_MULTIPART:
          mimeType = "multipart";
          break;

        case MAILMIME_COMPOSITE_TYPE_EXTENSION:
          mimeType = std::string(type->tp_data.tp_composite_type->ct_token);
          break;

        default:
          break;
      }
      break;

    default:
      break;
  }

  if (mimeType.empty())
  {
    mimeType = "application";
  }

  mimeType = mimeType + std::string("/") + std::string(content_type->ct_subtype);
  mimeType = Util::ToLower(mimeType);

  if (Log::GetTraceEnabled())
  {
    std::string mimeEntry = std::string(p_Depth, '|') + "+" + mimeType;
    LOG_TRACE("%s", mimeEntry.c_str());
  }

  switch (p_Mime->mm_type)
  {
    case MAILMIME_SINGLE:
      ParseMimeData(p_Mime, mimeType);
      break;

    case MAILMIME_MULTIPLE:
      for (clistiter* it = clist_begin(p_Mime->mm_data.mm_multipart.mm_mp_list); it != NULL;
           it = clist_next(it))
      {
        ParseMime((struct mailmime*)clist_content(it), p_Depth + 1);
      }
      break;

    case MAILMIME_MESSAGE:
      if (p_Mime->mm_data.mm_message.mm_fields)
      {
        if (p_Mime->mm_data.mm_message.mm_msg_mime != NULL)
        {
          if (p_Depth > 0)
          {
            // show attached/embedded emails as .eml attachments
            if (p_Mime->mm_body != NULL)
            {
              int col = 0;
              MMAPString* mmstr = mmap_string_new(NULL);
              mailmime_data_write_mem(mmstr, &col, p_Mime->mm_body, 1);
              std::string data = std::string(mmstr->str, mmstr->len);
              mmap_string_free(mmstr);

              // use header parser to determine subject of attached email message
              Header header;
              static const std::string serverTime("X-Nmail-ServerTime: 0");
              std::string headerData = serverTime + "\n" + data;
              header.SetData(headerData);
              std::string subject = header.GetSubject();
              if (subject.empty())
              {
                subject = "unnamed";
              }

              const std::string filename = subject + ".eml";

              Part part;
              part.m_Data = data;
              part.m_MimeType = mimeType;
              part.m_Filename = filename;
              part.m_IsAttachment = true;

              m_Parts[m_NumParts++] = part;
            }
            else
            {
              Part part;
              part.m_IsAttachment = true;

              m_Parts[m_NumParts++] = part;
            }
          }
          else
          {
            ParseMime(p_Mime->mm_data.mm_message.mm_msg_mime, p_Depth + 1);
          }
        }
        break;
      }

    default:
      break;
  }
}

void Body::ParseMimeData(mailmime* p_Mime, std::string p_MimeType)
{
  std::string filename;
  std::string contentId;
  std::string charset;
  bool isAttachment = false;
  ParseMimeFields(p_Mime, filename, contentId, charset, isAttachment);

  mailmime_data* data = p_Mime->mm_data.mm_single;
  if (data == NULL)
  {
    Part part;

    part.m_Charset = charset;
    part.m_MimeType = p_MimeType;
    part.m_Filename = Util::MimeToUtf8(filename);
    part.m_ContentId = contentId;
    part.m_IsAttachment = isAttachment;

    m_Parts[m_NumParts++] = part;
    return;
  }

  switch (data->dt_type)
  {
    case MAILMIME_DATA_TEXT:
      {
        size_t index = 0;
        char* parsedStr = NULL;
        size_t parsedLen = 0;
        int rv = mailmime_part_parse(data->dt_data.dt_text.dt_data,
                                     data->dt_data.dt_text.dt_length, &index,
                                     data->dt_encoding, &parsedStr, &parsedLen);

        if (rv == MAILIMF_NO_ERROR)
        {
          Part part;

          if (parsedStr != NULL)
          {
            part.m_Data = std::string(parsedStr, parsedLen);
            mmap_string_unref(parsedStr);
          }

          part.m_Charset = charset;
          part.m_MimeType = p_MimeType;
          part.m_Filename = Util::MimeToUtf8(filename);
          part.m_ContentId = contentId;
          part.m_IsAttachment = isAttachment;

          if ((m_TextPlainIndex == -1) && (p_MimeType == "text/plain"))
          {
            ParseMimeContentType(p_Mime->mm_content_type, part.m_IsFormatFlowed);
            m_TextPlainIndex = m_NumParts;
          }

          if ((m_TextHtmlIndex == -1) && (p_MimeType == "text/html"))
          {
            m_TextHtmlIndex = m_NumParts;
          }

          m_Parts[m_NumParts++] = part;
        }
      }
      break;

    case MAILMIME_DATA_FILE:
      break;

    default:
      break;
  }
}

void Body::ParseMimeFields(mailmime* p_Mime, std::string& p_Filename, std::string& p_ContentId,
                           std::string& p_Charset, bool& p_IsAttachment)
{
  mailmime_single_fields fields;
  memset(&fields, 0, sizeof(mailmime_single_fields));
  if (p_Mime->mm_mime_fields != NULL)
  {
    mailmime_single_fields_init(&fields, p_Mime->mm_mime_fields, p_Mime->mm_content_type);

    if (fields.fld_disposition != NULL)
    {
      struct mailmime_disposition_type* type = fields.fld_disposition->dsp_type;
      if (type != NULL)
      {
        p_IsAttachment = (type->dsp_type == MAILMIME_DISPOSITION_TYPE_ATTACHMENT);
      }
    }

    if (fields.fld_disposition_filename != NULL)
    {
      p_Filename = std::string(fields.fld_disposition_filename);
    }
    else if (fields.fld_content_name != NULL)
    {
      p_Filename = std::string(fields.fld_content_name);
    }

    if (fields.fld_id != NULL)
    {
      p_ContentId = std::string(fields.fld_id);
    }

    if (fields.fld_content_charset != NULL)
    {
      p_Charset = Util::ToLower(std::string(fields.fld_content_charset));
    }
  }
}

void Body::ParseMimeContentType(struct mailmime_content* p_MimeContentType, bool& p_IsFormatFlowed)
{
  if (p_MimeContentType == NULL) return;

  clist* parameters = p_MimeContentType->ct_parameters;
  if (parameters == NULL) return;

  for (clistiter* it = clist_begin(parameters); it != NULL; it = clist_next(it))
  {
    struct mailmime_parameter* mimeParameter = (struct mailmime_parameter*)clist_content(it);
    std::string name(mimeParameter->pa_name);
    std::string value(mimeParameter->pa_value);
    if ((name == "format") && (value == "flowed"))
    {
      p_IsFormatFlowed = true;
    }
  }
}

void Body::RemoveInvalidHeaders()
{
  if (m_Data.find("From ", 0) == 0)
  {
    size_t firstLinefeed = m_Data.find("\n");
    if (firstLinefeed != std::string::npos)
    {
      m_Data.erase(0, firstLinefeed + 1);
    }
  }
}

std::ostream& operator<<(std::ostream& p_Stream, const Body& p_Body)
{
  p_Stream << p_Body.GetData();
  return p_Stream;
}
