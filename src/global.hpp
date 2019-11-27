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

inline rapp app (APP_NAME, APP_NAME_SHORT, APP_VERSION, APP_COPYRIGHT);

inline STATIC_DATA config;

inline FWPM_SESSION session;

inline OBJECTS_MAP apps;
inline OBJECTS_MAP apps_helper;
inline OBJECTS_VEC rules_arr;
inline OBJECTS_MAP rules_config;
inline OBJECTS_MAP network_map;

inline OBJECTS_MAP cache_arpa;
inline OBJECTS_MAP cache_signatures;
inline OBJECTS_MAP cache_versions;
inline OBJECTS_MAP cache_dns;
inline OBJECTS_MAP cache_hosts;
inline TYPES_MAP cache_types;

inline THREADS_VEC threads_pool;

inline OBJECTS_VEC colors;
inline std::vector<time_t> timers;

inline GUIDS_VEC filter_ids;

inline ITEM_LIST_HEAD log_stack;

inline _R_FASTLOCK lock_access;
inline _R_FASTLOCK lock_apply;
inline _R_FASTLOCK lock_cache;
inline _R_FASTLOCK lock_checkbox;
inline _R_FASTLOCK lock_logbusy;
inline _R_FASTLOCK lock_logthread;
inline _R_FASTLOCK lock_transaction;
inline _R_FASTLOCK lock_writelog;

// dropped events callback subscription (win7+)
#ifndef FWP_DIRECTION_IN
#define FWP_DIRECTION_IN 0x00003900L
#endif

#ifndef FWP_DIRECTION_OUT
#define FWP_DIRECTION_OUT 0x00003901L
#endif
