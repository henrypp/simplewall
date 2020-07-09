// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

#include "routine.hpp"

#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <windns.h>
#include <mstcpip.h>
#include <iphlpapi.h>
#include <aclapi.h>
#include <dbt.h>
#include <fwpmu.h>
#include <mmsystem.h>
#include <netfw.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <softpub.h>
#include <subauth.h>
#include <wincodec.h>

#include "app.hpp"
#include "rapp.hpp"
#include "main.hpp"

#include "controls.hpp"
#include "editor.hpp"
#include "helper.hpp"
#include "log.hpp"
#include "notifications.hpp"
#include "profile.hpp"
#include "security.hpp"
#include "timer.hpp"
#include "wfp.hpp"

#include "pugiconfig.hpp"
#include "..\..\pugixml\src\pugixml.hpp"

#include "resource.hpp"

inline rapp app;

inline STATIC_DATA config;

inline OBJECTS_APP_MAP apps;
inline OBJECTS_APP_HELPER_MAP apps_helper;
inline OBJECTS_RULE_VECTOR rules_arr;
inline OBJECTS_RULE_CONFIG_MAP rules_config;
inline OBJECTS_NETWORK_MAP network_map;
inline OBJECTS_LOG_VECTOR log_arr;

inline OBJECTS_STRINGS_MAP cache_arpa;
inline OBJECTS_STRINGS_MAP cache_signatures;
inline OBJECTS_STRINGS_MAP cache_versions;
inline OBJECTS_STRINGS_MAP cache_dns;
inline OBJECTS_STRINGS_MAP cache_hosts;
inline TYPES_MAP cache_types;

inline THREADS_VEC threads_pool;

inline std::vector<PITEM_COLOR> colors;
inline std::vector<time_t> timers;

inline GUIDS_VEC filter_ids;

inline ITEM_LIST_HEAD log_stack;

inline _R_FASTLOCK lock_apply;
inline _R_FASTLOCK lock_checkbox;
inline _R_FASTLOCK lock_logbusy;
inline _R_FASTLOCK lock_logthread;
inline _R_FASTLOCK lock_transaction;

// dropped events callback subscription (win7+)
#ifndef FWP_DIRECTION_IN
#define FWP_DIRECTION_IN 0x00003900L
#endif

#ifndef FWP_DIRECTION_OUT
#define FWP_DIRECTION_OUT 0x00003901L
#endif
