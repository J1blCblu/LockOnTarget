// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnTargetExtensions/LockOnTargetExtensionBase.h"
#include "Camera/CameraModifier.h"
#include "CameraModifierExtension.generated.h"

/**
 * Smoothly changes the camera FOV.
 */
UCLASS(Blueprintable)
class LOCKONTARGET_API ULockOnTargetCameraModifier_Zoom : public UCameraModifier
{
	GENERATED_BODY()

public:

	ULockOnTargetCameraModifier_Zoom();

public:

	/** The maximum delta FOV to be applied. */
	UPROPERTY(EditDefaultsOnly, Category = "Zoom")
	float MaxDeltaFOV;

public: //Overrides

	virtual void ModifyCamera(float DeltaTime, FVector ViewLocation, FRotator ViewRotation, float FOV, FVector& NewViewLocation, FRotator& NewViewRotation, float& NewFOV);
};

/**
 * Adds the specified camera modifier to the PlayerCameraManager.
 * The camera modifier will be enabled while any Target is locked, and disabled otherwise.
 */
UCLASS(Blueprintable, HideCategories = Tick)
class LOCKONTARGET_API UCameraModifierExtension final : public ULockOnTargetExtensionBase
{
	GENERATED_BODY()

public:

	UCameraModifierExtension();

public:

	/** The class of camera modifier to apply. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera Modifier")
	TSubclassOf<UCameraModifier> CameraModifierClass;

protected:

	TWeakObjectPtr<UCameraModifier> CameraModifier;

protected: /** Overrides */
	
	//ULockOnTargetExtensionBase
	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void OnTargetLocked(UTargetComponent* Target, FName Socket) override;
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket) override;
};
