// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "TargetHandlers/TargetHandlerBase.h"
#include "LockOnTargetDefines.h"
#include "LockOnTargetComponent.h"

UTargetHandlerBase::UTargetHandlerBase()
{
	//Do something.
}

FTargetInfo UTargetHandlerBase::FindTarget_Implementation(FVector2D PlayerInput)
{
	LOG_ERROR("Unimplemented method is called. FindTarget() must be overriden.");
	unimplemented();
	return FTargetInfo::NULL_TARGET;
}

void UTargetHandlerBase::CheckTargetState_Implementation(const FTargetInfo& Target, float DeltaTime)
{
	//Optional.
}

void UTargetHandlerBase::HandleTargetException_Implementation(const FTargetInfo& Target, ETargetExceptionType Exception)
{
	//Optional.
}

bool UTargetHandlerBase::IsTargetValid(const UTargetComponent* Target) const
{
	return GetLockOnTargetComponent() ? GetLockOnTargetComponent()->IsTargetValid(Target) : false;
}
