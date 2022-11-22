// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/LockOnTargetModuleBase.h"
#include "Components/TimelineComponent.h"
#include "CameraZoomModule.generated.h"

class UCurveFloat;
class UCameraComponent;

/**
 * Applies zoom to the camera when any Target is locked or unlocked.
 * ActiveCamera FOV can be safely changed while zooming.
 * The module will clean up everything after being removed.
 */
UCLASS(Blueprintable)
class LOCKONTARGET_API UCameraZoomModule : public ULockOnTargetModuleBase
{
	GENERATED_BODY()

public:
	UCameraZoomModule();

public:

	//The FOV will be added as delta. You can safely change absolute FOV.
	//Note: There is no way to change this at runtime.
	UPROPERTY(EditAnywhere, Category = "Camera Zoom")
	TSoftObjectPtr<UCurveFloat> ZoomCurve;

private:

	UPROPERTY(Transient)
	FTimeline Timeline;
	
	//Cached active camera.
	TWeakObjectPtr<UCameraComponent> ActiveCamera;

	//The actual zoom value applied to the ActiveCamera.
	float CachedZoomValue;

public:

	/** Set a new ActiveCamera. Zoom will be cleared on the previous camera. */
	UFUNCTION(BlueprintCallable, Category = "Camera Zoom")
	void SetActiveCamera(UCameraComponent* InActiveCamera);

protected:
	virtual void UpdateTimeline(float Value);
	void ClearZoomOnActiveCamera();

protected: // ULockOnTargetModuleBase overrides
	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void OnTargetLocked(UTargetingHelperComponent* Target, FName Socket) override;
	virtual void OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket) override;
	virtual void UpdateOverridable(FVector2D PlayerInput, float DeltaTime) override;
};
