#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <inttypes.h>
#include <string>
#include <vector>

#pragma warning(disable : 4996)

#if !defined(FMT_HEADER_ONLY)
#define FMT_HEADER_ONLY
#include <fmt/format.h>
#endif

typedef unsigned long u_long;

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windef.h>
#include <winternl.h>
#include <TlHelp32.h>
#endif

#include "asm.h"

#if defined(HS_USE_SHARED_DEFS)
#include "shared/shared.h"
#endif

#include "sdk.h"
