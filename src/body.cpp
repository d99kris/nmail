// body.cpp
//
// Copyright (c) 2019-2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "body.h"

#include <libetpan/libetpan.h>

#include "log.h"
#include "loghelp.h"
#include "util.h"

void Body::SetData(const std::string &p_Data)
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

  if ((m_TextPlainIndex != -1) && m_Parts.count(m_TextPlainIndex))
  {
    return m_Parts.at(m_TextPlainIndex).m_Data;
  }
  else
  {
    return "";
  }
}

std::string Body::GetTextHtml()
{
  Parse();

  if ((m_TextHtmlIndex != -1) && m_Parts.count(m_TextHtmlIndex))
  {
    return m_Parts.at(m_TextHtmlIndex).m_Data;
  }
  else
  {
    return "";
  }
}

std::string Body::GetTextFromHtml()
{
  Parse();

  return m_TextFromHtml;
}

std::string Body::GetText()
{
  Parse();

  if (!m_TextFromHtml.empty())
  {
    return m_TextFromHtml;
  }
  else
  {
    return GetTextPlain();
  }
}

std::map<ssize_t, Part> Body::GetParts()
{
  Parse();
  return m_Parts;
}

void Body::Parse()
{
  if (!m_Parsed)
  {
    struct mailmime* mime = NULL;
    size_t current_index = 0;
    LOG_IF_IMAP_ERR(mailmime_parse(m_Data.c_str(), m_Data.size(), &current_index, &mime));

    if (mime != NULL)
    {
      ParseMime(mime);
      mailmime_free(mime);
    }

    ParseHtml();

    m_Parsed = true;
  }
}

void Body::ParseHtml()
{
  if ((m_TextHtmlIndex != -1) && m_Parts.count(m_TextHtmlIndex))
  {
    const std::string& textHtml = m_Parts.at(m_TextHtmlIndex).m_Data;
    const std::string& textHtmlPath = Util::GetTempFilename(".html");
    Util::WriteFile(textHtmlPath, textHtml);

    const std::string& textFromHtmlPath = Util::GetTempFilename(".txt");
    const std::string& command = Util::GetHtmlToTextConvertCmd() + std::string(" ") + textHtmlPath +
        std::string(" 2> /dev/null > ") + textFromHtmlPath;
    int rv = system(command.c_str());
    if (rv == 0)
    {
      m_TextFromHtml = Util::ReadFile(textFromHtmlPath);
      m_TextFromHtml = Util::ReduceIndent(m_TextFromHtml, 3); // @todo: make configurable?
    }

    Util::DeleteFile(textHtmlPath);
    Util::DeleteFile(textFromHtmlPath);
  }
}

void Body::ParseMime(mailmime *p_Mime)
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

  switch (p_Mime->mm_type)
  {
    case MAILMIME_SINGLE:
      ParseMimeData(p_Mime, mimeType);
      break;

    case MAILMIME_MULTIPLE:
      for(clistiter *it = clist_begin(p_Mime->mm_data.mm_multipart.mm_mp_list); it != NULL;
          it = clist_next(it))
      {
        ParseMime((struct mailmime*)clist_content(it));
      }
      break;

    case MAILMIME_MESSAGE:
      if (p_Mime->mm_data.mm_message.mm_fields)
      {
        if (p_Mime->mm_data.mm_message.mm_msg_mime != NULL)
        {
          ParseMime(p_Mime->mm_data.mm_message.mm_msg_mime);
        }
        break;
      }

    default:
      break;
  }
}

void Body::ParseMimeData(mailmime* p_Mime, std::string p_MimeType)
{
  mailmime_data* data = p_Mime->mm_data.mm_single;
  mailmime_single_fields fields;
  std::string filename;
  std::string contentId;
  std::string charset;

  memset(&fields, 0, sizeof(mailmime_single_fields));
  if (p_Mime->mm_mime_fields != NULL)
  {
    mailmime_single_fields_init(&fields, p_Mime->mm_mime_fields, p_Mime->mm_content_type);

    if (fields.fld_disposition_filename != NULL)
    {
      filename = std::string(fields.fld_disposition_filename);
    }
    else if (fields.fld_content_name != NULL)
    {
      filename = std::string(fields.fld_content_name);
    }

    if (fields.fld_id != NULL)
    {
      contentId = std::string(fields.fld_id);
    }
    
    if (fields.fld_content_charset != NULL)
    {
      charset = Util::ToLower(std::string(fields.fld_content_charset));
    }
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

          if (!charset.empty() && (charset != "utf-8"))
          {
            char* convStr = NULL;
            size_t convLen = 0;
            if ((charconv_buffer("utf-8", charset.c_str(), parsedStr, parsedLen, &convStr, &convLen) == MAIL_CHARCONV_NO_ERROR) &&
                (convStr != NULL))
            {
              part.m_Data = std::string(convStr, convLen);
              charconv_buffer_free(convStr);
            }
            else
            {
              LOG_ERROR("cannot convert %s to utf-8", charset.c_str());
            }
          }

          if (parsedStr != NULL)
          {
            if (part.m_Data.empty())
            {
              part.m_Data = std::string(parsedStr, parsedLen);
            }

            mmap_string_unref(parsedStr);
          }

          part.m_MimeType = p_MimeType;
          part.m_Filename = Util::MimeToUtf8(filename);
          part.m_ContentId = contentId;
          m_Parts[index] = part;
          
          if ((m_TextPlainIndex == -1) && (p_MimeType == "text/plain"))
          {
            m_TextPlainIndex = index;
          }

          if ((m_TextHtmlIndex == -1) && (p_MimeType == "text/html"))
          {
            m_TextHtmlIndex = index;
          }
        }
      }
      break;

    case MAILMIME_DATA_FILE:
      break;

    default:
      break;
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
