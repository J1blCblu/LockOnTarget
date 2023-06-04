// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogLockOnTarget, All, All);

#define LOG(Str, ...) UE_LOG(LogLockOnTarget, Log, TEXT(Str), ##__VA_ARGS__)
#define LOG_WARNING(Str, ...) UE_LOG(LogLockOnTarget, Warning, TEXT("%s[%d]: " Str), *FString(__FUNCTION__), __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(Str, ...) UE_LOG(LogLockOnTarget, Error, TEXT("%s[%d]: " Str), *FString(__FUNCTION__), __LINE__, ##__VA_ARGS__)

#if LOT_INSIGHTS
	#define LOT_SCOPED_EVENT(Name, Color) SCOPED_NAMED_EVENT(LOT_##Name, FColor::Color)
	#define LOT_BOOKMARK(Name, ...) TRACE_BOOKMARK(TEXT("LOT_" Name), ##__VA_ARGS__)
#else
	#define LOT_SCOPED_EVENT(Name, Color)
	#define LOT_BOOKMARK(Name, ...)
#endif
