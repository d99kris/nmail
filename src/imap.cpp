// imap.cpp
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "imap.h"

#include <libetpan/libetpan.h>

#include "aes.h"
#include "flag.h"
#include "log.h"
#include "loghelp.h"
#include "serialized.h"
#include "util.h"

Imap::Imap(const std::string &p_User, const std::string &p_Pass, const std::string &p_Host,
           const uint16_t p_Port, const bool p_CacheEncrypt)
  : m_User(p_User)
  , m_Pass(p_Pass)
  , m_Host(p_Host)
  , m_Port(p_Port)
  , m_CacheEncrypt(p_CacheEncrypt)
{
  m_Imap = mailimap_new(0, NULL);
  InitCacheDir();
}

Imap::~Imap()
{
  mailimap_free(m_Imap);
}

bool Imap::Login()
{
  bool connected = false;

  {
    std::lock_guard<std::mutex> imapLock(m_ImapMutex);
    m_SelectedFolder.clear();
    int rv = LOG_IF_IMAP_ERR(mailimap_ssl_connect(m_Imap, m_Host.c_str(), m_Port));

    if (rv == MAILIMAP_NO_ERROR_AUTHENTICATED)
    {
      connected = true;
    }
    else if (rv == MAILIMAP_NO_ERROR_NON_AUTHENTICATED)
    {
      rv = LOG_IF_IMAP_ERR(mailimap_login(m_Imap, m_User.c_str(), m_Pass.c_str()));
      connected = (rv == MAILIMAP_NO_ERROR);
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

bool Imap::GetFolders(const bool p_Cached, std::set<std::string>& p_Folders)
{
  if (p_Cached)
  {
    p_Folders = Deserialize<std::set<std::string>>(ReadCacheFile(GetFoldersCachePath()));
    return true;
  }

  clist* list = NULL;
  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  int rv = LOG_IF_IMAP_ERR(mailimap_list(m_Imap, "", "*", &list));
  if (rv == MAILIMAP_NO_ERROR)
  {
    for(clistiter* it = clist_begin(list); it != NULL; it = it->next)
    {
      struct mailimap_mailbox_list* mblist = (struct mailimap_mailbox_list*)clist_content(it);
      p_Folders.insert(std::string(mblist->mb_name));
    }

    mailimap_list_result_free(list);

    WriteCacheFile(GetFoldersCachePath(), Serialize(p_Folders));

    return true;
  }

  return false;
}

bool Imap::GetUids(const std::string &p_Folder, const bool p_Cached, std::set<uint32_t>& p_Uids)
{
  if (p_Cached)
  {
    p_Uids = Deserialize<std::set<uint32_t>>(ReadCacheFile(GetFolderUidsCachePath(p_Folder)));
    return true;
  }

  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  if (!SelectFolder(p_Folder, true))
  {
    return false;
  }

  if (SelectedFolderIsEmpty())
  {
    return true;
  }

  struct mailimap_set* set = mailimap_set_new_interval(1, 0);
  struct mailimap_fetch_type* fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
  mailimap_fetch_type_new_fetch_att_list_add(fetch_type, mailimap_fetch_att_new_uid());
  clist* fetch_result = NULL;
  
  int rv = LOG_IF_IMAP_ERR(mailimap_fetch(m_Imap, set, fetch_type, &fetch_result));
  if (rv == MAILIMAP_NO_ERROR)
  {
    for(clistiter* it = clist_begin(fetch_result); it != NULL; it = clist_next(it))
    {
      struct mailimap_msg_att* msg_att = (struct mailimap_msg_att*)clist_content(it);

      for(clistiter* ait = clist_begin(msg_att->att_list); ait != NULL; ait = clist_next(ait))
      {
        struct mailimap_msg_att_item* item = (struct mailimap_msg_att_item *)clist_content(ait);
        if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC) continue;

        if (item->att_data.att_static->att_type != MAILIMAP_MSG_ATT_UID) continue;

        p_Uids.insert(item->att_data.att_static->att_data.att_uid);
        break;
      }
    }

    mailimap_fetch_list_free(fetch_result);

    WriteCacheFile(GetFolderUidsCachePath(p_Folder), Serialize(p_Uids));
  }

  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);

  // @todo: delete any cached messages from folder that is no longer present in uids set

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::GetHeaders(const std::string &p_Folder, const std::set<uint32_t> &p_Uids,
                      const bool p_Cached, std::map<uint32_t, Header>& p_Headers)
{
  bool needFetch = false;
  struct mailimap_set* set = mailimap_set_new_empty();
  for (auto& uid : p_Uids)
  {
    const std::string& cachePath = GetHeaderCachePath(p_Folder, uid);
    if (Util::Exists(cachePath))
    {
      const std::string& cacheData = ReadCacheFile(cachePath);
      if (!cacheData.empty())
      {
        Header header;
        header.SetData(cacheData);
        Util::Touch(cachePath);
        p_Headers[uid] = header;
      }
    }
    else
    {
      if (!p_Cached)
      {
        mailimap_set_add_single(set, uid);
        needFetch = true;
      }
    }
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
    std::lock_guard<std::mutex> imapLock(m_ImapMutex);

    if (!SelectFolder(p_Folder))
    {
      mailimap_set_free(set);
      return false;
    }

    if (SelectedFolderIsEmpty())
    {
      mailimap_set_free(set);
      return true;
    }
    
    struct mailimap_fetch_type* fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type,
                                               mailimap_fetch_att_new_rfc822_header());
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, mailimap_fetch_att_new_uid());
    
    rv = LOG_IF_IMAP_ERR(mailimap_uid_fetch(m_Imap, set, fetch_type, &fetch_result));
    if (rv == MAILIMAP_NO_ERROR)
    {
      for(clistiter* it = clist_begin(fetch_result); it != NULL; it = clist_next(it))
      {
        struct mailimap_msg_att* msg_att = (struct mailimap_msg_att*)clist_content(it);

        uint32_t uid = 0;
        Header header;
        for(clistiter* ait = clist_begin(msg_att->att_list); ait != NULL; ait = clist_next(ait))
        {
          struct mailimap_msg_att_item* item = (struct mailimap_msg_att_item *)clist_content(ait);

          if (item->att_type == MAILIMAP_MSG_ATT_ITEM_DYNAMIC) continue;

          if (item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC)
          {
            if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_RFC822_HEADER)
            {
              std::string data(item->att_data.att_static->att_data.att_rfc822_header.att_content,
                               item->att_data.att_static->att_data.att_rfc822_header.att_length);
              header.SetData(data);
            }

            if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_UID)
            {
              uid = item->att_data.att_static->att_data.att_uid;
            }
          }
        }

        p_Headers[uid] = header;

        const std::string& cachePath = GetHeaderCachePath(p_Folder, uid);
        WriteCacheFile(cachePath, header.GetData());
      }
    
      mailimap_fetch_list_free(fetch_result);
    }

    mailimap_fetch_type_free(fetch_type);
  }

  mailimap_set_free(set);

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::GetFlags(const std::string &p_Folder, const std::set<uint32_t> &p_Uids,
                    const bool p_Cached, std::map<uint32_t, uint32_t>& p_Flags)
{
  if (p_Cached)
  {
    p_Flags = Deserialize<std::map<uint32_t, uint32_t>>(ReadCacheFile(GetFolderFlagsCachePath(p_Folder)));
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

  if (SelectedFolderIsEmpty())
  {
    mailimap_set_free(set);
    return true;
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
        struct mailimap_msg_att_item* item = (struct mailimap_msg_att_item *)clist_content(ait);

        if (item->att_type==MAILIMAP_MSG_ATT_ITEM_DYNAMIC)
        {
          if (item->att_data.att_dyn->att_list != NULL)
          {
            for (clistiter* dit = clist_begin(item->att_data.att_dyn->att_list); dit != NULL;
                 dit = clist_next(dit))
            {
              struct mailimap_flag_fetch* flag_fetch =
                (struct mailimap_flag_fetch*) clist_content(dit);
              if (flag_fetch)
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

      p_Flags[uid] = flag;
    }

    mailimap_fetch_list_free(fetch_result);

    std::map<uint32_t, uint32_t> newFlags = p_Flags;
    std::map<uint32_t, uint32_t> oldFlags;
    oldFlags = Deserialize<std::map<uint32_t, uint32_t>>(ReadCacheFile(GetFolderFlagsCachePath(p_Folder)));
    newFlags.insert(oldFlags.begin(), oldFlags.end());

    WriteCacheFile(GetFolderFlagsCachePath(p_Folder), Serialize(newFlags));
  }

  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::GetBodys(const std::string &p_Folder, const std::set<uint32_t> &p_Uids,
                    const bool p_Cached, std::map<uint32_t, Body>& p_Bodys)
{
  bool needFetch = false;
  struct mailimap_set* set = mailimap_set_new_empty();
  for (auto& uid : p_Uids)
  {
    const std::string& cachePath = GetBodyCachePath(p_Folder, uid);
    if (Util::Exists(cachePath))
    {
      const std::string& cacheData = ReadCacheFile(cachePath);
      if (!cacheData.empty())
      {
        Body body;
        body.SetData(cacheData);
        Util::Touch(cachePath);
        p_Bodys[uid] = body;
      }
    }
    else
    {
      if (!p_Cached)
      {
        mailimap_set_add_single(set, uid);
        needFetch = true;
      }
    }
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

    if (SelectedFolderIsEmpty())
    {
      mailimap_set_free(set);
      return true;
    }

    struct mailimap_fetch_type* fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
    struct mailimap_fetch_att* body_att =
        mailimap_fetch_att_new_body_peek_section(mailimap_section_new(NULL));
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, body_att);
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, mailimap_fetch_att_new_uid());

    clist* fetch_result = NULL;
    
    rv = LOG_IF_IMAP_ERR(mailimap_uid_fetch(m_Imap, set, fetch_type, &fetch_result));
    if (rv == MAILIMAP_NO_ERROR)
    {
      for(clistiter* it = clist_begin(fetch_result); it != NULL; it = clist_next(it))
      {
        struct mailimap_msg_att* msg_att = (struct mailimap_msg_att*)clist_content(it);

        uint32_t uid = 0;
        Body body;
        for(clistiter* ait = clist_begin(msg_att->att_list); ait != NULL; ait = clist_next(ait))
        {
          struct mailimap_msg_att_item* item =
            (struct mailimap_msg_att_item *) clist_content(ait);

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

        p_Bodys[uid] = body;

        const std::string& cachePath = GetBodyCachePath(p_Folder, uid);
        WriteCacheFile(cachePath, body.GetData());
      }

      mailimap_fetch_list_free(fetch_result);
    }
    mailimap_fetch_type_free(fetch_type);
  }

  mailimap_set_free(set);
  
  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::SetFlagSeen(const std::string &p_Folder, const std::set<uint32_t> &p_Uids,
                       bool p_Value)
{
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
    ? mailimap_store_att_flags_new_add_flags(flaglist)
    : mailimap_store_att_flags_new_remove_flags(flaglist);
  
  int rv = LOG_IF_IMAP_ERR(mailimap_uid_store(m_Imap, set, storeflags));

  if (storeflags != NULL)
  {
    mailimap_store_att_flags_free(storeflags);
  }

  mailimap_set_free(set);

  if (rv == MAILIMAP_NO_ERROR)
  {
    std::map<uint32_t, uint32_t> flags =
      Deserialize<std::map<uint32_t, uint32_t>>(ReadCacheFile(GetFolderFlagsCachePath(p_Folder)));
    for (auto& uid : p_Uids)
    {
      if (p_Value)
      {
        flags[uid] |= Flag::Seen;
      }
      else
      {
        flags[uid] &= !Flag::Seen;
      }
    }

    WriteCacheFile(GetFolderFlagsCachePath(p_Folder), Serialize(flags));
  }
  
  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::SetFlagDeleted(const std::string &p_Folder, const std::set<uint32_t> &p_Uids,
                          bool p_Value)
{
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
    ? mailimap_store_att_flags_new_add_flags(flaglist)
    : mailimap_store_att_flags_new_remove_flags(flaglist);
  
  int rv = LOG_IF_IMAP_ERR(mailimap_uid_store(m_Imap, set, storeflags));

  mailimap_set_free(set);
  
  if (storeflags != NULL)
  {
    mailimap_store_att_flags_free(storeflags);
  }

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::MoveMessages(const std::string &p_Folder, const std::set<uint32_t> &p_Uids,
                        const std::string &p_DestFolder)
{
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
  
  int rv = LOG_IF_IMAP_ERR(mailimap_uid_move(m_Imap, set, p_DestFolder.c_str()));

  mailimap_set_free(set);

  if (rv == MAILIMAP_NO_ERROR)
  {
    std::set<uint32_t> uids =
      Deserialize<std::set<uint32_t>>(ReadCacheFile(GetFolderUidsCachePath(p_Folder)));
    for (auto& uid : p_Uids)
    {
      uids.erase(uid);
    }
    
    WriteCacheFile(GetFolderUidsCachePath(p_Folder), Serialize(uids));
  }
  
  // @todo: delete cached message from source folder

  return (rv == MAILIMAP_NO_ERROR);
}

bool Imap::DeleteMessages(const std::string &p_Folder, const std::set<uint32_t> &p_Uids)
{
  bool rv = true;
  rv &= SetFlagDeleted(p_Folder, p_Uids, true);
  rv &= (LOG_IF_IMAP_ERR(mailimap_expunge(m_Imap)) == MAILIMAP_NO_ERROR);
  return rv;
}

bool Imap::GetConnected()
{
  std::lock_guard<std::mutex> connectedLock(m_ConnectedMutex);
  return m_Connected;
}

int Imap::IdleStart(const std::string& p_Folder)
{
  std::lock_guard<std::mutex> imapLock(m_ImapMutex);

  if (!SelectFolder(p_Folder))
  {
    return -1;
  }

  int rv = LOG_IF_IMAP_ERR(mailimap_idle(m_Imap));
  if (rv == MAILIMAP_NO_ERROR)
  {
    int fd = mailimap_idle_get_fd(m_Imap);
    return fd;
  }

  return -1;
}

void Imap::IdleDone()
{
  mailimap_idle_done(m_Imap);
}

bool Imap::SelectFolder(const std::string &p_Folder, bool p_Force)
{
  if (p_Force || (p_Folder != m_SelectedFolder))
  {
    int rv = LOG_IF_IMAP_ERR(mailimap_select(m_Imap, p_Folder.c_str()));
    if (rv == MAILIMAP_NO_ERROR)
    {
      m_SelectedFolder = p_Folder;
      m_SelectedFolderIsEmpty = (m_Imap->imap_selection_info->sel_exists == 0);
      InitFolderCacheDir(p_Folder);
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

std::string Imap::GetCacheDir()
{
  return Util::GetApplicationDir() + std::string("cache/");
}

void Imap::InitCacheDir()
{
  static const int version = 1;
  const std::string cacheDir = GetCacheDir();
  CommonInitCacheDir(cacheDir, version);
}

std::string Imap::GetImapCacheDir()
{
  return GetCacheDir() + std::string("imap/");
}

std::string Imap::GetFolderCacheDir(const std::string &p_Folder)
{
  if (m_CacheEncrypt)
  {
    // @todo: consider encrypting folder name instead of hashing
    return GetImapCacheDir() + Util::SHA256(p_Folder) + std::string("/");
  }
  else
  {
    return GetImapCacheDir() + Serialized::ToHex(p_Folder) + std::string("/");
  }
}

std::string Imap::GetFolderUidsCachePath(const std::string &p_Folder)
{
  return GetFolderCacheDir(p_Folder) + std::string("uids");
}

std::string Imap::GetFolderFlagsCachePath(const std::string &p_Folder)
{
  return GetFolderCacheDir(p_Folder) + std::string("flags");
}

std::string Imap::GetFoldersCachePath()
{
  return GetImapCacheDir() + std::string("folders");
}

std::string Imap::GetMessageCachePath(const std::string &p_Folder, uint32_t p_Uid,
                                      const std::string &p_Suffix)
{
  const std::string& path = GetFolderCacheDir(p_Folder) + std::string("/") +
    std::to_string(p_Uid) + p_Suffix;
  return path;
}

std::string Imap::GetHeaderCachePath(const std::string &p_Folder, uint32_t p_Uid)
{
  return GetMessageCachePath(p_Folder, p_Uid, ".hdr");
}

std::string Imap::GetBodyCachePath(const std::string &p_Folder, uint32_t p_Uid)
{
  return GetMessageCachePath(p_Folder, p_Uid, ".eml");
}

void Imap::InitFolderCacheDir(const std::string &p_Folder)
{
  static const int validity = GetUidValidity();
  const std::string folderCacheDir = GetFolderCacheDir(p_Folder);
  CommonInitCacheDir(folderCacheDir, validity);
}

void Imap::CommonInitCacheDir(const std::string &p_Dir, int p_Version)
{
  const std::string& dirVersionPath = p_Dir + "version";
  if (Util::Exists(p_Dir))
  {
    int dirVersion = -1;
    DeserializeFromFile(dirVersionPath, dirVersion);
    if (dirVersion != p_Version)
    {
      Util::RmDir(p_Dir);
      Util::MkDir(p_Dir);
      SerializeToFile(dirVersionPath, p_Version);
    }
  }
  else
  {
    Util::MkDir(p_Dir);
    SerializeToFile(dirVersionPath, p_Version);
  }
}

std::string Imap::ReadCacheFile(const std::string &p_Path)
{
  if (m_CacheEncrypt)
  {
    return AES::Decrypt(Util::ReadFile(p_Path), m_Pass);
  }
  else
  {
    return Util::ReadFile(p_Path);
  }
}

void Imap::WriteCacheFile(const std::string &p_Path, const std::string &p_Str)
{
  if (m_CacheEncrypt)
  {
    Util::WriteFile(p_Path, AES::Encrypt(p_Str, m_Pass));
  }
  else
  {
    Util::WriteFile(p_Path, p_Str);
  }
}
