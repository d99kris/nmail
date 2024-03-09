// header.h
//
// Copyright (c) 2019-2024 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <ctime>
#include <set>
#include <string>
#include <vector>

class Header
{
public:
  void SetData(const std::string& p_Data);
  void SetHeaderData(const std::string& p_HdrData, const std::string& p_StrData,
                     const time_t p_ServerTime);
  std::string GetData() const;

  std::string GetDate() const;
  std::string GetDateTime() const;
  std::string GetDateOrTime(const std::string& p_CurrentDate) const;
  time_t GetTimeStamp() const;

  std::string GetFrom() const;
  std::string GetShortFrom() const;
  std::string GetTo() const;
  std::string GetShortTo() const;
  std::string GetCc() const;
  std::string GetBcc() const;
  std::string GetReplyTo() const;
  std::string GetSubject() const;
  std::string GetUniqueId() const;
  std::string GetMessageId() const;
  std::set<std::string> GetAddresses() const;
  bool GetHasAttachments() const;
  std::string GetRawHeaderText(bool p_LocalHeaders);
  inline bool ParseIfNeeded()
  {
    if (m_ParseVersion == GetCurrentParseVersion()) return false;

    Parse();
    return true;
  }

  template<class Archive>
  void serialize(Archive& p_Archive)
  {
    p_Archive(m_Data,
              m_ParseVersion,
              m_Date,
              m_DateTime,
              m_Time,
              m_TimeStamp,
              m_From,
              m_ShortFrom,
              m_To,
              m_ShortTo,
              m_Cc,
              m_Bcc,
              m_ReplyTo,
              m_Subject,
              m_MessageId,
              m_UniqueId,
              m_Addresses,
              m_HasAttachments);
  }

  static std::string GetCurrentDate();

private:
  void Parse();
  std::vector<std::string> MailboxListToStrings(struct mailimf_mailbox_list* p_MailboxList,
                                                const bool p_Short = false);
  std::vector<std::string> AddressListToStrings(struct mailimf_address_list* p_AddrList,
                                                const bool p_Short = false);
  std::string MailboxToString(struct mailimf_mailbox* p_Mailbox,
                              const bool p_Short = false);
  std::string GroupToString(struct mailimf_group* p_Group,
                            const bool p_Short = false);
  size_t GetCurrentParseVersion();

private:
  std::string m_Data;

  size_t m_ParseVersion = 0;
  std::string m_Date;
  std::string m_DateTime;
  std::string m_Time;
  time_t m_TimeStamp = 0;
  std::string m_From;
  std::string m_ShortFrom;
  std::string m_To;
  std::string m_ShortTo;
  std::string m_Cc;
  std::string m_Bcc;
  std::string m_ReplyTo;
  std::string m_Subject;
  std::string m_MessageId;
  std::string m_UniqueId;
  std::set<std::string> m_Addresses;
  bool m_HasAttachments = false;
  std::string m_RawHeaderText;
};

std::ostream& operator<<(std::ostream& p_Stream, const Header& p_Header);
