// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "TargetHandlers/TargetHandlerBase.h"
#include "LockOnTargetDefines.h"
#include "LockOnTargetComponent.h"

UTargetHandlerBase::UTargetHandlerBase()
{
	//Do something.
}

FFindTargetRequestResponse UTargetHandlerBase::FindTarget_Implementation(const FFindTargetRequestParams& RequestParams)
{
	LOG_WARNING("Unimplemented method is called. NULL_TARGET is returned.");
	return FFindTargetRequestResponse();
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
	check(GetLockOnTargetComponent());
	return GetLockOnTargetComponent()->IsTargetValid(Target);
}
