// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

#include "Trace/Trace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLockOnTarget, All, All);

#define LOG(Str, ...) UE_LOG(LogLockOnTarget, Log, TEXT(Str), ##__VA_ARGS__)
#define LOG_WARNING(Str, ...) UE_LOG(LogLockOnTarget, Warning, TEXT("%s[%d]: " Str), *FString(__FUNCTION__), __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(Str, ...) UE_LOG(LogLockOnTarget, Error, TEXT("%s[%d]: " Str), *FString(__FUNCTION__), __LINE__, ##__VA_ARGS__)

#ifndef LOT_TRACE_ENABLED
#if UE_TRACE_ENABLED && !UE_BUILD_SHIPPING
#define LOT_TRACE_ENABLED 1
#else
#define LOT_TRACE_ENABLED 0
#endif
#endif

#if LOT_TRACE_ENABLED

//Custom channel declaration. -trace=default,LockOnTarget
UE_TRACE_CHANNEL_EXTERN(LockOnTargetChannel, LOCKONTARGET_API);

//Scoped event on the LockOnTarget channel.
#define LOT_SCOPED_EVENT(EventName) TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL(LOT_##EventName, LockOnTargetChannel)

#define LOT_BOOKMARK(Name, ...) if (UE_TRACE_CHANNELEXPR_IS_ENABLED(LockOnTargetChannel)) { TRACE_BOOKMARK(TEXT("LOT_" Name), ##__VA_ARGS__) }

#else

#define LOT_SCOPED_EVENT(...)
#define LOT_BOOKMARK(...)

#endif
