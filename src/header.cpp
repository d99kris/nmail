// header.cpp
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "header.h"

#include <cstring>

#include <libetpan/libetpan.h>

#include "log.h"
#include "loghelp.h"
#include "util.h"

void Header::SetData(const std::string &p_Data)
{
  m_Data = p_Data;
}

std::string Header::GetData() const
{
  return m_Data;
}

std::string Header::GetDate() const
{
  Parse();
  return m_Date;
}

std::string Header::GetShortDate() const
{
  Parse();
  return m_ShortDate;
}

std::string Header::GetFrom() const
{
  Parse();
  return m_From;
}

std::string Header::GetShortFrom() const
{
  Parse();
  return m_ShortFrom;
}

std::string Header::GetTo() const
{
  Parse();
  return m_To;
}

std::string Header::GetCc() const
{
  Parse();
  return m_Cc;
}

std::string Header::GetSubject() const
{
  Parse();
  return m_Subject;
}

void Header::Parse() const
{
  if (!m_Parsed)
  {
    struct mailmime* mime = NULL;
    size_t current_index = 0;
    LOG_IF_IMAP_ERR(mailmime_parse(m_Data.c_str(), m_Data.size(), &current_index, &mime));

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

                    time_t nowtime = time(NULL);
                    struct tm* nowtimeinfo = localtime(&nowtime);
                    char nowdatestr[64];
                    strftime(nowdatestr, sizeof(nowdatestr), "%Y-%m-%d", nowtimeinfo);
                    std::string nowdate(nowdatestr);

                    std::string fulltimestr = sentdate + std::string(" ") + senttime;
                    std::string shorttimestr = (sentdate == nowdate) ? senttime : sentdate;

                    m_Date = fulltimestr;
                    m_ShortDate = shorttimestr;
                  }
                  break;

                case MAILIMF_FIELD_FROM:
                  m_From =
                    MimeToUtf8(MailboxListToString(field->fld_data.fld_from->frm_mb_list));
                  m_ShortFrom =
                    MimeToUtf8(MailboxListToString(field->fld_data.fld_from->frm_mb_list, true));
                  break;

                case MAILIMF_FIELD_TO:
                  m_To = MimeToUtf8(AddressListToString(field->fld_data.fld_to->to_addr_list));
                  break;

                case MAILIMF_FIELD_CC:
                  m_Cc = MimeToUtf8(AddressListToString(field->fld_data.fld_cc->cc_addr_list));
                  break;

                case MAILIMF_FIELD_SUBJECT:
                  m_Subject = MimeToUtf8(std::string(field->fld_data.fld_subject->sbj_value));
                  break;

                default:
                  break;
              }
            }
          }
        }
      }

      mailmime_free(mime);
    }

    m_Parsed = true;
  }
}

std::string Header::MailboxListToString(mailimf_mailbox_list *p_MailboxList,
                                        const bool p_Short) const
{
  std::string str;
  for (clistiter* it = clist_begin(p_MailboxList->mb_list); it != NULL; it = clist_next(it))
  {
    struct mailimf_mailbox* mb = (struct mailimf_mailbox*)clist_content(it);
    str += MailboxToString(mb, p_Short);
    if (clist_next(it) != NULL)
    {
      str += std::string(", ");
    }
  }

  return str;
}

std::string Header::AddressListToString(mailimf_address_list *p_AddrList) const
{
  std::string str;
  for (clistiter* it = clist_begin(p_AddrList->ad_list); it != NULL; it = clist_next(it))
  {
    struct mailimf_address* addr = (struct mailimf_address*)clist_content(it);

    switch (addr->ad_type)
    {
      case MAILIMF_ADDRESS_GROUP:
        str += GroupToString(addr->ad_data.ad_group);
        break;

      case MAILIMF_ADDRESS_MAILBOX:
        str += MailboxToString(addr->ad_data.ad_mailbox);
        break;

      default:
        break;
    }

    if (clist_next(it) != NULL)
    {
      str += std::string(", ");
    }
  }

  return str;
}

std::string Header::MailboxToString(mailimf_mailbox *p_Mailbox, const bool p_Short) const
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

std::string Header::GroupToString(mailimf_group *p_Group) const
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

std::string Header::MimeToUtf8(const std::string &p_Str) const
{
  const char* charset = "UTF-8";
  char* cdecoded = NULL;
  size_t curtoken = 0;
  int rv = mailmime_encoded_phrase_parse(charset, p_Str.c_str(), p_Str.size(), &curtoken,
                                         charset, &cdecoded);
  if ((rv == MAILIMF_NO_ERROR) && (cdecoded != NULL))
  {
    std::string decoded(cdecoded);
    free(cdecoded);
    return decoded;
  }
  else
  {
    return p_Str;
  }
}
