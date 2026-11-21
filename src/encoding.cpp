// encoding.cpp
//
// Copyright (c) 2021-2023 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "encoding.h"

#include <string>

#include <iconv.h>
#include <libetpan/charconv.h>
#include <magic.h>

#if defined(HAS_BUNDLED_MAGIC)
#include <vector>
#define ZLIB_CONST
#include <zlib.h>
#endif

#include "imapurl.h"

#include "log.h"
#include "loghelp.h"

#if defined(HAS_BUNDLED_MAGIC)
// Generated at configure time by CMakeLists.txt: gzip-compressed magic.mgc
// from the libmagic prefix the binary is statically linked against.
extern "C" const unsigned char g_MagicDbGz[];
extern "C" const size_t g_MagicDbGzSize;
extern "C" const size_t g_MagicDbSize;

static std::vector<unsigned char> InflateMagicDb()
{
  std::vector<unsigned char> db(g_MagicDbSize);
  z_stream zs = {};
  if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) return std::vector<unsigned char>();

  zs.next_in = g_MagicDbGz;
  zs.avail_in = static_cast<uInt>(g_MagicDbGzSize);
  zs.next_out = db.data();
  zs.avail_out = static_cast<uInt>(db.size());
  int rv = inflate(&zs, Z_FINISH);
  uLong totalOut = zs.total_out;
  inflateEnd(&zs);
  if ((rv != Z_STREAM_END) || (totalOut != db.size())) return std::vector<unsigned char>();

  return db;
}
#endif

void Encoding::ConvertToUtf8(const std::string& p_Enc, std::string& p_Str)
{
  std::string enc = p_Enc;
  if (enc == "utf-8") return;

  bool detected = false;
  if (enc.empty() || (enc == "binary"))
  {
    enc = Detect(p_Str);
    detected = true;
  }

  if (!enc.empty() && (enc != "binary"))
  {
    std::string newStr;
    bool rv = Convert(enc, "utf-8", p_Str, newStr);
    if (detected)
    {
      LOG_TRACE("conv \"%s\" inv, using \"%s\" %s",
                p_Enc.c_str(), enc.c_str(), rv ? "ok" : "nok");
    }

    if (!rv)
    {
      if (!detected)
      {
        enc = Detect(p_Str);
        if (enc != p_Enc)
        {
          rv = Convert(enc, "utf-8", p_Str, newStr);
          LOG_TRACE("conv \"%s\" err, using \"%s\" %s",
                    p_Enc.c_str(), enc.c_str(), rv ? "ok" : "nok");
        }
      }
    }

    p_Str = newStr;
  }
}

std::string Encoding::ImapUtf7ToUtf8(const std::string& p_Src)
{
  char* cdst = (char*)malloc(p_Src.size() * 2);
  MailboxToURL(cdst, p_Src.c_str(), 0);
  std::string dst(cdst);
  free(cdst);
  return dst;
}

std::string Encoding::Utf8ToImapUtf7(const std::string& p_Src)
{
  char* cdst = (char*)malloc(p_Src.size() * 2);
  URLtoMailbox(cdst, p_Src.c_str(), 0);
  std::string dst(cdst);
  free(cdst);
  return dst;
}

std::string Encoding::Detect(const std::string& p_Str)
{
  int flags = MAGIC_MIME_ENCODING;
  magic_t cookie = magic_open(flags);
  if (cookie == NULL) return "";

#if defined(HAS_BUNDLED_MAGIC)
  // Static builds use the embedded db, as libmagic's compiled-in default db
  // path refers to the build host. The db is inflated per call (~10 ms) to
  // avoid keeping ~10 MB resident, as this is only hit for parts lacking a
  // usable charset. The buffer must outlive the cookie (referenced in place).
  std::vector<unsigned char> db = InflateMagicDb();
  void* dbBufs[1] = { db.data() };
  size_t dbSizes[1] = { db.size() };
  bool loaded = !db.empty() && (magic_load_buffers(cookie, dbBufs, dbSizes, 1) == 0);
#else
  bool loaded = (magic_load(cookie, NULL) == 0);
#endif

  std::string mime;
  if (loaded)
  {
    const char* rv = magic_buffer(cookie, p_Str.c_str(), p_Str.size());
    if (rv != NULL)
    {
      mime = std::string(rv);
    }
  }
  else
  {
    const char* err = magic_error(cookie);
    LOG_WARNING("magic load failed: %s", (err != NULL) ? err : "unknown error");
  }

  magic_close(cookie);

  if (mime == "unknown-8bit")
  {
    mime = "iso-8859-1"; // @todo: consider making default 8bit fallback configurable
  }

  return mime;
}

bool Encoding::Convert(const std::string& p_SrcEnc, const std::string& p_DstEnc,
                       const std::string& p_SrcStr, std::string& p_DstStr)
{
  std::string str;
  char* convStr = NULL;
  size_t convLen = 0;
  errno = 0;
  bool rv = false;
  if ((charconv_buffer(p_DstEnc.c_str(), p_SrcEnc.c_str(), p_SrcStr.c_str(), p_SrcStr.size(),
                       &convStr, &convLen) == MAIL_CHARCONV_NO_ERROR) && (convStr != NULL))
  {
    rv = (errno == 0);
    p_DstStr = std::string(convStr, convLen);
    charconv_buffer_free(convStr);
  }
  else
  {
    p_DstStr = p_SrcStr;
  }

  return rv;
}
