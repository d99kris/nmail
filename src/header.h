// header.h
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

class Header
{
public:
  void SetData(const std::string& p_Data);
  std::string GetData() const;
  std::string GetDate() const;
  std::string GetShortDate() const;
  std::string GetFrom() const;
  std::string GetShortFrom() const;
  std::string GetTo() const;
  std::string GetCc() const;
  std::string GetSubject() const;

private:
  void Parse() const;
  std::string MailboxListToString(struct mailimf_mailbox_list* p_MailboxList,
                                  const bool p_Short = false) const;
  std::string AddressListToString(struct mailimf_address_list* p_AddrList) const;
  std::string MailboxToString(struct mailimf_mailbox* p_Mailbox,
                              const bool p_Short = false) const;
  std::string GroupToString(struct mailimf_group* p_Group) const;
  std::string MimeToUtf8(const std::string& p_Str) const;

private:
  std::string m_Data;

  mutable bool m_Parsed = false;
  mutable std::string m_Date;
  mutable std::string m_ShortDate;
  mutable std::string m_From;
  mutable std::string m_ShortFrom;
  mutable std::string m_To;
  mutable std::string m_Cc;
  mutable std::string m_Subject;
};
