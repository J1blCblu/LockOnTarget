// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Utilities/Structs.h"
#include "UObject/NoExportTypes.h"
#include "LockOnSubobjectBase.generated.h"

class ULockOnTargetComponent;

/**
 * Base LockOnTargetComponent subobject class.
 */
UCLASS(Abstract)
class LOCKONTARGET_API ULockOnSubobjectBase : public UObject
{
	GENERATED_BODY()

public:
	ULockOnSubobjectBase();

	UFUNCTION(BlueprintPure, meta = (BlueprintProtected))
	ULockOnTargetComponent* GetLockOn() const;

	virtual UWorld* GetWorld() const override;

private:
	/** Cached LockOnComponent for quick access. */
	mutable TWeakObjectPtr<ULockOnTargetComponent> CachedLockOn;
};
