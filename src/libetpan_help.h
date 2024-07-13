// libetpan_help.h
//
// Copyright (c) 2023 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

// @todo: This is a temporary workaround to fix shadowing compilation warnings,
// to be removed eventually.

#ifdef __cplusplus
extern "C" {
#endif

struct _mailstream;
typedef struct _mailstream mailstream;
void mailstream_cancel(mailstream* s);

struct mailimap;
typedef struct mailimap mailimap;
struct mailimap_capability_data;
typedef struct mailimap_capability_data mailimap_capability_data;
int mailimap_capability(mailimap*, mailimap_capability_data**);

#ifdef __cplusplus
}
#endif
