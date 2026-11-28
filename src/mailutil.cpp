// mailutil.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "mailutil.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <regex>

#include "libetpan_help.h"
#include <libetpan/mailimap.h>
#include <libetpan/mailmime.h>

#include "strutil.h"

std::string MailUtil::m_LocalizedSubjectPrefixes;

time_t MailUtil::MailtimeToTimet(mailimf_date_time* p_Dt)
{
  int year = p_Dt->dt_year;
  if (year < 100)
  {
    // assume two-digit year is 20xx
    year += 2000;
  }

  char buf[128];
  snprintf(buf, sizeof(buf), "%04i-%02i-%02i %02i:%02i:%02i",
           year, p_Dt->dt_month, p_Dt->dt_day, p_Dt->dt_hour,
           p_Dt->dt_min, p_Dt->dt_sec);
  int offs = p_Dt->dt_zone;

  struct tm tm;
  memset(&tm, 0, sizeof(struct tm));
  strptime(buf, "%Y-%m-%d %H:%M:%S", &tm);
  tm.tm_isdst = -1;

  time_t t = timegm(&tm);
  int offs_h = offs / 100;
  int offs_m = offs % 100;
  t -= offs_h * 3600;
  t -= offs_m * 60;

  return t;
}

void MailUtil::MailimapTimeToMailimfTime(mailimap_date_time* p_Src, mailimf_date_time* p_Dst)
{
  p_Dst->dt_day = p_Src->dt_day;
  p_Dst->dt_month = p_Src->dt_month;
  p_Dst->dt_year = p_Src->dt_year;
  p_Dst->dt_hour = p_Src->dt_hour;
  p_Dst->dt_min = p_Src->dt_min;
  p_Dst->dt_sec = p_Src->dt_sec;
  p_Dst->dt_zone = p_Src->dt_zone;
}

std::string MailUtil::MakeReplySubject(const std::string& p_Str)
{
  std::string subject = p_Str;
  NormalizeSubject(subject, false /*p_ToLower*/);
  return ("Re: " + subject);
}

std::string MailUtil::MakeForwardSubject(const std::string& p_Str)
{
  std::string subject = p_Str;
  NormalizeSubject(subject, false /*p_ToLower*/);
  return ("Fwd: " + subject);
}

std::string MailUtil::MimeToUtf8(const std::string& p_Str)
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
    StrUtil::ReplaceString(decoded, "\r", "");
    StrUtil::ReplaceString(decoded, "\n", "");
    return decoded;
  }
  else
  {
    return p_Str;
  }
}

void MailUtil::NormalizeName(std::string& p_String)
{
  std::transform(p_String.begin(), p_String.end(), p_String.begin(), ::tolower);
}

void MailUtil::NormalizeSubject(std::string& p_String, bool p_ToLower)
{
  static const std::regex re = [&]()
  {
    std::vector<std::string> prefixes = { "re", "fwd?" };
    std::vector<std::string> customPrefixes = StrUtil::Split(m_LocalizedSubjectPrefixes, ',');
    prefixes.insert(prefixes.end(), customPrefixes.begin(), customPrefixes.end());
    std::string prefixesJoined = StrUtil::Join(prefixes, "|");
    return std::regex("^((" + prefixesJoined + ") *(:) *)+", std::regex_constants::icase);
  }();

  p_String = std::regex_replace(p_String, re, "");
  if (p_ToLower)
  {
    p_String = StrUtil::ToLower(p_String);
  }
}

void MailUtil::SetLocalizedSubjectPrefixes(const std::string& p_Prefixes)
{
  m_LocalizedSubjectPrefixes = p_Prefixes;
}

// percent-decoded url component
std::string MailUtil::DecodeUrl(const std::string& p_Str)
{
  std::string str;
  for (size_t i = 0; i < p_Str.size(); ++i)
  {
    if ((p_Str[i] == '%') && ((i + 2) < p_Str.size()) && isxdigit((unsigned char)p_Str[i + 1]) &&
        isxdigit((unsigned char)p_Str[i + 2]))
    {
      str += (char)std::stoi(p_Str.substr(i + 1, 2), nullptr, 16);
      i += 2;
    }
    else
    {
      str += p_Str[i];
    }
  }

  return str;
}

std::vector<std::string> MailUtil::SplitAddrs(const std::string& p_Str)
{
  std::vector<std::string> vec = StrUtil::SplitQuoted(p_Str, false);
  return vec;
}

std::vector<std::string> MailUtil::SplitAddrsUnquote(const std::string& p_Str)
{
  std::vector<std::string> vec = StrUtil::SplitQuoted(p_Str, true);
  return vec;
}

std::string MailUtil::EscapeName(const std::string& p_Str)
{
  if (p_Str.empty()) return p_Str;

  if (p_Str.at(0) == '"') return p_Str;

  std::string text = p_Str;
  if ((text.find(",") != std::string::npos) || (text.find("\"") != std::string::npos))
  {
    StrUtil::ReplaceString(text, "\"", "\\\"");
    text = "\"" + text + "\"";
  }

  return text;
}

std::string MailUtil::GetDomainName(const std::string& p_HostAddress)
{
  char key = '.';
  std::size_t lastDot = p_HostAddress.rfind(key);
  if ((lastDot != std::string::npos) && (lastDot > 0))
  {
    std::size_t secondLastDot = p_HostAddress.rfind(key, lastDot - 1);
    if (secondLastDot != std::string::npos)
    {
      return p_HostAddress.substr(secondLastDot + 1);
    }
  }

  return p_HostAddress;
}
