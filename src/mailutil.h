// mailutil.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <ctime>
#include <string>
#include <vector>

struct mailimap_date_time;

class MailUtil
{
public:
  static time_t MailtimeToTimet(struct mailimf_date_time* p_Dt);
  static void MailimapTimeToMailimfTime(mailimap_date_time* p_Src, mailimf_date_time* p_Dst);
  static std::string MakeReplySubject(const std::string& p_Str);
  static std::string MakeForwardSubject(const std::string& p_Str);
  static std::string MimeToUtf8(const std::string& p_Str);
  static void NormalizeName(std::string& p_String);
  static void NormalizeSubject(std::string& p_String, bool p_ToLower);
  static void SetLocalizedSubjectPrefixes(const std::string& p_Prefixes);
  static std::string DecodeUrl(const std::string& p_Str);
  static std::vector<std::string> SplitAddrs(const std::string& p_Str);
  static std::vector<std::string> SplitAddrsUnquote(const std::string& p_Str);
  static std::string EscapeName(const std::string& p_Str);
  static std::string GetDomainName(const std::string& p_HostAddress);

private:
  static std::string m_LocalizedSubjectPrefixes;
};
