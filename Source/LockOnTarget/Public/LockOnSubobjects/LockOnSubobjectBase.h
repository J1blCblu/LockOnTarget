// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "Engine/EngineBaseTypes.h"
#include "Utilities/Structs.h"
#include "UObject/NoExportTypes.h"
#include "LockOnSubobjectBase.generated.h"

class ULockOnTargetComponent;

/**
 * Base LockOnTargetComponent's subobject class.
 */
UCLASS(Abstract)
class LOCKONTARGET_API ULockOnSubobjectBase : public UObject
{
	GENERATED_BODY()

public:
	ULockOnSubobjectBase();

	UFUNCTION(BlueprintPure, Category = LockOnTarget, meta = (BlueprintProtected))
	ULockOnTargetComponent* GetLockOn() const;

	virtual UWorld* GetWorld() const override;

private:
	/** Cached LockOnComponent for the quick access. */
	mutable TWeakObjectPtr<ULockOnTargetComponent> CachedLockOn;
};
