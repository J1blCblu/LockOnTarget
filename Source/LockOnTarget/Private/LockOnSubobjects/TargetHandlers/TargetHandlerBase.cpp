// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/TargetHandlers/TargetHandlerBase.h"
#include "LockOnTargetDefines.h"

FTargetInfo UTargetHandlerBase::FindTarget_Implementation(FVector2D PlayerInput)
{
	LOG_ERROR("Unimplemented method is called.");
	unimplemented();
	return {};
}

bool UTargetHandlerBase::CanContinueTargeting_Implementation()
{
	LOG_ERROR("Unimplemented method is called.");
	unimplemented();
	return false;
}
