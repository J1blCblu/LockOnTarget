// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTarget.h"
#include "LockOnTargetDefines.h"

#include "Interfaces/IPluginManager.h"
#include "PluginDescriptor.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY(LogLockOnTarget);
IMPLEMENT_MODULE(FLockOnTargetModule, LockOnTarget);

void FLockOnTargetModule::StartupModule()
{
	LOG("LockOnTarget(v%s) module startup.", *GetPluginVersion());

#if LOT_INSIGHTS
	LOG("LockOnTarget uses Unreal Insights. The LOT_ prefix can be used for sorting.");
#endif
}

FString FLockOnTargetModule::GetPluginVersion()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("LockOnTarget");
	return Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : FString(TEXT("unavailable"));
}
