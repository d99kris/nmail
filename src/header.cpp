// header.cpp
//
// Copyright (c) 2019-2022 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "header.h"

#include <cstring>
#include <set>

#include <libetpan/libetpan.h>

#include "body.h"
#include "crypto.h"
#include "log.h"
#include "loghelp.h"
#include "sethelp.h"
#include "util.h"

static const std::string labelServerTime("X-Nmail-ServerTime: ");

void Header::SetData(const std::string& p_Data)
{
  m_Data = p_Data;
  ParseIfNeeded();
}

void Header::SetHeaderData(const std::string& p_HdrData, const std::string& p_StrData,
                           const time_t p_ServerTime)
{
  m_Data =
    labelServerTime + std::to_string(p_ServerTime) + "\n" +
    p_HdrData +
    p_StrData;
  ParseIfNeeded();
}

std::string Header::GetData() const
{
  return m_Data;
}

std::string Header::GetDate() const
{
  return m_Date;
}

std::string Header::GetDateTime() const
{
  return m_DateTime;
}

std::string Header::GetDateOrTime(const std::string& p_CurrentDate) const
{
  return (m_Date == p_CurrentDate) ? m_Time : m_Date;
}

time_t Header::GetTimeStamp() const
{
  return m_TimeStamp;
}

std::string Header::GetFrom() const
{
  return m_From;
}

std::string Header::GetShortFrom() const
{
  return m_ShortFrom;
}

std::string Header::GetTo() const
{
  return m_To;
}

std::string Header::GetShortTo() const
{
  return m_ShortTo;
}

std::string Header::GetCc() const
{
  return m_Cc;
}

std::string Header::GetBcc() const
{
  return m_Bcc;
}

std::string Header::GetReplyTo() const
{
  return m_ReplyTo;
}

std::string Header::GetSubject() const
{
  return m_Subject;
}

std::string Header::GetUniqueId() const
{
  return m_UniqueId;
}

std::string Header::GetMessageId() const
{
  return m_MessageId;
}

std::set<std::string> Header::GetAddresses() const
{
  return m_Addresses;
}

bool Header::GetHasAttachments() const
{
  return m_HasAttachments;
}

std::string Header::GetRawHeaderText(bool p_LocalHeaders)
{
  std::string& raw = m_RawHeaderText;
  if (!raw.empty()) return raw;

  raw = m_Data;
  raw.erase(std::remove(raw.begin(), raw.end(), L'\r'), raw.end());

  // remove body structure header info
  size_t endpos = raw.find("\n\n");
  if (endpos != std::string::npos)
  {
    raw = raw.substr(0, endpos + 1);
  }

  // remove local headers if not requested
  if (!p_LocalHeaders)
  {
    size_t startpos = raw.find("\n");
    if (startpos != std::string::npos)
    {
      raw = raw.substr(startpos + 1);
    }
  }

  m_RawHeaderText = raw;

  return raw;
}

std::string Header::GetCurrentDate()
{
  time_t nowtime = time(NULL);
  struct tm* nowtimeinfo = localtime(&nowtime);
  char nowdatestr[64];
  strftime(nowdatestr, sizeof(nowdatestr), "%Y-%m-%d", nowtimeinfo);
  return std::string(nowdatestr);
}

void Header::Parse()
{
  // @note: this function should not be called directly, only via ParseIfNeeded()
  LOG_DURATION();
  time_t headerTimeStamp = 0;
  time_t serverTimeStamp = 0;

  {
    std::stringstream sstream(m_Data);
    std::string line;
    if (std::getline(sstream, line))
    {
      if ((line.rfind(labelServerTime, 0) == 0) && (line.size() > labelServerTime.size()))
      {
        std::string serverTime = line.substr(labelServerTime.size());
        serverTimeStamp = std::stoi(serverTime);
      }
      else
      {
        LOG_WARNING("unexpected hdr content \"%s\"", line.c_str());
      }
    }
    else
    {
      LOG_WARNING("unexpected empty hdr");
    }
  }

  {
    Body body;
    body.FromHeader(m_Data);
    m_HasAttachments = body.HasAttachments();
  }

  struct mailmime* mime = NULL;
  size_t current_index = 0;
  mailmime_parse(m_Data.c_str(), m_Data.size(), &current_index, &mime);

  if (mime != NULL)
  {
    if (mime->mm_type == MAILMIME_MESSAGE)
    {
      if (mime->mm_data.mm_message.mm_fields)
      {
        if (clist_begin(mime->mm_data.mm_message.mm_fields->fld_list) != NULL)
        {
          struct mailimf_fields* fields = mime->mm_data.mm_message.mm_fields;
          for (clistiter* it = clist_begin(fields->fld_list); it != NULL; it = clist_next(it))
          {
            std::vector<std::string> addrs;
            struct mailimf_field* field = (struct mailimf_field*)clist_content(it);
            switch (field->fld_type)
            {
              case MAILIMF_FIELD_ORIG_DATE:
                {
                  struct mailimf_date_time* dt = field->fld_data.fld_orig_date->dt_date_time;
                  headerTimeStamp = Util::MailtimeToTimet(dt);
                }
                break;

              case MAILIMF_FIELD_FROM:
                addrs = MailboxListToStrings(field->fld_data.fld_from->frm_mb_list);
                m_Addresses = m_Addresses + std::set<std::string>(addrs.begin(), addrs.end());
                m_From = Util::Join(addrs, ", ");
                addrs = MailboxListToStrings(field->fld_data.fld_from->frm_mb_list, true);
                m_ShortFrom = Util::Join(addrs, ", ");
                break;

              case MAILIMF_FIELD_TO:
                addrs = AddressListToStrings(field->fld_data.fld_to->to_addr_list);
                m_Addresses = m_Addresses + std::set<std::string>(addrs.begin(), addrs.end());
                m_To = Util::Join(addrs, ", ");
                addrs = AddressListToStrings(field->fld_data.fld_to->to_addr_list, true);
                m_ShortTo = Util::Join(addrs, ", ");
                break;

              case MAILIMF_FIELD_CC:
                addrs = AddressListToStrings(field->fld_data.fld_cc->cc_addr_list);
                m_Addresses = m_Addresses + std::set<std::string>(addrs.begin(), addrs.end());
                m_Cc = Util::Join(addrs, ", ");
                break;

              case MAILIMF_FIELD_BCC:
                if (field->fld_data.fld_bcc->bcc_addr_list != nullptr)
                {
                  addrs = AddressListToStrings(field->fld_data.fld_bcc->bcc_addr_list);
                  m_Addresses = m_Addresses + std::set<std::string>(addrs.begin(), addrs.end());
                  m_Bcc = Util::Join(addrs, ", ");
                }
                break;

              case MAILIMF_FIELD_SUBJECT:
                m_Subject = Util::MimeToUtf8(std::string(field->fld_data.fld_subject->sbj_value));
                break;

              case MAILIMF_FIELD_MESSAGE_ID:
                m_MessageId = std::string(field->fld_data.fld_message_id->mid_value);
                break;

              case MAILIMF_FIELD_REPLY_TO:
                addrs = AddressListToStrings(field->fld_data.fld_reply_to->rt_addr_list);
                m_Addresses = m_Addresses + std::set<std::string>(addrs.begin(), addrs.end());
                m_ReplyTo = Util::Join(addrs, ", ");
                break;

              default:
                break;
            }
          }

          m_UniqueId = Crypto::SHA256(m_From + m_DateTime + m_MessageId);
        }
      }
    }

    mailmime_free(mime);
  }

  static const bool useServerTime = Util::GetUseServerTimestamps();
  time_t timeStamp = useServerTime ? serverTimeStamp : headerTimeStamp;
  if (timeStamp == 0)
  {
    // fall back to other option
    timeStamp = useServerTime ? headerTimeStamp : serverTimeStamp;
  }

  if (timeStamp != 0)
  {
    struct tm* timeinfo = localtime(&timeStamp);

    char senttimestr[64];
    strftime(senttimestr, sizeof(senttimestr), "%H:%M", timeinfo);
    std::string senttime(senttimestr);

    char sentdatestr[64];
    strftime(sentdatestr, sizeof(sentdatestr), "%Y-%m-%d", timeinfo);
    std::string sentdate(sentdatestr);

    m_TimeStamp = timeStamp;
    m_Date = sentdate;
    m_DateTime = sentdate + std::string(" ") + senttime;
    m_Time = senttime;
  }

  m_ParseVersion = GetCurrentParseVersion();
}

std::vector<std::string> Header::MailboxListToStrings(mailimf_mailbox_list* p_MailboxList,
                                                      const bool p_Short)
{
  std::vector<std::string> strs;
  for (clistiter* it = clist_begin(p_MailboxList->mb_list); it != NULL; it = clist_next(it))
  {
    struct mailimf_mailbox* mb = (struct mailimf_mailbox*)clist_content(it);
    strs.push_back(MailboxToString(mb, p_Short));
  }

  return strs;
}

std::vector<std::string> Header::AddressListToStrings(mailimf_address_list* p_AddrList,
                                                      const bool p_Short)
{
  std::vector<std::string> strs;
  for (clistiter* it = clist_begin(p_AddrList->ad_list); it != NULL; it = clist_next(it))
  {
    struct mailimf_address* addr = (struct mailimf_address*)clist_content(it);

    switch (addr->ad_type)
    {
      case MAILIMF_ADDRESS_GROUP:
        strs.push_back(GroupToString(addr->ad_data.ad_group, p_Short));
        break;

      case MAILIMF_ADDRESS_MAILBOX:
        strs.push_back(MailboxToString(addr->ad_data.ad_mailbox, p_Short));
        break;

      default:
        break;
    }
  }

  return strs;
}

std::string Header::MailboxToString(mailimf_mailbox* p_Mailbox, const bool p_Short)
{
  std::string str;
  if (p_Short)
  {
    if (p_Mailbox->mb_display_name != NULL)
    {
      str = Util::MimeToUtf8(std::string(p_Mailbox->mb_display_name));
    }
    else
    {
      str = std::string(p_Mailbox->mb_addr_spec);
    }
  }
  else
  {
    if ((p_Mailbox->mb_display_name != NULL) && (strlen(p_Mailbox->mb_display_name) > 0))
    {
      str = Util::EscapeName(Util::MimeToUtf8(std::string(p_Mailbox->mb_display_name)));
      str += std::string(" <") + std::string(p_Mailbox->mb_addr_spec) + std::string(">");
    }
    else
    {
      str += std::string(p_Mailbox->mb_addr_spec);
    }
  }

  return str;
}

std::string Header::GroupToString(mailimf_group* p_Group, const bool p_Short)
{
  std::string str;
  str += Util::MimeToUtf8(std::string(p_Group->grp_display_name)) + std::string(": ");

  for (clistiter* it = clist_begin(p_Group->grp_mb_list->mb_list); it != NULL;
       it = clist_next(it))
  {
    struct mailimf_mailbox* mb = (struct mailimf_mailbox*)clist_content(it);
    str += MailboxToString(mb, p_Short);
  }

  str += std::string("; ");

  return str;
}

size_t Header::GetCurrentParseVersion()
{
  static size_t parseVersion = 2; // update offset when parsing changes
  return parseVersion;
}

std::ostream& operator<<(std::ostream& p_Stream, const Header& p_Header)
{
  p_Stream << p_Header.GetData();
  return p_Stream;
}
