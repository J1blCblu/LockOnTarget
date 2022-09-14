// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "TargetInfo.h"
#include "TargetingHelperComponent.h"

/**
 * FTargetInfo
 */

AActor* FTargetInfo::GetActor() const
{
	return IsValid(HelperComponent) ? HelperComponent->GetOwner() : nullptr;
}
