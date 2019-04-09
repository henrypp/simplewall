// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <windns.h>
#include <mstcpip.h>
#include <windows.h>
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

typedef std::vector<PITEM_APP> MFILTER_APPS;
typedef std::vector<PITEM_RULE> MFILTER_RULES;
typedef std::vector<HANDLE> MTHREADPOOL;
typedef std::unordered_map<size_t, ITEM_APP> MAPPS_MAP;
typedef std::unordered_map<size_t, LPWSTR> MCACHE_MAP;
typedef std::unordered_map<size_t, EnumRuleItemType> MCACHETYPES_MAP;

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

extern MAPPS_MAP apps;
extern std::vector<PITEM_RULE> rules_arr;
extern std::unordered_map<size_t, PITEM_RULE_CONFIG> rules_config;
extern std::vector<PITEM_NETWORK> network_arr;

extern MCACHE_MAP cache_signatures;
extern MCACHE_MAP cache_versions;
extern MCACHE_MAP cache_dns;
extern MCACHE_MAP cache_hosts;
extern MCACHETYPES_MAP cache_types;

extern MTHREADPOOL threads_pool;

extern std::vector<PITEM_COLOR> colors;
extern std::vector<PITEM_PROTOCOL> protocols;
extern std::vector<PITEM_ADD> items;
extern std::vector<time_t> timers;

extern std::vector<PITEM_LOG> notifications;

extern MARRAY filter_ids;

extern ITEM_LIST_HEAD log_stack;

extern _R_FASTLOCK lock_access;
extern _R_FASTLOCK lock_apply;
extern _R_FASTLOCK lock_cache;
extern _R_FASTLOCK lock_checkbox;
extern _R_FASTLOCK lock_logbusy;
extern _R_FASTLOCK lock_logthread;
extern _R_FASTLOCK lock_network;
extern _R_FASTLOCK lock_notification;
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
