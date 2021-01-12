// sasl.cpp
//
// Copyright (c) 2020-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "sasl.h"

#include <sasl/sasl.h>

// Ideally Cyrus SASL should not be a direct dependency of nmail (it's an
// indirect dependency through libetpan), however there's been quite a bit
// of user-reported bugs with failure to authenticate SMTP due to missing
// the LOGIN mechanism. With this file we add direct interfacing with SASL
// to get a list of its installed mechanisms. The list retrieved and logged
// at nmail startup.

// Hack to disable warning about macOS deprecation of SASL functions.
#ifdef __APPLE__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

std::string Sasl::GetMechanisms()
{
  int rv = 0;

  rv = sasl_client_init(NULL);
  if (rv != SASL_OK)
  {
    return "";
  }

  std::string mechstr;
  const char** mechs = sasl_global_listmech();
  for (int i = 0; (mechs != NULL) && (mechs[i] != NULL); ++i)
  {
    mechstr += std::string((i == 0) ? "" : ", ") + std::string(mechs[i]);
  }

  rv = sasl_client_done();
  if (rv != SASL_OK)
  {
    return "";
  }

  return mechstr;
}

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif
