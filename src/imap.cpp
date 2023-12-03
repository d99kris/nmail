// imap.cpp
//
// Copyright (c) 2019-2023 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "imap.h"

#include "libetpan_help.h"
#include <libetpan/imapdriver_tools.h>
#include <libetpan/mailimap.h>

#include "auth.h"
#include "crypto.h"
#include "encoding.h"
#include "flag.h"
#include "imapcache.h"
#include "log.h"
#include "loghelp.h"
#include "lockfile.h"
#include "maphelp.h"
#include "sethelp.h"
#include "util.h"

Imap::Imap(const std::string& p_User, const std::string& p_Pass, const std::string& p_Host,
           const uint16_t p_Port, const int64_t p_Timeout,
           const bool p_CacheEncrypt, const bool p_CacheIndexEncrypt,
           const std::set<std::string>& p_FoldersExclude,
           const std::function<void(const StatusUpdate&)>& p_StatusHandler)
  : m_User(p_User)
  , m_Pass(p_Pass)
  , m_Host(p_Host)
  , m_Port(p_Port)
  , m_Timeout(p_Timeout)
  , m_CacheEncrypt(p_CacheEncrypt)
  , m_CacheIndexEncrypt(p_CacheIndexEncrypt)
  , m_FoldersExclude(p_FoldersExclude)
{
  if (Log::GetTraceEnabled())
  {
    LOG_TRACE_FUNC(STR(p_User, "***" /*p_Pass*/, p_Host, p_Port, p_CacheEncrypt));
  }
  else
  {
    LOG_DEBUG_FUNC(STR("***", "***" /*p_Pass*/, p_Host, p_Port, p_CacheEncrypt));
  }

  InitImap();

  m_ImapCache.reset(new ImapCache(m_CacheEncrypt, m_Pass));
  m_ImapIndex.reset(new ImapIndex(m_CacheIndexEncrypt, m_Pass, m_ImapCache, p_StatusHandler));
}

Imap::~Imap()
{
  LOG_DEBUG_FUNC(STR());

  m_ImapIndex.reset();
  m_ImapCache.reset();

  if (m_Aborting)
  {
    LOG_DEBUG("skip cleanup");
  }
  else
  {
    CleanupImap();
  }
}

void Imap::InitImap()
{
  m_Imap = LOG_IF_NULL(mailimap_new(0, NULL));

  if (Log::GetTraceEnabled())
  {
    mailimap_set_logger(m_Imap, Logger, NULL);
  }

  mailimap_set_timeout(m_Imap, m_Timeout);
}

void Imap::CleanupImap()
{
  if (m_Imap != NULL)
  {
    mailimap_free(m_Imap);
    m_Imap = NULL;
  }
}

bool Imap::Login()
{
  LOG_DEBUG_FUNC(STR());

  bool connected = false;
  bool isSSL = (m_Port == 993);
  bool isStartTLS = (m_Port == 143);

  {
    std::lock_guard<std::mutex> imapLock(m_ImapMutex);
    m_SelectedFolder.clear();

    int rv = 0;
    if (isSSL)
    {
      rv = LOG_IF_IMAP_ERR(mailimap_ssl_connect(m_Imap, m_Host.c_str(), m_Port));
    }
    else if (isStartTLS)
    {
      rv = LOG_IF_IMAP_ERR(mailimap_socket_connect(m_Imap, m_Host.c_str(), m_Port));
      if (rv == MAILIMAP_NO_ERROR_NON_AUTHENTICATED)
      {
        rv = LOG_IF_IMAP_ERR(mailimap_socket_starttls(m_Imap));
      }
    }
    else
    {
      rv = LOG_IF_IMAP_ERR(mailimap_socket_connect(m_Imap, m_Host.c_str(), m_Port));
    }

    if (rv == MAILIMAP_NO_ERROR_AUTHENTICATED)
    {
      connected = true;
    }
    else if ((rv == MAILIMAP_NO_ERROR_NON_AUTHENTICATED) || (isStartTLS && (rv == MAILIMAP_NO_ERROR)))
    {
      if (Auth::IsOAuthEnabled())
      {
        bool authResult = AuthRefresh();
        rv = (authResult ? MAILIMAP_NO_ERROR : MAILIMAP_ERROR_STREAM);
      }
      else
      {
        rv = LOG_IF_IMAP_ERR(mailimap_login(m_Imap, m_User.c_str(), m_Pass.c_str()));
      }

      connected = (rv == MAILIMAP_NO_ERROR);
    }
    else if (rv == MAILIMAP_ERROR_BAD_STATE)
    {
      LOG_WARNING("bad state reinit");

      CleanupImap();

      InitImap();
    }
  }

  {
    std::lock_guard<std::mutex> connectedLock(m_ConnectedMutex);
    m_Connected = connected;
  }

  if (connected)
  {
    // @todo: clear all cache if cannot use existing (cater for password change)
  }

  return connected;
}

bool Imap::Logout()
{
  LOG_DEBUG_FUNC(STR());

  std::lock_guard<std::mutex> connectedLock(m_ConnectedMutex);

  int rv = MAILIMAP_NO_ERROR;
  if (m_Connected)
  {
    std::lock_guard<std::mutex> imapLock(m_ImapMutex);
    if (m_Imap != NULL)
    {
      rv = LOG_IF_IMAP_LOGOUT_ERR(mailimap_logout(m_Imap));
    }
    m_SelectedFolder.clear();

    m_Connected = false;
  }

  return ((rv == MAILIMAP_NO_ERROR) || (rv == MAILIMAP_ERROR_STREAM));
}

bool Imap::AuthRefresh()
{
  LOG_DEBUG_FUNC(STR());

  if (!Auth::RefreshToken())
  {
    return false;
  }

  int rv = MAILIMAP_NO_ERROR;
  std::string token = Auth::GetAccessToken();
  rv = LOG_IF_IMAP_ERR(mailimap_oauth2_authenticate(m_Imap, m_User.c_str(), token.c_str()));

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::GetFolders(const bool p_Cached, std::set<std::string>& p_Folders)
{
  LOG_DEBUG_FUNC(STR(p_Cached, p_Folders));

  if (p_Cached)
  {
    p_Folders = m_ImapCache->GetFolders();
    return true;
  }

  clist* list = NULL;
  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  int rv = LOG_IF_IMAP_ERR(mailimap_list(m_Imap, "", "*", &list));
  if (rv == MAILIMAP_NO_ERROR)
  {
    for (clistiter* it = clist_begin(list); it != NULL; it = it->next)
    {
      struct mailimap_mailbox_list* mblist = (struct mailimap_mailbox_list*)clist_content(it);
      if (mblist && mblist->mb_name)
      {
        const std::string& folder = DecodeFolderName(std::string(mblist->mb_name));
        mailimap_mbx_list_flags* bflags = mblist->mb_flag;
        if (bflags && ((bflags->mbf_type == MAILIMAP_MBX_LIST_FLAGS_SFLAG) &&
                       (bflags->mbf_sflag == MAILIMAP_MBX_LIST_SFLAG_NOSELECT)))
        {
          // Skip Gmail root folder [Gmail]
          LOG_DEBUG("folder skip %s", folder.c_str());
          continue;
        }

        if (m_FoldersExclude.find(folder) == m_FoldersExclude.end())
        {
          LOG_DEBUG("folder include %s", folder.c_str());
          p_Folders.insert(folder);
        }
        else
        {
          LOG_DEBUG("folder exclude %s", folder.c_str());
        }
      }
    }

    mailimap_list_result_free(list);

    m_ImapCache->SetFolders(p_Folders);
    m_ImapIndex->SetFolders(p_Folders);

    return true;
  }

  return false;
}

bool Imap::GetUids(const std::string& p_Folder, const bool p_Cached, std::set<uint32_t>& p_Uids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Cached, p_Uids));

  if (p_Cached)
  {
    p_Uids = m_ImapCache->GetUids(p_Folder);
    return true;
  }

  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  if (!SelectFolder(p_Folder, true))
  {
    return false;
  }

  if (SelectedFolderIsEmpty())
  {
    m_ImapCache->SetUids(p_Folder, p_Uids);
    m_ImapIndex->SetUids(p_Folder, p_Uids);
    return true;
  }

  struct mailimap_set* set = mailimap_set_new_interval(1, 0);
  struct mailimap_fetch_type* fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
  mailimap_fetch_type_new_fetch_att_list_add(fetch_type, mailimap_fetch_att_new_uid());
  clist* fetch_result = NULL;

  int rv = LOG_IF_IMAP_ERR(mailimap_fetch(m_Imap, set, fetch_type, &fetch_result));
  if (rv == MAILIMAP_NO_ERROR)
  {
    for (clistiter* it = clist_begin(fetch_result); it != NULL; it = clist_next(it))
    {
      struct mailimap_msg_att* msg_att = (struct mailimap_msg_att*)clist_content(it);

      for (clistiter* ait = clist_begin(msg_att->att_list); ait != NULL; ait = clist_next(ait))
      {
        struct mailimap_msg_att_item* item = (struct mailimap_msg_att_item*)clist_content(ait);
        if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC) continue;

        if (item->att_data.att_static->att_type != MAILIMAP_MSG_ATT_UID) continue;

        p_Uids.insert(item->att_data.att_static->att_data.att_uid);
        break;
      }
    }

    mailimap_fetch_list_free(fetch_result);

    m_ImapCache->SetUids(p_Folder, p_Uids);
    m_ImapIndex->SetUids(p_Folder, p_Uids);
  }

  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::GetHeaders(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                      const bool p_Cached, const bool p_Prefetch,
                      std::map<uint32_t, Header>& p_Headers)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids, p_Cached, p_Prefetch, p_Headers));

  bool needFetch = false;
  struct mailimap_set* set = mailimap_set_new_empty();

  p_Headers = m_ImapCache->GetHeaders(p_Folder, p_Uids, p_Prefetch);

  if (!p_Cached)
  {
    std::set<uint32_t> uidsNotCached = p_Uids - MapKey(p_Headers);
    for (auto& uid : uidsNotCached)
    {
      mailimap_set_add_single(set, uid);
      needFetch = true;
    }
  }

  if (p_Prefetch)
  {
    // in prefetch mode the cache result is only used to indicate cache presence
    p_Headers.clear();
  }

  if (p_Cached)
  {
    mailimap_set_free(set);
    return true;
  }

  int rv = MAILIMAP_NO_ERROR;

  if (needFetch)
  {
    clist* fetch_result = NULL;
    std::map<uint32_t, Header> cacheHeaders;
    std::lock_guard<std::mutex> imapLock(m_ImapMutex);

    if (!SelectFolder(p_Folder))
    {
      mailimap_set_free(set);
      return false;
    }

    struct mailimap_fetch_type* fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, mailimap_fetch_att_new_rfc822_header());
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, mailimap_fetch_att_new_uid());
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, mailimap_fetch_att_new_internaldate());
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, mailimap_fetch_att_new_bodystructure());

    rv = LOG_IF_IMAP_ERR(mailimap_uid_fetch(m_Imap, set, fetch_type, &fetch_result));
    if (rv == MAILIMAP_NO_ERROR)
    {
      for (clistiter* it = clist_begin(fetch_result); it != NULL; it = clist_next(it))
      {
        struct mailimap_msg_att* msg_att = (struct mailimap_msg_att*)clist_content(it);

        std::string hdrData;
        std::string strData;
        uint32_t uid = 0;
        time_t time = 0;
        Header header;
        for (clistiter* ait = clist_begin(msg_att->att_list); ait != NULL; ait = clist_next(ait))
        {
          struct mailimap_msg_att_item* item = (struct mailimap_msg_att_item*)clist_content(ait);

          if (item->att_type == MAILIMAP_MSG_ATT_ITEM_DYNAMIC) continue;

          if (item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC)
          {
            if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_RFC822_HEADER)
            {
              hdrData = std::string(item->att_data.att_static->att_data.att_rfc822_header.att_content,
                                    item->att_data.att_static->att_data.att_rfc822_header.att_length);
            }
            else if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_UID)
            {
              uid = item->att_data.att_static->att_data.att_uid;
            }
            else if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_INTERNALDATE)
            {
              struct mailimap_date_time* datetime =
                item->att_data.att_static->att_data.att_internal_date;
              if (datetime != NULL)
              {
                struct mailimf_date_time imftime;
                Util::MailimapTimeToMailimfTime(datetime, &imftime);
                time = Util::MailtimeToTimet(&imftime);
              }
            }
            else if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_BODYSTRUCTURE)
            {
              struct mailmime* mime = NULL;
              if ((imap_body_to_body(item->att_data.att_static->att_data.att_bodystructure,
                                     &mime) == MAILIMAP_NO_ERROR) &&
                  (mime != NULL))
              {
                int col = 0;
                MMAPString* mmstr = mmap_string_new(NULL);
                mailmime_write_mem(mmstr, &col, mime);
                strData = std::string(mmstr->str, mmstr->len);
                mmap_string_free(mmstr);
              }
            }
          }
        }

        if (uid == 0)
        {
          LOG_WARNING("skip header uid = %d", uid);
          continue;
        }

        header.SetHeaderData(hdrData, strData, time);

        if (header.GetData().empty())
        {
          LOG_WARNING("skip header = \"\"");
          continue;
        }

        if (!p_Prefetch)
        {
          p_Headers[uid] = header;
        }

        cacheHeaders[uid] = header;
      }

      mailimap_fetch_list_free(fetch_result);
    }

    m_ImapCache->SetHeaders(p_Folder, cacheHeaders);

    mailimap_fetch_type_free(fetch_type);
  }

  mailimap_set_free(set);

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::GetFlags(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                    const bool p_Cached, std::map<uint32_t, uint32_t>& p_Flags)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids, p_Cached, p_Flags));

  if (p_Uids.empty())
  {
    return true;
  }

  if (p_Cached)
  {
    p_Flags = m_ImapCache->GetFlags(p_Folder, p_Uids);
    return true;
  }

  struct mailimap_set* set = mailimap_set_new_empty();
  for (auto& uid : p_Uids)
  {
    mailimap_set_add_single(set, uid);
  }

  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  if (!SelectFolder(p_Folder))
  {
    mailimap_set_free(set);
    return false;
  }

  struct mailimap_fetch_type* fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
  mailimap_fetch_type_new_fetch_att_list_add(fetch_type, mailimap_fetch_att_new_uid());
  mailimap_fetch_type_new_fetch_att_list_add(fetch_type, mailimap_fetch_att_new_flags());

  clist* fetch_result = NULL;

  int rv = LOG_IF_IMAP_ERR(mailimap_uid_fetch(m_Imap, set, fetch_type, &fetch_result));
  if (rv == MAILIMAP_NO_ERROR)
  {
    for (clistiter* it = clist_begin(fetch_result); it != NULL; it = clist_next(it))
    {
      struct mailimap_msg_att* msg_att = (struct mailimap_msg_att*)clist_content(it);

      uint32_t uid = 0;
      uint32_t flag = 0;
      for (clistiter* ait = clist_begin(msg_att->att_list); ait != NULL; ait = clist_next(ait))
      {
        struct mailimap_msg_att_item* item = (struct mailimap_msg_att_item*)clist_content(ait);

        if (item->att_type == MAILIMAP_MSG_ATT_ITEM_DYNAMIC)
        {
          if (item->att_data.att_dyn->att_list != NULL)
          {
            for (clistiter* dit = clist_begin(item->att_data.att_dyn->att_list); dit != NULL;
                 dit = clist_next(dit))
            {
              struct mailimap_flag_fetch* flag_fetch =
                (struct mailimap_flag_fetch*)clist_content(dit);
              if (flag_fetch && flag_fetch->fl_flag)
              {
                switch (flag_fetch->fl_flag->fl_type)
                {
                  case MAILIMAP_FLAG_SEEN:
                    flag |= Flag::Seen;
                    break;

                  default:
                    break;
                }
              }
            }
          }
        }
        else if (item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC)
        {
          if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_UID)
          {
            uid = item->att_data.att_static->att_data.att_uid;
          }
        }
      }

      if (uid == 0)
      {
        LOG_WARNING("skip flag uid = %d", uid);
        continue;
      }

      p_Flags[uid] = flag;
    }

    mailimap_fetch_list_free(fetch_result);

    m_ImapCache->SetFlags(p_Folder, p_Flags);
  }

  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::GetBodys(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                    const bool p_Cached, const bool p_Prefetch,
                    std::map<uint32_t, Body>& p_Bodys)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids, p_Cached, p_Prefetch, p_Bodys));

  bool needFetch = false;
  struct mailimap_set* set = mailimap_set_new_empty();

  p_Bodys = m_ImapCache->GetBodys(p_Folder, p_Uids, p_Prefetch);

  if (!p_Cached)
  {
    std::set<uint32_t> uidsNotCached = p_Uids - MapKey(p_Bodys);
    for (auto& uid : uidsNotCached)
    {
      mailimap_set_add_single(set, uid);
      needFetch = true;
    }
  }

  if (p_Prefetch)
  {
    // in prefetch mode the cache result is only used to indicate cache presence
    p_Bodys.clear();
  }

  if (p_Cached)
  {
    mailimap_set_free(set);
    return true;
  }

  int rv = MAILIMAP_NO_ERROR;

  if (needFetch)
  {
    std::lock_guard<std::mutex> imapLock(m_ImapMutex);

    if (!SelectFolder(p_Folder))
    {
      mailimap_set_free(set);
      return false;
    }

    struct mailimap_fetch_type* fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
    struct mailimap_fetch_att* body_att =
      mailimap_fetch_att_new_body_peek_section(mailimap_section_new(NULL));
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, body_att);
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, mailimap_fetch_att_new_uid());

    clist* fetch_result = NULL;
    std::map<uint32_t, Body> cacheBodys;

    rv = LOG_IF_IMAP_ERR(mailimap_uid_fetch(m_Imap, set, fetch_type, &fetch_result));
    if (rv == MAILIMAP_NO_ERROR)
    {
      for (clistiter* it = clist_begin(fetch_result); it != NULL; it = clist_next(it))
      {
        struct mailimap_msg_att* msg_att = (struct mailimap_msg_att*)clist_content(it);

        uint32_t uid = 0;
        Body body;
        for (clistiter* ait = clist_begin(msg_att->att_list); ait != NULL; ait = clist_next(ait))
        {
          struct mailimap_msg_att_item* item =
            (struct mailimap_msg_att_item*)clist_content(ait);

          if (item->att_type == MAILIMAP_MSG_ATT_ITEM_DYNAMIC) continue;

          if (item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC)
          {
            if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_BODY_SECTION)
            {
              std::string data(item->att_data.att_static->att_data.att_body_section->sec_body_part,
                               item->att_data.att_static->att_data.att_body_section->sec_length);
              body.SetData(data);
            }

            if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_UID)
            {
              uid = item->att_data.att_static->att_data.att_uid;
            }
          }
        }

        if (uid == 0)
        {
          LOG_WARNING("skip body uid = %d", uid);
          continue;
        }

        if (body.GetData().empty())
        {
          LOG_WARNING("skip body = \"\"");
          continue;
        }

        if (!p_Prefetch)
        {
          p_Bodys[uid] = body;
        }

        cacheBodys[uid] = body;
      }

      mailimap_fetch_list_free(fetch_result);
    }

    m_ImapCache->SetBodys(p_Folder, cacheBodys);
    m_ImapIndex->SetBodys(p_Folder, MapKey(cacheBodys));

    mailimap_fetch_type_free(fetch_type);
  }

  mailimap_set_free(set);

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::SetFlagSeen(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                       bool p_Value)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids, p_Value));

  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  if (!SelectFolder(p_Folder))
  {
    return false;
  }

  struct mailimap_flag_list* flaglist = mailimap_flag_list_new_empty();
  mailimap_flag_list_add(flaglist, mailimap_flag_new_seen());

  struct mailimap_set* set = mailimap_set_new_empty();
  for (auto& uid : p_Uids)
  {
    mailimap_set_add_single(set, uid);
  }

  struct mailimap_store_att_flags* storeflags = p_Value
    ? mailimap_store_att_flags_new_add_flags(flaglist) : mailimap_store_att_flags_new_remove_flags(flaglist);

  int rv = LOG_IF_IMAP_ERR(mailimap_uid_store(m_Imap, set, storeflags));

  if (storeflags != NULL)
  {
    mailimap_store_att_flags_free(storeflags);
  }

  mailimap_set_free(set);

  if (rv == MAILIMAP_NO_ERROR)
  {
    m_ImapCache->SetFlagSeen(p_Folder, p_Uids, p_Value);
  }

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::SetFlagDeleted(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                          bool p_Value)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids, p_Value));

  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  if (!SelectFolder(p_Folder))
  {
    return false;
  }

  struct mailimap_flag_list* flaglist = mailimap_flag_list_new_empty();
  mailimap_flag_list_add(flaglist, mailimap_flag_new_deleted());

  struct mailimap_set* set = mailimap_set_new_empty();
  for (auto& uid : p_Uids)
  {
    mailimap_set_add_single(set, uid);
  }

  struct mailimap_store_att_flags* storeflags = p_Value
    ? mailimap_store_att_flags_new_add_flags(flaglist) : mailimap_store_att_flags_new_remove_flags(flaglist);

  int rv = LOG_IF_IMAP_ERR(mailimap_uid_store(m_Imap, set, storeflags));

  mailimap_set_free(set);

  if (storeflags != NULL)
  {
    mailimap_store_att_flags_free(storeflags);
  }

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::MoveMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                        const std::string& p_DestFolder)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids, p_DestFolder));

  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  if (!SelectFolder(p_Folder))
  {
    return false;
  }

  struct mailimap_set* set = mailimap_set_new_empty();
  for (auto& uid : p_Uids)
  {
    mailimap_set_add_single(set, uid);
  }

  const std::string encDestFolder = EncodeFolderName(p_DestFolder);
  int rv = LOG_IF_IMAP_ERR(mailimap_uid_move(m_Imap, set, encDestFolder.c_str()));

  mailimap_set_free(set);

  if (rv == MAILIMAP_NO_ERROR)
  {
    m_ImapCache->DeleteMessages(p_Folder, p_Uids);
    m_ImapIndex->DeleteMessages(p_Folder, p_Uids);
  }

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::DeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Uids));

  bool rv = true;
  rv &= SetFlagDeleted(p_Folder, p_Uids, true);

  std::lock_guard<std::mutex> imapLock(m_ImapMutex);
  rv &= (LOG_IF_IMAP_ERR(mailimap_expunge(m_Imap)) == MAILIMAP_NO_ERROR);

  if (rv)
  {
    m_ImapCache->DeleteMessages(p_Folder, p_Uids);
    m_ImapIndex->DeleteMessages(p_Folder, p_Uids);
  }

  return rv;
}

bool Imap::CheckConnection()
{
  LOG_DEBUG_FUNC(STR());

  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  bool rv = true;
  rv &= (LOG_IF_IMAP_ERR(mailimap_noop(m_Imap)) == MAILIMAP_NO_ERROR);
  return rv;
}

bool Imap::GetConnected()
{
  std::lock_guard<std::mutex> connectedLock(m_ConnectedMutex);
  return m_Connected;
}

int Imap::IdleStart(const std::string& p_Folder)
{
  LOG_DEBUG_FUNC(STR(p_Folder));

  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  if (!SelectFolder(p_Folder))
  {
    return -1;
  }

  int rv = LOG_IF_IMAP_ERR(mailimap_idle(m_Imap));
  if (rv == MAILIMAP_NO_ERROR)
  {
    int fd = mailimap_idle_get_fd(m_Imap);
    m_ImapIndex->NotifyIdle(true);
    return fd;
  }

  return -1;
}

bool Imap::IdleDone()
{
  LOG_DEBUG_FUNC(STR());

  std::lock_guard<std::mutex> imapLock(m_ImapMutex);
  int rv = LOG_IF_IMAP_ERR(mailimap_idle_done(m_Imap));
  m_ImapIndex->NotifyIdle(false);
  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::UploadMessage(const std::string& p_Folder, const std::string& p_Msg, bool p_IsDraft)
{
  LOG_DEBUG_FUNC(STR(p_Folder, "***", p_IsDraft));
  LOG_TRACE_FUNC(STR(p_Folder, p_Msg, p_IsDraft));

  struct mailimap_flag_list* flaglist = mailimap_flag_list_new_empty();
  mailimap_flag_list_add(flaglist, mailimap_flag_new_seen());

  if (p_IsDraft)
  {
    mailimap_flag_list_add(flaglist, mailimap_flag_new_draft());
  }

  time_t nowtime = time(NULL);
  struct tm* lt = localtime(&nowtime);

  struct mailimap_date_time* datetime =
    mailimap_date_time_new(lt->tm_mday, (lt->tm_mon + 1), (lt->tm_year + 1900),
                           lt->tm_hour, lt->tm_min, lt->tm_sec, 0 /* dt_zone */);

  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  const std::string encFolder = EncodeFolderName(p_Folder);
  bool rv = (LOG_IF_IMAP_ERR(mailimap_append(m_Imap, encFolder.c_str(), flaglist, datetime,
                                             p_Msg.c_str(), p_Msg.size())) == MAILIMAP_NO_ERROR);

  mailimap_date_time_free(datetime);

  return rv;
}

void Imap::Search(const std::string& p_QueryStr, const unsigned p_Offset, const unsigned p_Max,
                  std::vector<Header>& p_Headers, std::vector<std::pair<std::string, uint32_t>>& p_FolderUids,
                  bool& p_HasMore)
{
  return m_ImapIndex->Search(p_QueryStr, p_Offset, p_Max, p_Headers, p_FolderUids, p_HasMore);
}

void Imap::SetAborting(bool p_Aborting)
{
  m_Aborting = p_Aborting;
}

void Imap::IndexNotifyIdle(bool p_IsIdle)
{
  m_ImapIndex->NotifyIdle(p_IsIdle);
}

bool Imap::SetBodysCache(const std::string& p_Folder, const std::map<uint32_t, Body>& p_Bodys)
{
  m_ImapCache->SetBodys(p_Folder, p_Bodys);
  m_ImapIndex->SetBodys(p_Folder, MapKey(p_Bodys));
  return true;
}

Imap::FolderInfo Imap::GetFolderInfo(const std::string& p_Folder)
{
  FolderInfo folderInfo;

  if (!SelectFolder(p_Folder))
  {
    return folderInfo;
  }

  struct mailimap_status_att_list* status_att_list =
    mailimap_status_att_list_new_empty();
  mailimap_status_att_list_add(status_att_list, MAILIMAP_STATUS_ATT_UNSEEN);
  mailimap_status_att_list_add(status_att_list, MAILIMAP_STATUS_ATT_MESSAGES);
  mailimap_status_att_list_add(status_att_list, MAILIMAP_STATUS_ATT_UIDNEXT);

  struct mailimap_mailbox_data_status* status = nullptr;

  int rv = LOG_IF_IMAP_ERR(mailimap_status(m_Imap, p_Folder.c_str(),
                                           status_att_list, &status));
  if ((rv == MAILIMAP_NO_ERROR) && (status != nullptr))
  {
    for (clistiter* it = clist_begin(status->st_info_list); it != nullptr;
         it = clist_next(it))
    {
      struct mailimap_status_info* status_info =
        (struct mailimap_status_info*)clist_content(it);

      switch (status_info->st_att)
      {
        case MAILIMAP_STATUS_ATT_MESSAGES:
          folderInfo.m_Count = status_info->st_value;
          break;

        case MAILIMAP_STATUS_ATT_UIDNEXT:
          folderInfo.m_NextUid = status_info->st_value;
          break;

        case MAILIMAP_STATUS_ATT_UNSEEN:
          folderInfo.m_Unseen = status_info->st_value;
          break;

        default:
          break;
      }
    }
  }

  if (status != nullptr)
  {
    mailimap_mailbox_data_status_free(status);
  }

  mailimap_status_att_list_free(status_att_list);

  return folderInfo;
}

bool Imap::SelectFolder(const std::string& p_Folder, bool p_Force)
{
  LOG_DEBUG_FUNC(STR(p_Folder, p_Force));

  if (p_Force || (p_Folder != m_SelectedFolder))
  {
    const std::string encFolder = EncodeFolderName(p_Folder);
    int rv = LOG_IF_IMAP_ERR(mailimap_select(m_Imap, encFolder.c_str()));
    if (rv == MAILIMAP_NO_ERROR)
    {
      m_SelectedFolder = p_Folder;
      m_SelectedFolderIsEmpty = (m_Imap->imap_selection_info->sel_has_exists == 1) &&
        (m_Imap->imap_selection_info->sel_exists == 0);

      const bool cachedUidValid = m_ImapCache->CheckUidValidity(p_Folder, GetUidValidity());
      if (!cachedUidValid)
      {
        LOG_DEBUG("delete and add folder %s", p_Folder.c_str());
        std::set<std::string> folders = m_ImapCache->GetFolders();
        std::set<std::string> tmpFolders = folders;
        tmpFolders.erase(p_Folder);
        m_ImapIndex->SetFolders(tmpFolders);
        m_ImapIndex->SetFolders(folders);
      }

      LOG_DEBUG("folder %s = %d", p_Folder.c_str(),
                (m_Imap->imap_selection_info->sel_has_exists == 1) ? m_Imap->imap_selection_info->sel_exists : -1);
    }

    return (rv == MAILIMAP_NO_ERROR);
  }
  else
  {
    return true;
  }
}

bool Imap::SelectedFolderIsEmpty()
{
  return m_SelectedFolderIsEmpty;
}

uint32_t Imap::GetUidValidity()
{
  return m_Imap->imap_selection_info->sel_uidvalidity;
}

std::string Imap::DecodeFolderName(const std::string& p_Folder)
{
  static std::map<std::string, std::string> cacheMap;
  std::map<std::string, std::string>::iterator it = cacheMap.find(p_Folder);
  if (it != cacheMap.end())
  {
    return it->second;
  }

  std::string decFolder = Encoding::ImapUtf7ToUtf8(p_Folder);
  cacheMap[p_Folder] = decFolder;

  return decFolder;
}

std::string Imap::EncodeFolderName(const std::string& p_Folder)
{
  static std::map<std::string, std::string> cacheMap;
  std::map<std::string, std::string>::iterator it = cacheMap.find(p_Folder);
  if (it != cacheMap.end())
  {
    return it->second;
  }

  const std::string encFolder = Encoding::Utf8ToImapUtf7(p_Folder);
  cacheMap[p_Folder] = encFolder;

  return encFolder;
}

void Imap::Logger(struct mailimap* p_Imap, int p_LogType, const char* p_Buffer, size_t p_Size, void* p_UserData)
{
  if (p_LogType == MAILSTREAM_LOG_TYPE_DATA_SENT_PRIVATE) return; // dont log private data, like passwords

  (void)p_Imap;
  (void)p_UserData;
  char* buffer = (char*)malloc(p_Size + 1);
  memcpy(buffer, p_Buffer, p_Size);
  buffer[p_Size] = 0;
  std::string str = Util::TrimRight(Util::Strip(std::string(buffer), '\r'), "\n");
  LOG_TRACE("imap %i: %s", p_LogType, str.c_str());
  free(buffer);
}
