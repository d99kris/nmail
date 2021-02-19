// smtp.h
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <libetpan/libetpan.h>

#include "contact.h"

class Smtp
{
public:
  Smtp(const std::string& p_User, const std::string& p_Pass, const std::string& p_Host,
       const uint16_t p_Port, const std::string& p_Name, const std::string& p_Address,
       const int64_t p_Timeout);
  virtual ~Smtp();

  bool Send(const std::string& p_Subject, const std::string& p_Message,
            const std::string& p_HtmlMessage,
            const std::vector<Contact>& p_To,
            const std::vector<Contact>& p_Cc,
            const std::vector<Contact>& p_Bcc,
            const std::string& p_RefMsgId,
            const std::vector<std::string>& p_AttachmentPaths,
            std::string& p_ResultMessage);
  bool Send(const std::string& p_Data, const std::vector<Contact>& p_To,
            const std::vector<Contact>& p_Cc, const std::vector<Contact>& p_Bcc);
  std::string GetHeader(const std::string& p_Subject, const std::vector<Contact>& p_To,
                        const std::vector<Contact>& p_Cc, const std::vector<Contact>& p_Bcc,
                        const std::string& p_RefMsgId);
  std::string GetBody(const std::string& p_Message, const std::string& p_HtmlMessage,
                      const std::vector<std::string>& p_AttachmentPaths);

private:
  bool SendMessage(const std::string& p_Data, const std::vector<Contact>& p_Recipients);
  struct mailmime* GetMimeTextPart(const char* p_MimeType, const std::string& p_Message);
  struct mailmime* GetMimeFilePart(const std::string& p_Path,
                                   const std::string& p_MimeType = "application/octet-stream");
  struct mailmime* GetMimePart(struct mailmime_content* p_Content,
                               struct mailmime_fields* p_MimeFields,
                               bool p_ForceSingle);
  std::string MimeEncodeStr(const std::string& p_In);

  static void Logger(mailsmtp* p_Smtp, int p_LogType, const char* p_Buffer, size_t p_Size,
                     void* p_UserData);

private:
  std::mutex m_Mutex;
  std::string m_User;
  std::string m_Pass;
  std::string m_Host;
  uint16_t m_Port = 0;
  std::string m_Name;
  std::string m_Address;
  int64_t m_Timeout = 0;
};
