// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/LockOnSubobjectBase.h"
#include "LockOnTargetComponent.h"


ULockOnSubobjectBase::ULockOnSubobjectBase()
	: CachedLockOn(nullptr)
{
}

ULockOnTargetComponent* ULockOnSubobjectBase::GetLockOn() const
{
	if (!CachedLockOn.IsValid())
	{
		CachedLockOn = CastChecked<ULockOnTargetComponent>(GetOuter());
	}

	return CachedLockOn.Get();
}

UWorld* ULockOnSubobjectBase::GetWorld() const
{
	return (GetOuter() && (!GIsEditor || GIsPlayInEditorWorld)) ? GetOuter()->GetWorld() : nullptr;
}
