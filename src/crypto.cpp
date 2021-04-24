// crypto.cpp
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "crypto.h"

#include <cstring>
#include <fstream>

#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>

#include "loghelp.h"
#include "util.h"

void Crypto::Init()
{
  SSL_library_init();
  SSL_load_error_strings();
}

void Crypto::Cleanup()
{
}

std::string Crypto::GetVersion()
{
  return std::string(SSLeay_version(SSLEAY_VERSION));
}

std::string Crypto::AESEncrypt(const std::string& p_Plaintext, const std::string& p_Pass)
{
  unsigned char salt[8] = { 0 };
  RAND_bytes(salt, sizeof(salt));

  unsigned char key[32] = { 0 };
  unsigned char iv[32] = { 0 };
  EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt,
                 (unsigned char*)const_cast<char*>(p_Pass.c_str()), p_Pass.size(), 1, key, iv);

  std::string ciphertext;

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx != NULL)
  {
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) == 1)
    {
      int buflen = p_Plaintext.size() + 256;
      unsigned char* buf = (unsigned char*)calloc(buflen, 1);
      if (buf != NULL)
      {
        int len = 0;
        unsigned char* cipherbuf = buf + 16;
        if (EVP_EncryptUpdate(ctx, cipherbuf, &len,
                              (const unsigned char*)p_Plaintext.c_str(),
                              p_Plaintext.size()) == 1)
        {
          int cipherbuflen = len;
          if (EVP_EncryptFinal_ex(ctx, cipherbuf + len, &len) == 1)
          {
            cipherbuflen += len;
            buflen = cipherbuflen + 16;

            memcpy(buf, "Salted__", 8);
            memcpy(buf + 8, salt, 8);

            ciphertext = std::string((char*)buf, buflen);
          }
        }

        free(buf);
      }
    }

    EVP_CIPHER_CTX_free(ctx);
  }

  return ciphertext;
}

std::string Crypto::AESDecrypt(const std::string& p_Ciphertext, const std::string& p_Pass)
{
  if (p_Ciphertext.empty()) return std::string();

  unsigned char salt[8] = { 0 };
  unsigned char* ciphertext = (unsigned char*)const_cast<char*>(p_Ciphertext.c_str());
  int ciphertextlen = p_Ciphertext.size();
  if (strncmp((const char*)ciphertext, "Salted__", 8) == 0)
  {
    memcpy(salt, &ciphertext[8], 8);
    ciphertext += 16;
    ciphertextlen -= 16;
  }

  unsigned char key[32] = { 0 };
  unsigned char iv[32] = { 0 };
  EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt,
                 (unsigned char*)const_cast<char*>(p_Pass.c_str()), p_Pass.size(), 1, key, iv);

  std::string plaintext;
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx != NULL)
  {
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) == 1)
    {
      int len = 0;
      unsigned char* buf = (unsigned char*)calloc(ciphertextlen + 256, 1);
      if (buf != NULL)
      {
        if (EVP_DecryptUpdate(ctx, buf, &len, ciphertext, ciphertextlen) == 1)
        {
          int plaintextlen = len;
          if (EVP_DecryptFinal_ex(ctx, buf + len, &len) == 1)
          {
            plaintextlen += len;
            plaintext = std::string((char*)buf, plaintextlen);
          }
        }

        free(buf);
      }

    }

    EVP_CIPHER_CTX_free(ctx);
  }

  return plaintext;
}

std::string Crypto::SHA256(const std::string& p_Str)
{
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, p_Str.c_str(), p_Str.size());
  SHA256_Final(hash, &sha256);
  return Util::ToHex(std::string((char*)hash, SHA256_DIGEST_LENGTH));
}

bool Crypto::AESEncryptFile(const std::string& p_InPath, const std::string& p_OutPath, const std::string& p_Pass)
{
  unsigned char salt[8] = { 0 };
  RAND_bytes(salt, sizeof(salt));

  unsigned char key[32] = { 0 };
  unsigned char iv[32] = { 0 };
  EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, (unsigned char*)const_cast<char*>(p_Pass.c_str()),
                 p_Pass.size(), 1, key, iv);

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx != NULL)
  {
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) == 1)
    {
      // @todo: add file error handling
      std::ifstream inStream;
      inStream.open(p_InPath, std::ios::binary);

      inStream.seekg(0, std::ios::end);
      std::streamsize inFileRemainingLen = inStream.tellg();
      inStream.seekg(0, std::ios::beg);

      std::ofstream outStream;
      outStream.open(p_OutPath, std::ios::binary);

      outStream.write("Salted__", 8);
      outStream.write((char*)salt, 8);

      const std::streamsize inBufLen = 64 * 1024;
      std::vector<char> inBuf(inBufLen);

      const std::streamsize outBufLen = inBufLen + EVP_CIPHER_CTX_block_size(ctx);
      std::vector<char> outBuf(outBufLen);

      while (inFileRemainingLen > 0)
      {
        std::streamsize readLen = std::min(inFileRemainingLen, inBufLen);
        inStream.read(inBuf.data(), readLen);

        int writeLen = 0;
        if (EVP_EncryptUpdate(ctx, (unsigned char*)outBuf.data(), &writeLen, (unsigned char*)inBuf.data(),
                              readLen) == 0)
        {
          EVP_CIPHER_CTX_free(ctx);
          return false;
        }

        outStream.write(outBuf.data(), writeLen);
        inFileRemainingLen -= readLen;
      }

      int writeLen = 0;
      if (EVP_EncryptFinal_ex(ctx, (unsigned char*)outBuf.data(), &writeLen) == 0)
      {
        EVP_CIPHER_CTX_free(ctx);
        return false;
      }

      outStream.write(outBuf.data(), writeLen);
    }
    else
    {
      return false;
    }

    EVP_CIPHER_CTX_free(ctx);
  }
  else
  {
    return false;
  }

  return true;
}

bool Crypto::AESDecryptFile(const std::string& p_InPath, const std::string& p_OutPath, const std::string& p_Pass)
{
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx != NULL)
  {
    // @todo: add file error handling
    std::ifstream inStream;
    inStream.open(p_InPath, std::ios::binary);

    inStream.seekg(0, std::ios::end);
    std::streamsize inFileRemainingLen = inStream.tellg();
    inStream.seekg(0, std::ios::beg);

    std::ofstream outStream;
    outStream.open(p_OutPath, std::ios::binary);

    const std::streamsize inBufLen = 64 * 1024;
    std::vector<char> inBuf(inBufLen);

    if (inFileRemainingLen < 16)
    {
      return false;
    }

    inStream.read(inBuf.data(), 8);
    inFileRemainingLen -= 8;
    if (strncmp((const char*)inBuf.data(), "Salted__", 8) != 0)
    {
      return false;
    }

    unsigned char salt[8] = { 0 };
    inStream.read((char*)salt, 8);
    inFileRemainingLen -= 8;

    unsigned char key[32] = { 0 };
    unsigned char iv[32] = { 0 };
    EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, (unsigned char*)const_cast<char*>(p_Pass.c_str()),
                   p_Pass.size(), 1, key, iv);

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) == 1)
    {
      const std::streamsize outBufLen = inBufLen + EVP_CIPHER_CTX_block_size(ctx);
      std::vector<char> outBuf(outBufLen);

      while (inFileRemainingLen > 0)
      {
        std::streamsize readLen = std::min(inFileRemainingLen, inBufLen);
        inStream.read(inBuf.data(), readLen);

        int writeLen = 0;
        if (EVP_DecryptUpdate(ctx, (unsigned char*)outBuf.data(), &writeLen, (unsigned char*)inBuf.data(),
                              readLen) == 0)
        {
          EVP_CIPHER_CTX_free(ctx);
          return false;
        }

        outStream.write(outBuf.data(), writeLen);
        inFileRemainingLen -= readLen;
      }

      int writeLen = 0;
      if (EVP_DecryptFinal_ex(ctx, (unsigned char*)outBuf.data(), &writeLen) == 0)
      {
        EVP_CIPHER_CTX_free(ctx);
        return false;
      }

      outStream.write(outBuf.data(), writeLen);
    }
    else
    {
      EVP_CIPHER_CTX_free(ctx);
      return false;
    }

    EVP_CIPHER_CTX_free(ctx);
  }
  else
  {
    return false;
  }

  return true;
}
