// uuid.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uuid.h"

#if defined(__OpenBSD__) || defined(__FreeBSD__)
#include <cstdlib>
#include <uuid.h>
#else
#include <uuid/uuid.h>
#endif

std::string Uuid::GenerateUuid()
{
#if defined(__OpenBSD__) || defined(__FreeBSD__)
  uuid_t uuid;
  uint32_t status = 0;
  uuid_create(&uuid, &status);

  char* str = nullptr;
  uuid_to_string(&uuid, &str, &status);
  std::string result = (str != nullptr) ? std::string(str) : std::string();
  free(str);

  return result;
#else
  uuid_t uuid;
  char uuid_str[37];
  uuid_generate(uuid);
  uuid_unparse(uuid, uuid_str);
  return std::string(uuid_str);
#endif
}
