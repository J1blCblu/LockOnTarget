// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/LockOnTargetModuleBase.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "CameraZoomModule.generated.h"

/**
 * Applies zoom to the camera when any Target is locked or unlocked.
 */
UCLASS(Blueprintable)
class LOCKONTARGET_API UCameraZoomModule : public ULockOnTargetModuleBase
{
	GENERATED_BODY()

public:
	UCameraZoomModule();

public:
	/** Camera FOV curve. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera Zoom", meta = (XAxisName = "Time", YAxisName = "FOV"))
	FRuntimeFloatCurve FieldOfViewCurve;

	/** Provide specific camera component by name. If None then the first camera from the component hierarchy will be used. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera Zoom")
	FName CameraName;

	/** If true an absolute FOV value from the curve will be applied to the camera. Otherwise as delta. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera Zoom")
	uint8 bUseAbsoluteValue : 1;

private:
	UPROPERTY(Transient)
	FTimeline Timeline;
	TWeakObjectPtr<class UCameraComponent> Camera;
	float DefaultFOV;

	virtual void UpdateTimeline(float Value);

protected: // ULockOnTargetModuleBase overrides
	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void OnTargetLocked(UTargetingHelperComponent* Target, FName Socket) override;
	virtual void OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket) override;
	virtual void UpdateOverridable(FVector2D PlayerInput, float DeltaTime) override;
};
