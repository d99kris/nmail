// body.cpp
//
// Copyright (c) 2019 Kristofer Berggren
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
}

std::string Body::GetData() const
{
  return m_Data;
}

std::string Body::GetTextPlain() const
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

std::string Body::GetTextHtml() const
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

std::string Body::GetTextFromHtml() const
{
  Parse();

  return m_TextFromHtml;
}

std::string Body::GetText() const
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

void Body::Parse() const
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

void Body::ParseHtml() const
{
  if ((m_TextHtmlIndex != -1) && m_Parts.count(m_TextHtmlIndex))
  {
    const std::string& textHtml = m_Parts.at(m_TextHtmlIndex).m_Data;
    const std::string& textHtmlPath = Util::GetTempFilename(".html");
    Util::WriteFile(textHtmlPath, textHtml);

    const std::string& textFromHtmlPath = Util::GetTempFilename(".txt");
    const std::string& command = Util::GetHtmlConvertCmd() + std::string(" ") + textHtmlPath +
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

void Body::ParseMime(mailmime *p_Mime) const
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

  switch (p_Mime->mm_type)
  {
    case MAILMIME_SINGLE:
      ParseMimeData(p_Mime->mm_data.mm_single, mimeType);
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

void Body::ParseMimeData(mailmime_data *p_Data, std::string p_MimeType) const
{
  switch (p_Data->dt_type)
  {
    case MAILMIME_DATA_TEXT:
      {
        size_t index = 0;
        char* result = NULL;
        size_t resultLen = 0;
        int rv = mailmime_part_parse(p_Data->dt_data.dt_text.dt_data,
                                     p_Data->dt_data.dt_text.dt_length, &index,
                                     p_Data->dt_encoding, &result, &resultLen);
        if (rv == MAILIMF_NO_ERROR)
        {
          Part part;
          part.m_MimeType = p_MimeType;
          part.m_Data = std::string(result, resultLen);
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
