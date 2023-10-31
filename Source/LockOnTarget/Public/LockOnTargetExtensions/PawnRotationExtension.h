// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnTargetExtensions/LockOnTargetExtensionBase.h"
#include "PawnRotationExtension.generated.h"

class UPawnMovementComponent;

/**
 * Smoothly orients the owning APawn to face the Target using UPawnMovementComponent.
 */
UCLASS(Blueprintable, HideCategories = Tick)
class LOCKONTARGET_API UPawnRotationExtension : public ULockOnTargetExtensionBase
{
	GENERATED_BODY()

public:

	UPawnRotationExtension();

public:

	/** Change in rotation per second. Set a negative value for infinite rotation rate and instant turns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	float RotationRate;
	
public:

	UPawnMovementComponent* GetMovementComponent() const;
	float GetDeltaYaw(float DeltaTime);

protected: /** Overrides */

	//ULockOnTargetExtensionBase
	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void Update(float DeltaTime) override;
	virtual void OnTargetLocked(UTargetComponent* Target, FName Socket) override;
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket) override;
};
