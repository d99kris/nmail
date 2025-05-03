// smtp.h
//
// Copyright (c) 2019-2025 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "contact.h"

struct mailsmtp;

enum SmtpStatus
{
  SmtpStatusOk = 0,
  SmtpStatusFailed = 1,
  SmtpStatusSaslFailed = 2,
  SmtpStatusAuthFailed = 3,
  SmtpStatusConnFailed = 4,
  SmtpStatusInitFailed = 5,
  SmtpStatusMessageFailed = 6,
  SmtpStatusImplFailed = 7,
};

class Smtp
{
public:
  Smtp(const std::string& p_User, const std::string& p_Pass, const std::string& p_Host,
       const uint16_t p_Port, const std::string& p_Address, const int64_t p_Timeout);
  virtual ~Smtp();

  SmtpStatus Send(const std::string& p_Subject, const std::string& p_Message,
                  const std::string& p_HtmlMessage,
                  const std::vector<Contact>& p_To,
                  const std::vector<Contact>& p_Cc,
                  const std::vector<Contact>& p_Bcc,
                  const std::string& p_RefMsgId,
                  const Contact& p_From,
                  const std::vector<std::string>& p_AttachmentPaths,
                  const bool p_Flowed,
                  std::string& p_ResultMessage);
  SmtpStatus Send(const std::string& p_Data, const std::vector<Contact>& p_To,
                  const std::vector<Contact>& p_Cc, const std::vector<Contact>& p_Bcc);
  std::string GetHeader(const std::string& p_Subject, const std::vector<Contact>& p_To,
                        const std::vector<Contact>& p_Cc, const std::vector<Contact>& p_Bcc,
                        const std::string& p_RefMsgId, const Contact& p_From);
  std::string GetBody(const std::string& p_Message, const std::string& p_HtmlMessage,
                      const std::vector<std::string>& p_AttachmentPaths, bool p_Flowed);
  static std::string GetErrorMessage(SmtpStatus p_SmtpStatus);

private:
  SmtpStatus SendMessage(const std::string& p_Data, const std::vector<Contact>& p_Recipients);
  struct mailmime* GetMimeTextPart(const char* p_MimeType, const std::string& p_Message, bool p_Flowed);
  struct mailmime* GetMimeFilePart(const std::string& p_Path,
                                   const std::string& p_MimeType);
  std::string GetMimeType(const std::string& p_Path);
  struct mailmime* GetMimePart(struct mailmime_content* p_Content,
                               struct mailmime_fields* p_MimeFields,
                               bool p_ForceSingle);
  static std::string MimeEncodeStr(const std::string& p_In);
  static std::vector<std::string> LineWrap(const std::string& p_Str, size_t p_Len);
  static std::string MimeEncodeWrap(const std::string& p_In);
  std::string RemoveBccHeader(const std::string& p_Data);

  std::string GenerateMessageId() const;

  static void Logger(mailsmtp* p_Smtp, int p_LogType, const char* p_Buffer, size_t p_Size,
                     void* p_UserData);

private:
  std::mutex m_Mutex;
  std::string m_User;
  std::string m_Pass;
  std::string m_Host;
  uint16_t m_Port = 0;
  std::string m_Address;
  int64_t m_Timeout = 0;
};
