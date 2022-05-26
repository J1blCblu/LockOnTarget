// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTarget.h"

#include "Interfaces/IPluginManager.h"
#include "PluginDescriptor.h"
#include "Engine/StreamableManager.h"

#define LOCTEXT_NAMESPACE "FLockOnTargetModule"

DEFINE_LOG_CATEGORY(LogLockOnTarget);

FString LOTGlobals::PluginVersion;

void FLockOnTargetModule::StartupModule()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("LockOnTarget");
	LOTGlobals::PluginVersion = Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : TEXT("unavailable");

	UE_LOG(LogLockOnTarget, Log, TEXT("LockOnTarget(v%s) module startup."), *LOTGlobals::PluginVersion);

#if LOC_INSIGHTS
	UE_LOG(LogLockOnTarget, Log, TEXT("LockOnTarget uses Unreal Insights."));
#endif
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLockOnTargetModule, LockOnTarget)
