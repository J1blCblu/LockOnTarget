// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetDev.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "GameplayDebugger/GameplayDebuggerCategory_LockOnTarget.h"
#endif //WITH_GAMEPLAY_DEBUGGER

IMPLEMENT_MODULE(FLockOnTargetDevModule, LockOnTargetDev)

void FLockOnTargetDevModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GDModule = IGameplayDebugger::Get();
	GDModule.RegisterCategory(TEXT("LockOnTarget"), IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_LockOnTarget::MakeInstance), EGameplayDebuggerCategoryState::Disabled);
	GDModule.NotifyCategoriesChanged();
#endif //WITH_GAMEPLAY_DEBUGGER
}

void FLockOnTargetDevModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable()) 
	{
		IGameplayDebugger& GDModule = IGameplayDebugger::Get();
		GDModule.UnregisterCategory(TEXT("LockOnTarget"));
		GDModule.NotifyCategoriesChanged();
	}
#endif //WITH_GAMEPLAY_DEBUGGER
}
