// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.
#include "LockOnTarget.h"

#define LOCTEXT_NAMESPACE "FLockOnTargetModule"

DEFINE_LOG_CATEGORY(Log_LOC);

void FLockOnTargetModule::StartupModule()
{

#if LOC_INSIGHTS
	UE_LOG(Log_LOC, Log, TEXT("Lock on Target uses Unreal Insights."));
#endif

}

void FLockOnTargetModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLockOnTargetModule, LockOnTarget)