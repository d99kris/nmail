// loghelp.cpp
//
// Copyright (c) 2019-2025 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "loghelp.h"

#include <map>
#include <sstream>
#include <string>

#include "libetpan_help.h"
#include <libetpan/mailimap_types.h>
#include <libetpan/mailsmtp_types.h>

#define STRINGIFY(name) STRINGIFY_HELPER(name)
#define STRINGIFY_HELPER(name) #name
#define VALSTR(val) { val, STRINGIFY(val) }

std::mutex LogLatency::m_Mutex;
std::map<LatencyMetric, std::chrono::high_resolution_clock::time_point> LogLatency::m_StartTimes;

void LogHelp::PrettyPrintArgsHelper(std::stringstream& /*p_Sstream*/)
{
}

std::string LogHelp::ImapErrToStr(int p_ImapErr)
{
  static const std::map<int, std::string> errMap =
  {
    VALSTR(MAILIMAP_NO_ERROR),
    VALSTR(MAILIMAP_NO_ERROR_AUTHENTICATED),
    VALSTR(MAILIMAP_NO_ERROR_NON_AUTHENTICATED),
    VALSTR(MAILIMAP_ERROR_BAD_STATE),
    VALSTR(MAILIMAP_ERROR_STREAM),
    VALSTR(MAILIMAP_ERROR_PARSE),
    VALSTR(MAILIMAP_ERROR_CONNECTION_REFUSED),
    VALSTR(MAILIMAP_ERROR_MEMORY),
    VALSTR(MAILIMAP_ERROR_FATAL),
    VALSTR(MAILIMAP_ERROR_PROTOCOL),
    VALSTR(MAILIMAP_ERROR_DONT_ACCEPT_CONNECTION),
    VALSTR(MAILIMAP_ERROR_APPEND),
    VALSTR(MAILIMAP_ERROR_NOOP),
    VALSTR(MAILIMAP_ERROR_LOGOUT),
    VALSTR(MAILIMAP_ERROR_CAPABILITY),
    VALSTR(MAILIMAP_ERROR_CHECK),
    VALSTR(MAILIMAP_ERROR_CLOSE),
    VALSTR(MAILIMAP_ERROR_EXPUNGE),
    VALSTR(MAILIMAP_ERROR_COPY),
    VALSTR(MAILIMAP_ERROR_UID_COPY),
    VALSTR(MAILIMAP_ERROR_MOVE),
    VALSTR(MAILIMAP_ERROR_UID_MOVE),
    VALSTR(MAILIMAP_ERROR_CREATE),
    VALSTR(MAILIMAP_ERROR_DELETE),
    VALSTR(MAILIMAP_ERROR_EXAMINE),
    VALSTR(MAILIMAP_ERROR_FETCH),
    VALSTR(MAILIMAP_ERROR_UID_FETCH),
    VALSTR(MAILIMAP_ERROR_LIST),
    VALSTR(MAILIMAP_ERROR_LOGIN),
    VALSTR(MAILIMAP_ERROR_LSUB),
    VALSTR(MAILIMAP_ERROR_RENAME),
    VALSTR(MAILIMAP_ERROR_SEARCH),
    VALSTR(MAILIMAP_ERROR_UID_SEARCH),
    VALSTR(MAILIMAP_ERROR_SELECT),
    VALSTR(MAILIMAP_ERROR_STATUS),
    VALSTR(MAILIMAP_ERROR_STORE),
    VALSTR(MAILIMAP_ERROR_UID_STORE),
    VALSTR(MAILIMAP_ERROR_SUBSCRIBE),
    VALSTR(MAILIMAP_ERROR_UNSUBSCRIBE),
    VALSTR(MAILIMAP_ERROR_STARTTLS),
    VALSTR(MAILIMAP_ERROR_INVAL),
    VALSTR(MAILIMAP_ERROR_EXTENSION),
    VALSTR(MAILIMAP_ERROR_SASL),
    VALSTR(MAILIMAP_ERROR_SSL),
    VALSTR(MAILIMAP_ERROR_NEEDS_MORE_DATA),
    VALSTR(MAILIMAP_ERROR_CUSTOM_COMMAND),
  };

  auto it = errMap.find(p_ImapErr);

  return (it != errMap.end()) ? it->second : std::to_string(p_ImapErr);
}

std::string LogHelp::SmtpErrToStr(int p_SmtpErr)
{
  static const std::map<int, std::string> errMap =
  {
    VALSTR(MAILSMTP_NO_ERROR),
    VALSTR(MAILSMTP_ERROR_UNEXPECTED_CODE),
    VALSTR(MAILSMTP_ERROR_SERVICE_NOT_AVAILABLE),
    VALSTR(MAILSMTP_ERROR_STREAM),
    VALSTR(MAILSMTP_ERROR_HOSTNAME),
    VALSTR(MAILSMTP_ERROR_NOT_IMPLEMENTED),
    VALSTR(MAILSMTP_ERROR_ACTION_NOT_TAKEN),
    VALSTR(MAILSMTP_ERROR_EXCEED_STORAGE_ALLOCATION),
    VALSTR(MAILSMTP_ERROR_IN_PROCESSING),
    VALSTR(MAILSMTP_ERROR_INSUFFICIENT_SYSTEM_STORAGE),
    VALSTR(MAILSMTP_ERROR_MAILBOX_UNAVAILABLE),
    VALSTR(MAILSMTP_ERROR_MAILBOX_NAME_NOT_ALLOWED),
    VALSTR(MAILSMTP_ERROR_BAD_SEQUENCE_OF_COMMAND),
    VALSTR(MAILSMTP_ERROR_USER_NOT_LOCAL),
    VALSTR(MAILSMTP_ERROR_TRANSACTION_FAILED),
    VALSTR(MAILSMTP_ERROR_MEMORY),
    VALSTR(MAILSMTP_ERROR_AUTH_NOT_SUPPORTED),
    VALSTR(MAILSMTP_ERROR_AUTH_LOGIN),
    VALSTR(MAILSMTP_ERROR_AUTH_REQUIRED),
    VALSTR(MAILSMTP_ERROR_AUTH_TOO_WEAK),
    VALSTR(MAILSMTP_ERROR_AUTH_TRANSITION_NEEDED),
    VALSTR(MAILSMTP_ERROR_AUTH_TEMPORARY_FAILTURE),
    VALSTR(MAILSMTP_ERROR_AUTH_ENCRYPTION_REQUIRED),
    VALSTR(MAILSMTP_ERROR_STARTTLS_TEMPORARY_FAILURE),
    VALSTR(MAILSMTP_ERROR_STARTTLS_NOT_SUPPORTED),
    VALSTR(MAILSMTP_ERROR_CONNECTION_REFUSED),
    VALSTR(MAILSMTP_ERROR_AUTH_AUTHENTICATION_FAILED),
    VALSTR(MAILSMTP_ERROR_SSL),
  };

  auto it = errMap.find(p_SmtpErr);

  return (it != errMap.end()) ? it->second : std::to_string(p_SmtpErr);
}

int LogHelp::LogImap(int p_Rv, const char* p_Expr, const char* p_File, int p_Line)
{
  if (p_Rv > MAILIMAP_NO_ERROR_NON_AUTHENTICATED)
  {
    Log::Error(p_File, p_Line, "%s = %s", p_Expr, ImapErrToStr(p_Rv).c_str());
  }
  else if (Log::GetDebugEnabled())
  {
    Log::Debug(p_File, p_Line, "%s = %s", p_Expr, ImapErrToStr(p_Rv).c_str());
  }

  return p_Rv;
}

int LogHelp::LogImapLogout(int p_Rv, const char* p_Expr, const char* p_File, int p_Line)
{
  if ((p_Rv > MAILIMAP_NO_ERROR_NON_AUTHENTICATED) && (p_Rv != MAILIMAP_ERROR_STREAM))
  {
    Log::Error(p_File, p_Line, "%s = %s", p_Expr, ImapErrToStr(p_Rv).c_str());
  }
  else if (Log::GetDebugEnabled())
  {
    Log::Debug(p_File, p_Line, "%s = %s", p_Expr, ImapErrToStr(p_Rv).c_str());
  }

  return p_Rv;
}

int LogHelp::LogSmtp(int p_Rv, const char* p_Expr, const char* p_File, int p_Line)
{
  if (p_Rv != MAILSMTP_NO_ERROR)
  {
    Log::Error(p_File, p_Line, "%s = %s", p_Expr, SmtpErrToStr(p_Rv).c_str());
  }
  else if (Log::GetDebugEnabled())
  {
    Log::Debug(p_File, p_Line, "%s = %s", p_Expr, SmtpErrToStr(p_Rv).c_str());
  }

  return p_Rv;
}
