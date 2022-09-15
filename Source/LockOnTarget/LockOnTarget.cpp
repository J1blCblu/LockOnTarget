// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTarget.h"
#include "LockOnTargetDefines.h"

#include "Interfaces/IPluginManager.h"
#include "PluginDescriptor.h"
#include "Engine/StreamableManager.h"

#define LOCTEXT_NAMESPACE "FLockOnTargetModule"

DEFINE_LOG_CATEGORY(LogLockOnTarget);

void FLockOnTargetModule::StartupModule()
{
	LOG("LockOnTarget(v%s) module startup.", *GetPluginVersion());

#if LOT_INSIGHTS
	LOG("LockOnTarget uses Unreal Insights.");
#endif
}

FString FLockOnTargetModule::GetPluginVersion()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("LockOnTarget");
	return Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : FString(TEXT("unavailable"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLockOnTargetModule, LockOnTarget)
