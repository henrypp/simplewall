// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <windns.h>
#include <mstcpip.h>
#include <windows.h>
#include <wincodec.h>
#include <netfw.h>
#include <iphlpapi.h>
#include <subauth.h>
#include <fwpmu.h>
#include <dbt.h>
#include <aclapi.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <softpub.h>
#include <algorithm>
#include <unordered_map>

#include "app.hpp"
#include "routine.hpp"
#include "rapp.hpp"
#include "main.hpp"

#include "helper.hpp"
#include "log.hpp"
#include "notifications.hpp"
#include "profile.hpp"
#include "timer.hpp"
#include "wfp.hpp"

#include "pugiconfig.hpp"
#include "..\..\pugixml\src\pugixml.hpp"

#include "resource.hpp"

extern rapp app;

extern STATIC_DATA config;

extern FWPM_SESSION session;

extern OBJECTS_MAP apps;
extern OBJECTS_MAP apps_helper;
extern OBJECTS_VEC rules_arr;
extern OBJECTS_MAP rules_config;
extern OBJECTS_MAP network_map;

extern OBJECTS_MAP cache_arpa;
extern OBJECTS_MAP cache_signatures;
extern OBJECTS_MAP cache_versions;
extern OBJECTS_MAP cache_dns;
extern OBJECTS_MAP cache_hosts;
extern TYPES_MAP cache_types;

extern THREADS_VEC threads_pool;

extern OBJECTS_VEC colors;
extern std::vector<time_t> timers;

extern GUIDS_VEC filter_ids;

extern ITEM_LIST_HEAD log_stack;

extern _R_FASTLOCK lock_access;
extern _R_FASTLOCK lock_apply;
extern _R_FASTLOCK lock_cache;
extern _R_FASTLOCK lock_checkbox;
extern _R_FASTLOCK lock_logbusy;
extern _R_FASTLOCK lock_logthread;
extern _R_FASTLOCK lock_threadpool;
extern _R_FASTLOCK lock_transaction;
extern _R_FASTLOCK lock_writelog;

// dropped events callback subscription (win7+)
#ifndef FWP_DIRECTION_IN
#define FWP_DIRECTION_IN 0x00003900L
#endif

#ifndef FWP_DIRECTION_OUT
#define FWP_DIRECTION_OUT 0x00003901L
#endif
