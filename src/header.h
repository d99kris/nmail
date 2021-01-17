// header.h
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <set>
#include <string>
#include <vector>

class Header
{
public:
  void SetData(const std::string& p_Data);
  void SetHeaderData(const std::string& p_Data, time_t p_ServerTime, bool p_HasAttachments);
  std::string GetData() const;
  std::string GetDateTime();
  std::string GetDateOrTime(const std::string& p_CurrentDate);
  time_t GetTimeStamp();
  std::string GetFrom();
  std::string GetShortFrom();
  std::string GetTo();
  std::string GetShortTo();
  std::string GetCc();
  std::string GetBcc();
  std::string GetReplyTo();
  std::string GetSubject();
  std::string GetUniqueId();
  std::string GetMessageId();
  std::set<std::string> GetAddresses();
  bool GetHasAttachments();

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

private:
  std::string m_Data;

  bool m_Parsed = false;
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
};

std::ostream& operator<<(std::ostream& p_Stream, const Header& p_Header);
