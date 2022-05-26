// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/LockOnSubobjectBase.h"
#include "LockOnTargetComponent.h"

ULockOnTargetComponent* ULockOnSubobjectBase::GetLockOn() const
{
	return StaticCast<ULockOnTargetComponent*>(GetOuter());
}

UWorld* ULockOnSubobjectBase::GetWorld() const
{
	return (IsValid(GetOuter()) && (!GIsEditor || GIsPlayInEditorWorld)) ? GetOuter()->GetWorld() : nullptr;
}
