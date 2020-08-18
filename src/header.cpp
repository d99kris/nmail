// header.cpp
//
// Copyright (c) 2019-2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "header.h"

#include <cstring>
#include <set>

#include <libetpan/libetpan.h>

#include "crypto.h"
#include "log.h"
#include "loghelp.h"
#include "sethelp.h"
#include "util.h"

void Header::SetData(const std::string &p_Data)
{
  m_Data = p_Data;
}

std::string Header::GetData() const
{
  return m_Data;
}

std::string Header::GetDateTime()
{
  Parse();
  return m_DateTime;
}

std::string Header::GetDateOrTime(const std::string& p_CurrentDate)
{
  Parse();
  return (m_Date == p_CurrentDate) ? m_Time : m_Date;
}

time_t Header::GetTimeStamp()
{
  Parse();
  return m_TimeStamp;
}

std::string Header::GetFrom()
{
  Parse();
  return m_From;
}

std::string Header::GetShortFrom()
{
  Parse();
  return m_ShortFrom;
}

std::string Header::GetTo()
{
  Parse();
  return m_To;
}

std::string Header::GetCc()
{
  Parse();
  return m_Cc;
}

std::string Header::GetReplyTo()
{
  Parse();
  return m_ReplyTo;
}

std::string Header::GetSubject()
{
  Parse();
  return m_Subject;
}

std::string Header::GetUniqueId()
{
  Parse();
  return m_UniqueId;
}

std::string Header::GetMessageId()
{
  Parse();
  return m_MessageId;
}

std::set<std::string> Header::GetAddresses()
{
  Parse();
  return m_Addresses;
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
  if (!m_Parsed)
  {
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
                    time_t rawtime = Util::MailtimeToTimet(dt);
                    struct tm* timeinfo = localtime(&rawtime);

                    char senttimestr[64];
                    strftime(senttimestr, sizeof(senttimestr), "%H:%M", timeinfo);
                    std::string senttime(senttimestr);

                    char sentdatestr[64];
                    strftime(sentdatestr, sizeof(sentdatestr), "%Y-%m-%d", timeinfo);
                    std::string sentdate(sentdatestr);

                    m_Date = sentdate;
                    m_DateTime = sentdate + std::string(" ") + senttime;
                    m_Time = senttime;
                    m_TimeStamp = rawtime;
                  }
                  break;

                case MAILIMF_FIELD_FROM:
                  addrs = Util::MimeToUtf8(MailboxListToStrings(field->fld_data.fld_from->frm_mb_list));
                  m_Addresses = m_Addresses + std::set<std::string>(addrs.begin(), addrs.end());
                  m_From = Util::Join(addrs, ", ");
                  addrs = Util::MimeToUtf8(MailboxListToStrings(field->fld_data.fld_from->frm_mb_list, true));
                  m_ShortFrom = Util::Join(addrs, ", ");
                  break;

                case MAILIMF_FIELD_TO:
                  addrs = Util::MimeToUtf8(AddressListToStrings(field->fld_data.fld_to->to_addr_list));
                  m_Addresses = m_Addresses + std::set<std::string>(addrs.begin(), addrs.end());
                  m_To = Util::Join(addrs, ", ");
                  break;

                case MAILIMF_FIELD_CC:
                  addrs = Util::MimeToUtf8(AddressListToStrings(field->fld_data.fld_cc->cc_addr_list));
                  m_Addresses = m_Addresses + std::set<std::string>(addrs.begin(), addrs.end());
                  m_Cc = Util::Join(addrs, ", ");
                  break;

                case MAILIMF_FIELD_SUBJECT:
                  m_Subject = Util::MimeToUtf8(std::string(field->fld_data.fld_subject->sbj_value));
                  Util::ReplaceString(m_Subject, "\r", "");
                  Util::ReplaceString(m_Subject, "\n", "");
                  break;

                case MAILIMF_FIELD_MESSAGE_ID:
                  m_MessageId = std::string(field->fld_data.fld_message_id->mid_value);
                  break;

                case MAILIMF_FIELD_REPLY_TO:
                  addrs = Util::MimeToUtf8(AddressListToStrings(field->fld_data.fld_reply_to->rt_addr_list));
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

    m_Parsed = true;
  }
}

std::vector<std::string> Header::MailboxListToStrings(mailimf_mailbox_list *p_MailboxList,
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

std::vector<std::string> Header::AddressListToStrings(mailimf_address_list *p_AddrList)
{
  std::vector<std::string> strs;
  for (clistiter* it = clist_begin(p_AddrList->ad_list); it != NULL; it = clist_next(it))
  {
    struct mailimf_address* addr = (struct mailimf_address*)clist_content(it);

    switch (addr->ad_type)
    {
      case MAILIMF_ADDRESS_GROUP:
        strs.push_back(GroupToString(addr->ad_data.ad_group));
        break;

      case MAILIMF_ADDRESS_MAILBOX:
        strs.push_back(MailboxToString(addr->ad_data.ad_mailbox));
        break;

      default:
        break;
    }
  }

  return strs;
}

std::string Header::MailboxToString(mailimf_mailbox *p_Mailbox, const bool p_Short)
{
  std::string str;
  if (p_Short)
  {
    if (p_Mailbox->mb_display_name != NULL)
    {
      str = std::string(p_Mailbox->mb_display_name);
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
      str = std::string(p_Mailbox->mb_display_name) + std::string(" ");
      str += std::string("<") + std::string(p_Mailbox->mb_addr_spec) + std::string(">");
    }
    else
    {
      str += std::string(p_Mailbox->mb_addr_spec);
    }
  }

  return str;
}

std::string Header::GroupToString(mailimf_group *p_Group)
{
  std::string str;
  str += std::string(p_Group->grp_display_name) + std::string(": ");

  for (clistiter *it = clist_begin(p_Group->grp_mb_list->mb_list); it != NULL;
       it = clist_next(it))
  {
    struct mailimf_mailbox* mb = (struct mailimf_mailbox*)clist_content(it);
    str += MailboxToString(mb);
  }

  str += std::string("; ");

  return str;
}

std::ostream& operator<<(std::ostream& p_Stream, const Header& p_Header)
{
  p_Stream << p_Header.GetData();
  return p_Stream;
}
