// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/TargetHandlers/TargetHandlerBase.h"
#include "Utilities/Structs.h"

FTargetInfo UTargetHandlerBase::FindTarget_Implementation()
{
	unimplemented();
	return {};
}

bool UTargetHandlerBase::SwitchTarget_Implementation(FTargetInfo& TargetInfo, FVector2D PlayerInput)
{
	unimplemented();
	TargetInfo.Reset();
	return false;
}

bool UTargetHandlerBase::CanContinueTargeting_Implementation()
{
	unimplemented();
	return false;
}

void UTargetHandlerBase::OnTargetLockedNative()
{
	OnTargetLocked();
}

void UTargetHandlerBase::OnTargetUnlockedNative()
{
	OnTargetUnlocked();
}
