#pragma once

// Version information - single source of truth for .rc files
// This file is included by both precomp.h (for C++ code) and .rc files (for resources)

#define PSERV_VERSION_MAJOR 5
#define PSERV_VERSION_MINOR 0
#define PSERV_VERSION_PATCH 0
#define PSERV_VERSION_BUILD 0

// String version for display
#define PSERV_VERSION_STRING "5.0.0"

// RC version format: major,minor,patch,build
#define PSERV_VERSION_RC 5,0,0,0

// Copyright
#define PSERV_COPYRIGHT "Copyright (c) 2025 Gerson Kurz"

// Product and file names
#define PSERV_PRODUCT_NAME "pserv"
#define PSERV_FILE_DESCRIPTION_GUI "pserv - Windows System Administration Tool"
#define PSERV_FILE_DESCRIPTION_CONSOLE "pservc - pserv Console Interface"
#define PSERV_COMPANY_NAME "Gerson Kurz"
