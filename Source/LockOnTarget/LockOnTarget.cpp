// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTarget.h"
#include "LockOnTargetDefines.h"

#include "Interfaces/IPluginManager.h"
#include "PluginDescriptor.h"

IMPLEMENT_MODULE(FLockOnTargetModule, LockOnTarget);

DEFINE_LOG_CATEGORY(LogLockOnTarget);

UE_TRACE_CHANNEL_DEFINE(LockOnTargetChannel);

void FLockOnTargetModule::StartupModule()
{
	LOG("LockOnTarget(v%s): The LockOnTarget channel can be used to enable profiling. Traces can be sorted by the LOT_ prefix.", *IPluginManager::Get().FindPlugin("LockOnTarget")->GetDescriptor().VersionName);
}
