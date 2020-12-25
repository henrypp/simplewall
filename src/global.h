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

#include "config.h"
#include "..\..\mxml\mxml.h"

#include "resource.h"

DECLSPEC_SELECTANY STATIC_DATA config;

DECLSPEC_SELECTANY PR_HASHTABLE apps = NULL;
DECLSPEC_SELECTANY PR_ARRAY rules_arr = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE rules_config = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE network_map = NULL;
DECLSPEC_SELECTANY PR_LIST log_arr = NULL;

DECLSPEC_SELECTANY PR_HASHTABLE cache_dns = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE cache_hosts = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE cache_signatures = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE cache_types = NULL;
DECLSPEC_SELECTANY PR_HASHTABLE cache_versions = NULL;

DECLSPEC_SELECTANY PR_ARRAY colors = NULL;
DECLSPEC_SELECTANY PR_ARRAY timers = NULL;

DECLSPEC_SELECTANY PR_ARRAY filter_ids = NULL;

DECLSPEC_SELECTANY SLIST_HEADER log_list_stack;

DECLSPEC_SELECTANY R_SPINLOCK lock_apps;
DECLSPEC_SELECTANY R_SPINLOCK lock_rules;
DECLSPEC_SELECTANY R_SPINLOCK lock_apply;
DECLSPEC_SELECTANY R_SPINLOCK lock_checkbox;
DECLSPEC_SELECTANY R_SPINLOCK lock_logbusy;
DECLSPEC_SELECTANY R_SPINLOCK lock_logthread;
DECLSPEC_SELECTANY R_SPINLOCK lock_transaction;

// dropped events callback subscription (win7+)
#ifndef FWP_DIRECTION_IN
#define FWP_DIRECTION_IN 0x00003900L
#endif

#ifndef FWP_DIRECTION_OUT
#define FWP_DIRECTION_OUT 0x00003901L
#endif
