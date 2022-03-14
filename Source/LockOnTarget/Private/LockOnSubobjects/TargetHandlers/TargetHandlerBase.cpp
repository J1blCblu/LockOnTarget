// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/TargetHandlers/TargetHandlerBase.h"
#include "Utilities/Structs.h"

UTargetHandlerBase::UTargetHandlerBase()
{

}

FTargetInfo UTargetHandlerBase::FindTarget_Implementation()
{
	unimplemented();
	return {};
}

bool UTargetHandlerBase::SwitchTarget_Implementation(FTargetInfo& TargetInfo, float PlayerInput)
{
	unimplemented();
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
