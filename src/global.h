// simplewall
// Copyright (c) 2016-2025 Henry++

#pragma once

#include "routine.h"

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
#include <mscat.h>

#include "app.h"
#include "rapp.h"
#include "main.h"

#include "resource.h"

DECLSPEC_SELECTANY STATIC_DATA config = {0};
DECLSPEC_SELECTANY PROFILE_DATA profile_info = {0};

DECLSPEC_SELECTANY PR_HASHTABLE apps_table = NULL;
DECLSPEC_SELECTANY PR_LIST rules_list = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE rules_config = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE log_table = NULL;

DECLSPEC_SELECTANY PR_HASHTABLE cache_information = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE cache_resolution = NULL;

DECLSPEC_SELECTANY PR_HASHTABLE colors_table = NULL;

DECLSPEC_SELECTANY PR_ARRAY filter_ids = NULL;

DECLSPEC_SELECTANY R_QUEUED_LOCK lock_apps = PR_QUEUED_LOCK_INIT;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_apply = PR_QUEUED_LOCK_INIT;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_rules = PR_QUEUED_LOCK_INIT;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_rules_config = PR_QUEUED_LOCK_INIT;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_loglist = PR_QUEUED_LOCK_INIT;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_notify = PR_QUEUED_LOCK_INIT;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_profile = PR_QUEUED_LOCK_INIT;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_transaction = PR_QUEUED_LOCK_INIT;

DECLSPEC_SELECTANY R_QUEUED_LOCK lock_cache_information = PR_QUEUED_LOCK_INIT;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_cache_resolution = PR_QUEUED_LOCK_INIT;

DECLSPEC_SELECTANY R_WORKQUEUE file_queue = {0};
DECLSPEC_SELECTANY R_WORKQUEUE log_queue = {0};
DECLSPEC_SELECTANY R_WORKQUEUE resolver_queue = {0};
DECLSPEC_SELECTANY R_WORKQUEUE resolve_notify_queue = {0};
DECLSPEC_SELECTANY R_WORKQUEUE wfp_queue = {0};

DECLSPEC_SELECTANY R_FREE_LIST context_free_list = {0};
DECLSPEC_SELECTANY R_FREE_LIST listview_free_list = {0};

// timers array
DECLSPEC_SELECTANY const LONG64 timer_array[] =
{
	2 * 60LL, // 2 min
	5 * 60LL, // 5 min
	10 * 60LL, // 10 min
	30 * 60LL, // 30 min
	1 * 3600LL, // 1 hour
	2 * 3600LL, // 2 hour
	4 * 3600LL, // 4 hour
	6 * 3600LL, // 6 hour
	12 * 3600LL, // 12 hour
	24 * 3600LL // 24 hour
};

// dropped events callback subscription (win7+)
#ifndef FWP_DIRECTION_IN
#define FWP_DIRECTION_IN 0x00003900L
#endif

#ifndef FWP_DIRECTION_OUT
#define FWP_DIRECTION_OUT 0x00003901L
#endif

#ifndef FWP_DIRECTION_FORWARD
#define FWP_DIRECTION_FORWARD 0x00003902L
#endif

#ifndef FWP_DIRECTION_FORWARD2
#define FWP_DIRECTION_FORWARD2 0x00003903L
#endif

#define WM_NOTIFICATION (WM_APP + 21)

#include "controls.h"
#include "db.h"
#include "editor.h"
#include "helper.h"
#include "icons.h"
#include "listview.h"
#include "log.h"
#include "messages.h"
#include "network.h"
#include "notifications.h"
#include "packages.h"
#include "profile.h"
#include "search.h"
#include "security.h"
#include "timer.h"
#include "uwp.h"
#include "wfp.h"
