// simplewall
// Copyright (c) 2016-2021 Henry++

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
#include <wincodec.h>

#include "app.h"
#include "rapp.h"
#include "main.h"

#include "resource.h"

DECLSPEC_SELECTANY STATIC_DATA config;

DECLSPEC_SELECTANY PR_HASHTABLE apps_table = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE context_table = NULL;
DECLSPEC_SELECTANY PR_LIST rules_list = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE rules_config = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE network_table = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE log_table = NULL;

DECLSPEC_SELECTANY PR_HASHTABLE cache_dns = NULL;

DECLSPEC_SELECTANY PR_HASHTABLE colors_table = NULL;
DECLSPEC_SELECTANY PR_ARRAY timers = NULL;

DECLSPEC_SELECTANY PR_ARRAY filter_ids = NULL;

DECLSPEC_SELECTANY R_QUEUED_LOCK lock_apps;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_apply;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_context;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_rules;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_rules_config;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_loglist;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_network;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_profile;
DECLSPEC_SELECTANY R_QUEUED_LOCK lock_transaction;

DECLSPEC_SELECTANY R_QUEUED_LOCK lock_cache_dns;

DECLSPEC_SELECTANY R_WORKQUEUE file_queue;
DECLSPEC_SELECTANY R_WORKQUEUE log_queue;
DECLSPEC_SELECTANY R_WORKQUEUE wfp_queue;

// dropped events callback subscription (win7+)
#ifndef FWP_DIRECTION_IN
#define FWP_DIRECTION_IN 0x00003900L
#endif

#ifndef FWP_DIRECTION_OUT
#define FWP_DIRECTION_OUT 0x00003901L
#endif

#include "controls.h"
#include "editor.h"
#include "helper.h"
#include "log.h"
#include "messages.h"
#include "notifications.h"
#include "profile.h"
#include "security.h"
#include "timer.h"
#include "wfp.h"
