// smtp.cpp
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "smtp.h"

#include <cstring>

#include <unistd.h>

#include <uuid/uuid.h>

#include "auth.h"
#include "log.h"
#include "loghelp.h"
#include "sasl.h"

Smtp::Smtp(const std::string& p_User, const std::string& p_Pass, const std::string& p_Host,
           const uint16_t p_Port, const std::string& p_Address, const int64_t p_Timeout)
  : m_User(p_User)
  , m_Pass(p_Pass)
  , m_Host(p_Host)
  , m_Port(p_Port)
  , m_Address(p_Address)
  , m_Timeout(p_Timeout)
{
  if (Log::GetTraceEnabled())
  {
    LOG_TRACE_FUNC(STR(p_User, "***" /*p_Pass*/, p_Host, p_Port, p_Address));
  }
  else
  {
    LOG_DEBUG_FUNC(STR("***", "***" /*p_Pass*/, p_Host, p_Port, "***", "***"));
  }
}

Smtp::~Smtp()
{
  LOG_DEBUG_FUNC(STR());
}

SmtpStatus Smtp::Send(const std::string& p_Subject, const std::string& p_Message,
                      const std::string& p_HtmlMessage,
                      const std::vector<Contact>& p_To, const std::vector<Contact>& p_Cc,
                      const std::vector<Contact>& p_Bcc,
                      const std::string& p_RefMsgId,
                      const Contact& p_From,
                      const std::vector<std::string>& p_AttachmentPaths,
                      const bool p_Flowed,
                      std::string& p_ResultMessage)
{
  LOG_DEBUG_FUNC(STR());
  LOG_TRACE_FUNC(STR(p_Subject, p_Message, p_To, p_Cc, p_Bcc, p_RefMsgId, p_From, p_AttachmentPaths, p_Flowed));

  std::vector<Contact> hdrbcc;
  const std::string& header = GetHeader(p_Subject, p_To, p_Cc, hdrbcc, p_RefMsgId, p_From);
  const std::string& body = GetBody(p_Message, p_HtmlMessage, p_AttachmentPaths, p_Flowed);
  const std::string& data = header + body;
  p_ResultMessage = data;
  std::vector<Contact> recipients;
  recipients.insert(recipients.end(), p_To.begin(), p_To.end());
  recipients.insert(recipients.end(), p_Cc.begin(), p_Cc.end());
  recipients.insert(recipients.end(), p_Bcc.begin(), p_Bcc.end());

  return SendMessage(data, recipients);
}

SmtpStatus Smtp::Send(const std::string& p_Data, const std::vector<Contact>& p_To,
                      const std::vector<Contact>& p_Cc, const std::vector<Contact>& p_Bcc)
{
  LOG_DEBUG_FUNC(STR());
  LOG_TRACE_FUNC(STR(p_Data, p_To, p_Cc, p_Bcc));

  std::vector<Contact> recipients;
  recipients.insert(recipients.end(), p_To.begin(), p_To.end());
  recipients.insert(recipients.end(), p_Cc.begin(), p_Cc.end());
  recipients.insert(recipients.end(), p_Bcc.begin(), p_Bcc.end());

  std::string dataNoBcc = RemoveBccHeader(p_Data);

  return SendMessage(dataNoBcc, recipients);
}

SmtpStatus Smtp::SendMessage(const std::string& p_Data, const std::vector<Contact>& p_Recipients)
{
  LOG_DEBUG_FUNC(STR());
  LOG_TRACE_FUNC(STR(p_Data, p_Recipients));

  const bool enableSsl = (m_Port == 465);
  const bool enableTls = !enableSsl;
  const bool enableEsmtp = true;
  const bool enableLmtp = !enableEsmtp;

  mailsmtp* smtp = LOG_IF_NULL(mailsmtp_new(0, NULL));
  if (smtp == NULL) return SmtpStatusFailed;

  if (Log::GetTraceEnabled())
  {
    mailsmtp_set_logger(smtp, Logger, NULL);
  }

  mailsmtp_set_timeout(smtp, m_Timeout);

  int rv = MAILSMTP_NO_ERROR;
  if (enableSsl)
  {
    rv = LOG_IF_SMTP_ERR(mailsmtp_ssl_connect(smtp, m_Host.c_str(), m_Port));
  }
  else
  {
    rv = LOG_IF_SMTP_ERR(mailsmtp_socket_connect(smtp, m_Host.c_str(), m_Port));
  }

  if (rv != MAILSMTP_NO_ERROR) return SmtpStatusConnFailed;

  bool esmtpMode = false;
  const std::string& hostname = Util::GetSenderHostname();
  if (enableLmtp)
  {
    rv = LOG_IF_SMTP_ERR(mailesmtp_lhlo(smtp, hostname.c_str()));
  }
  else if (enableEsmtp && (rv = LOG_IF_SMTP_ERR(mailesmtp_ehlo(smtp))) == MAILSMTP_NO_ERROR)
  {
    esmtpMode = true;
  }
  else if (!enableEsmtp || (rv == MAILSMTP_ERROR_NOT_IMPLEMENTED))
  {
    rv = LOG_IF_SMTP_ERR(mailsmtp_helo(smtp));
  }

  if (rv != MAILSMTP_NO_ERROR) return SmtpStatusInitFailed;

  if (esmtpMode && enableTls)
  {
    rv = LOG_IF_SMTP_ERR(mailsmtp_socket_starttls(smtp));

    if (rv != MAILSMTP_NO_ERROR) return SmtpStatusInitFailed;

    if (enableLmtp)
    {
      rv = LOG_IF_SMTP_ERR(mailesmtp_lhlo(smtp, hostname.c_str()));
    }
    else if (enableEsmtp && ((rv = LOG_IF_SMTP_ERR(mailesmtp_ehlo(smtp))) == MAILSMTP_NO_ERROR))
    {
      esmtpMode = true;
    }
    else if (!enableEsmtp || rv == MAILSMTP_ERROR_NOT_IMPLEMENTED)
    {
      rv = LOG_IF_SMTP_ERR(mailsmtp_helo(smtp));
    }

    if (rv != MAILSMTP_NO_ERROR) return SmtpStatusInitFailed;
  }

  if (esmtpMode)
  {
    LOG_DEBUG("smtp->auth = 0x%x", smtp->auth);

    if (Auth::IsOAuthEnabled())
    {
      std::string token = Auth::GetAccessToken();
      rv = LOG_IF_SMTP_ERR(mailsmtp_oauth2_authenticate(smtp, m_User.c_str(), token.c_str()));
    }
    else
    {
      rv = LOG_IF_SMTP_ERR(mailsmtp_auth(smtp, m_User.c_str(), m_Pass.c_str()));
    }

    if (rv != MAILSMTP_NO_ERROR)
    {
      if (!Sasl::IsMechanismsSupported(smtp->auth))
      {
        LOG_ERROR("requested sasl auth mechanism not available, please ensure "
                  "libsasl2-modules or equivalent package is installed");
        return SmtpStatusSaslFailed;
      }

      return SmtpStatusAuthFailed;
    }
  }

  static int msgid = 0;
  std::string envid = std::to_string(++msgid) + std::string("@") + hostname;

  if (esmtpMode)
  {
    rv = LOG_IF_SMTP_ERR(mailesmtp_mail(smtp, m_Address.c_str(), 1, envid.c_str()));
  }
  else
  {
    rv = LOG_IF_SMTP_ERR(mailsmtp_mail(smtp, m_Address.c_str()));
  }

  if (rv != MAILSMTP_NO_ERROR) return SmtpStatusMessageFailed;

  clist* recipients = clist_new();
  for (auto& recipient : p_Recipients)
  {
    char* r = strdup(recipient.GetAddress().c_str());
    if (esmtpMode)
    {
      rv = LOG_IF_SMTP_ERR(mailesmtp_rcpt(smtp, r,
                                          MAILSMTP_DSN_NOTIFY_FAILURE | MAILSMTP_DSN_NOTIFY_DELAY,
                                          NULL));
    }
    else
    {
      rv = LOG_IF_SMTP_ERR(mailsmtp_rcpt(smtp, r));
    }

    if (rv != MAILSMTP_NO_ERROR) return SmtpStatusMessageFailed;

    clist_append(recipients, (void*)r);

    free(r);
  }

  rv = LOG_IF_SMTP_ERR(mailsmtp_data(smtp));

  if (rv != MAILSMTP_NO_ERROR) return SmtpStatusMessageFailed;

  if (enableLmtp)
  {
    int* retcodes = (int*)malloc((clist_count(recipients) * sizeof(int)));

    rv = LOG_IF_SMTP_ERR(maillmtp_data_message(smtp, p_Data.c_str(), p_Data.size(),
                                               recipients, retcodes));

    if (rv != MAILSMTP_NO_ERROR) return SmtpStatusMessageFailed;

    for (int i = 0; i < clist_count(recipients); i++)
    {
      LOG_WARNING("recipient \"%s\" returned %d", (char*)clist_nth_data(recipients, i),
                  retcodes[i]);
    }

    free(retcodes);
  }
  else
  {
    rv = LOG_IF_SMTP_ERR(mailsmtp_data_message(smtp, p_Data.c_str(), p_Data.size()));

    if (rv != MAILSMTP_NO_ERROR) return SmtpStatusMessageFailed;
  }

  clist_free(recipients);
  mailsmtp_free(smtp);

  LOG_DEBUG("send success");

  return SmtpStatusOk;
}

std::string Smtp::GetHeader(const std::string& p_Subject, const std::vector<Contact>& p_To,
                            const std::vector<Contact>& p_Cc, const std::vector<Contact>& p_Bcc,
                            const std::string& p_RefMsgId, const Contact& p_From)
{
  std::string fromname = MimeEncodeStr(p_From.GetName());
  std::string fromaddress = p_From.GetAddress();
  struct mailimf_mailbox* mbfrom = mailimf_mailbox_new(strdup(fromname.c_str()),
                                                       strdup(fromaddress.c_str()));
  struct mailimf_mailbox_list* mblistfrom = mailimf_mailbox_list_new_empty();
  mailimf_mailbox_list_add(mblistfrom, mbfrom);

  clist* tolist = clist_new();
  for (auto& to : p_To)
  {
    std::string toname = to.GetName();
    std::string tonamemime = MimeEncodeStr(toname);
    std::string toaddr = to.GetAddress();
    struct mailimf_mailbox* mbto = mailimf_mailbox_new(toname.empty()
                                                       ? NULL : strdup(tonamemime.c_str()),
                                                         strdup(toaddr.c_str()));
    struct mailimf_address* addrto = mailimf_address_new(MAILIMF_ADDRESS_MAILBOX, mbto, NULL);
    clist_append(tolist, addrto);
  }

  struct mailimf_address_list* addrlistto = mailimf_address_list_new(tolist);

  struct mailimf_address_list* addrlistcc = NULL;
  if (!p_Cc.empty())
  {
    clist* cclist = clist_new();
    for (auto& cc : p_Cc)
    {
      std::string ccname = cc.GetName();
      std::string ccnamemime = MimeEncodeStr(ccname);
      std::string ccaddr = cc.GetAddress();
      struct mailimf_mailbox* mbcc = mailimf_mailbox_new(ccname.empty()
                                                         ? NULL : strdup(ccnamemime.c_str()),
                                                           strdup(ccaddr.c_str()));
      struct mailimf_address* addrcc = mailimf_address_new(MAILIMF_ADDRESS_MAILBOX, mbcc, NULL);
      clist_append(cclist, addrcc);
    }

    addrlistcc = mailimf_address_list_new(cclist);
  }

  struct mailimf_address_list* addrlistbcc = NULL;
  if (!p_Bcc.empty())
  {
    clist* bcclist = clist_new();
    for (auto& bcc : p_Bcc)
    {
      std::string bccname = MimeEncodeStr(bcc.GetName());
      std::string bccaddr = bcc.GetAddress();
      struct mailimf_mailbox* mbbcc = mailimf_mailbox_new(strdup(bccname.c_str()),
                                                          strdup(bccaddr.c_str()));
      struct mailimf_address* addrbcc = mailimf_address_new(MAILIMF_ADDRESS_MAILBOX,
                                                            mbbcc, NULL);
      clist_append(bcclist, addrbcc);
    }

    addrlistbcc = mailimf_address_list_new(bcclist);
  }

  struct mailimf_address_list* addrlistreplyto = NULL;

  std::string subjectstr = MimeEncodeStr(p_Subject);
  char* subjectcstr = strdup(subjectstr.c_str());

  time_t now = time(NULL);
  struct mailimf_date_time* datenow = mailimf_get_date(now);

  const std::string msgId = GenerateMessageId();
  char* messageid = strdup(msgId.c_str());

  clist* clist_inreplyto = NULL;
  clist* clist_references = NULL;
  if (!p_RefMsgId.empty())
  {
    clist_inreplyto = clist_new();
    clist_append(clist_inreplyto, strdup(p_RefMsgId.c_str()));

    clist_references = clist_new();
    clist_append(clist_references, strdup(p_RefMsgId.c_str()));
  }

  struct mailimf_fields* fields =
    mailimf_fields_new_with_data_all(datenow,
                                     mblistfrom, NULL, addrlistreplyto,
                                     addrlistto, addrlistcc, addrlistbcc,
                                     messageid,
                                     clist_inreplyto, clist_references,
                                     subjectcstr);

  int col = 0;
  MMAPString* mmstr = mmap_string_new(NULL);
  mailimf_fields_write_mem(mmstr, &col, fields);
  std::string out = std::string(mmstr->str, mmstr->len);

  mmap_string_free(mmstr);
  mailimf_fields_free(fields);

  return out;
}

std::string Smtp::GetBody(const std::string& p_Message, const std::string& p_HtmlMessage,
                          const std::vector<std::string>& p_AttachmentPaths, bool p_Flowed)
{
  // html and text message part layout:
  // mainMultipart (content for message, subType="mixed")
  // ->htmlAndTextBodyPart (bodyPart1 for mainMultipart)
  // --->htmlAndTextMultipart (content for htmlAndTextBodyPart, subType="alternative")
  // ----->textBodyPart (bodyPart2 for the htmlAndTextMultipart)
  // ------->text (content for textBodyPart)
  // ----->htmlBodyPart (bodyPart1 for htmlAndTextMultipart)
  // ------->html (content for htmlBodyPart)
  // ->fileBodyPart1 (bodyPart2 for the mainMultipart)
  // --->FileDataHandler (content for fileBodyPart1 )

  struct mailmime* textBodyPart = GetMimeTextPart("text/plain", p_Message, p_Flowed);

  struct mailmime* bodyPart = nullptr;
  if (!p_HtmlMessage.empty())
  {
    struct mailmime* htmlAndTextMultipart =
      GetMimePart(mailmime_content_new_with_str("multipart/alternative"),
                  mailmime_fields_new_empty(), 0);
    struct mailmime* htmlBodyPart = GetMimeTextPart("text/html", p_HtmlMessage, false);
    mailmime_smart_add_part(htmlAndTextMultipart, textBodyPart);
    mailmime_smart_add_part(htmlAndTextMultipart, htmlBodyPart);
    bodyPart = htmlAndTextMultipart;
  }
  else
  {
    bodyPart = textBodyPart;
  }

  struct mailmime* mainMultipart = nullptr;
  const bool hasAttachment = !p_AttachmentPaths.empty();
  if (hasAttachment)
  {
    mainMultipart =
      GetMimePart(mailmime_content_new_with_str("multipart/mixed"),
                  mailmime_fields_new_empty(), 0);
    mailmime_smart_add_part(mainMultipart, bodyPart);

    for (auto& path : p_AttachmentPaths)
    {
      if (Util::Exists(path))
      {
        struct mailmime* fileBodyPart = GetMimeFilePart(path);
        mailmime_smart_add_part(mainMultipart, fileBodyPart);
        LOG_DEBUG("attachment path \"%s\" added", path.c_str());
      }
      else
      {
        LOG_DEBUG("attachment path \"%s\" does not exist", path.c_str());
      }
    }
  }
  else
  {
    mainMultipart = bodyPart;
  }

  struct mailmime* msg_mime = mailmime_new_message_data(NULL);
  mailmime_smart_add_part(msg_mime, mainMultipart);

  int col = 0;
  MMAPString* mmstr = mmap_string_new(NULL);
  mailmime_write_mem(mmstr, &col, mainMultipart);
  std::string out = std::string(mmstr->str, mmstr->len);

  mmap_string_free(mmstr);
  mailmime_free(msg_mime);

  return out;
}

std::string Smtp::GetErrorMessage(SmtpStatus p_SmtpStatus)
{
  std::string msg;
  switch (p_SmtpStatus)
  {
    case SmtpStatusOk:
      msg = "";
      break;

    case SmtpStatusFailed:
      msg = "unknown error";
      break;

    case SmtpStatusSaslFailed:
      msg = "no sasl mechs";
      break;

    case SmtpStatusAuthFailed:
      msg = "auth error";
      break;

    case SmtpStatusConnFailed:
      msg = "connect error";
      break;

    case SmtpStatusInitFailed:
      msg = "protocol error";
      break;

    case SmtpStatusMessageFailed:
      msg = "transfer error";
      break;

    default:
      break;
  }

  return msg;
}

mailmime* Smtp::GetMimeTextPart(const char* p_MimeType, const std::string& p_Message, bool p_Flowed)
{
  int encodingType = MAILMIME_MECHANISM_QUOTED_PRINTABLE;
  struct mailmime_mechanism* encoding = mailmime_mechanism_new(encodingType, NULL);
  struct mailmime_disposition* disposition =
    mailmime_disposition_new_with_data(MAILMIME_DISPOSITION_TYPE_INLINE, NULL, NULL,
                                       NULL, NULL, (size_t)-1);
  struct mailmime_fields* mimefields =
    mailmime_fields_new_with_data(encoding, NULL, NULL, disposition, NULL);
  struct mailmime_content* content = mailmime_content_new_with_str(p_MimeType);
  char* paramkey = strdup("charset");
  char* paramval = strdup("utf-8");
  struct mailmime_parameter* param = mailmime_param_new_with_data(paramkey, paramval);
  clist_append(content->ct_parameters, param);
  if (p_Flowed)
  {
    char* formatkey = strdup("format");
    char* formatval = strdup("flowed");
    struct mailmime_parameter* formatparam = mailmime_param_new_with_data(formatkey, formatval);
    clist_append(content->ct_parameters, formatparam);
  }
  struct mailmime* mime = GetMimePart(content, mimefields, 1);
  mailmime_set_body_text(mime, const_cast<char*>(p_Message.c_str()), p_Message.size());

  free(paramkey);
  free(paramval);

  return mime;
}

mailmime* Smtp::GetMimeFilePart(const std::string& p_Path, const std::string& p_MimeType)
{
  const std::string& filename = Util::BaseName(p_Path);
  char* dispositionname = strdup(filename.c_str());
  int encodingtype = MAILMIME_MECHANISM_BASE64;
  struct mailmime_disposition* disposition =
    mailmime_disposition_new_with_data(MAILMIME_DISPOSITION_TYPE_ATTACHMENT,
                                       dispositionname, NULL, NULL, NULL, (size_t)-1);
  struct mailmime_mechanism* encoding = mailmime_mechanism_new(encodingtype, NULL);
  struct mailmime_content* content = mailmime_content_new_with_str(p_MimeType.c_str());
  struct mailmime_fields* mimefields =
    mailmime_fields_new_with_data(encoding, NULL, NULL, disposition, NULL);
  struct mailmime* mime = GetMimePart(content, mimefields, 1);
  mailmime_set_body_file(mime, strdup(p_Path.c_str()));

  return mime;
}

mailmime* Smtp::GetMimePart(mailmime_content* p_Content, mailmime_fields* p_MimeFields,
                            bool p_ForceSingle)
{
  clist* list = NULL;
  int mime_type = -1;

  if (p_ForceSingle)
  {
    mime_type = MAILMIME_SINGLE;
  }
  else
  {
    switch (p_Content->ct_type->tp_type)
    {
      case MAILMIME_TYPE_DISCRETE_TYPE:
        mime_type = MAILMIME_SINGLE;
        break;

      case MAILMIME_TYPE_COMPOSITE_TYPE:
        switch (p_Content->ct_type->tp_data.tp_composite_type->ct_type)
        {
          case MAILMIME_COMPOSITE_TYPE_MULTIPART :
            mime_type = MAILMIME_MULTIPLE;
            break;

          case MAILMIME_COMPOSITE_TYPE_MESSAGE:
            if (strcasecmp(p_Content->ct_subtype, "rfc822") == 0)
            {
              mime_type = MAILMIME_MESSAGE;
            }
            else
            {
              mime_type = MAILMIME_SINGLE;
            }
            break;

          default:
            return NULL;
        }
        break;

      default:
        return NULL;
    }
  }

  if (mime_type == MAILMIME_MULTIPLE)
  {
    list = clist_new();
    if (list == NULL)
    {
      return NULL;
    }

    char* attr_name = strdup("boundary");
    char id[512];
    snprintf(id, sizeof(id), "%llx_%lx_%x", (long long)time(NULL), random(), getpid());

    char* attr_value = strdup(id);
    if (attr_name == NULL)
    {
      clist_free(list);
      free(attr_name);
      return NULL;
    }

    struct mailmime_parameter* param = mailmime_parameter_new(attr_name, attr_value);
    if (param == NULL)
    {
      clist_free(list);
      free(attr_value);
      free(attr_name);
      return NULL;
    }

    clist* parameters = NULL;
    if (p_Content->ct_parameters == NULL)
    {
      parameters = clist_new();
      if (parameters == NULL)
      {
        clist_free(list);
        mailmime_parameter_free(param);
        return NULL;
      }
    }
    else
    {
      parameters = p_Content->ct_parameters;
    }

    if (clist_append(parameters, param) != 0)
    {
      clist_free(list);
      clist_free(parameters);
      mailmime_parameter_free(param);
      return NULL;
    }

    if (p_Content->ct_parameters == NULL)
    {
      p_Content->ct_parameters = parameters;
    }
  }

  struct mailmime* mime = mailmime_new(mime_type, NULL, 0, p_MimeFields, p_Content,
                                       NULL, NULL, NULL, list, NULL, NULL);
  if (mime == NULL)
  {
    clist_free(list);
  }

  return mime;
}

std::string Smtp::MimeEncodeStr(const std::string& p_In)
{
  if (std::all_of(p_In.cbegin(), p_In.cend(), [](char c) { return isprint(c); }))
  {
    return p_In;
  }

  int col = 0;
  MMAPString* mmstr = mmap_string_new(NULL);
  mailmime_quoted_printable_write_mem(mmstr, &col, 1, p_In.c_str(), p_In.size());
  std::string enc = std::string(mmstr->str, mmstr->len);
  Util::ReplaceString(enc, "=\r\n", "");
  mmap_string_free(mmstr);
  return std::string("=?UTF-8?Q?") + enc + std::string("?=");
}

std::string Smtp::RemoveBccHeader(const std::string& p_Data)
{
  bool isHeader = true;
  bool isBcc = false;
  std::string line;
  std::ostringstream oss;
  std::istringstream iss(p_Data);
  while (std::getline(iss, line))
  {
    if (isHeader)
    {
      if (line.substr(0, 1) == "\r")
      {
        isHeader = false;
        isBcc = false;
      }
      else if (!isBcc)
      {
        if (line.substr(0, 5) == "Bcc: ")
        {
          isBcc = true;
        }
      }
      else
      {
        if ((line.length() > 0) && !isspace(line.at(0)))
        {
          isBcc = false;
        }
      }
    }

    if (!isBcc)
    {
      oss << line << "\n";
    }
  }

  return oss.str();
}

std::string Smtp::GenerateMessageId() const
{
  uuid_t uuid;
  char uuid_str[37];
  uuid_generate(uuid);
  uuid_unparse(uuid, uuid_str);
  return std::string(uuid_str) + "@" + Util::GetDomainName(m_Host);
}

void Smtp::Logger(mailsmtp* p_Smtp, int p_LogType, const char* p_Buffer, size_t p_Size,
                  void* p_UserData)
{
  if (p_LogType == MAILSTREAM_LOG_TYPE_DATA_SENT_PRIVATE) return; // dont log private data, like passwords

  (void)p_Smtp;
  (void)p_UserData;
  char* buffer = (char*)malloc(p_Size + 1);
  memcpy(buffer, p_Buffer, p_Size);
  buffer[p_Size] = 0;
  std::string str = Util::TrimRight(Util::Strip(std::string(buffer), '\r'), "\n");
  LOG_TRACE("smtp %i: %s", p_LogType, str.c_str());
  free(buffer);
}
