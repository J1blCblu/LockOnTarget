// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Trace/Trace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLockOnTarget, All, All);

#define LOG(Str, ...) UE_LOG(LogLockOnTarget, Log, TEXT(Str), ##__VA_ARGS__)
#define LOG_WARNING(Str, ...) UE_LOG(LogLockOnTarget, Warning, TEXT("%s[%d]: " Str), *FString(__FUNCTION__), __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(Str, ...) UE_LOG(LogLockOnTarget, Error, TEXT("%s[%d]: " Str), *FString(__FUNCTION__), __LINE__, ##__VA_ARGS__)

#if CPUPROFILERTRACE_ENABLED

//Custom channel declaration. -trace=default,LockOnTarget
UE_TRACE_CHANNEL_EXTERN(LockOnTargetChannel, LOCKONTARGET_API);

//Bookmark on the LockOnTarget channel.
#define LOT_BOOKMARK(Name, ...) if (UE_TRACE_CHANNELEXPR_IS_ENABLED(LockOnTargetChannel)) { TRACE_BOOKMARK(TEXT("LOT_" Name), ##__VA_ARGS__) }

//Scoped event on the LockOnTarget channel.
#define LOT_SCOPED_EVENT(EventName) TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL(LOT_##EventName, LockOnTargetChannel)

//Defines custom scoped event with single meta.
//#define LOT_DEFINE_CUSTOM_SCOPED_EVENT_WITH_META(EventName, MetaType, MetaParam) \
//	UE_TRACE_EVENT_BEGIN(Cpu, LOT_##EventName, NoSync) \
//	UE_TRACE_EVENT_FIELD(MetaType, MetaParam) \
//	UE_TRACE_EVENT_END()

//Custom scoped event with single meta defined via LOT_DEFINE_CUSTOM_EVENT_WITH_META() on the LockOnTarget channel.
//#define LOT_CUSTOM_SCOPED_EVENT_WITH_META(EventName, MetaParam, MetaValue) UE_TRACE_LOG_SCOPED_T(Cpu, LOT_##EventName, LockOnTargetChannel) << LOT_##EventName.MetaParam(MetaValue)

#else

#define LOT_BOOKMARK(...)
#define LOT_SCOPED_EVENT(...)
//#define LOT_DEFINE_CUSTOM_SCOPED_EVENT_WITH_META(...)
//#define LOT_CUSTOM_SCOPED_EVENT_WITH_META(...)

#endif
