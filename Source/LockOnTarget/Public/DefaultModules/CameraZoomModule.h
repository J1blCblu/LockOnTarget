// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnTargetModuleBase.h"
#include "Components/TimelineComponent.h"
#include "CameraZoomModule.generated.h"

class UCurveFloat;
class UCameraComponent;

/**
 * Smoothly changes the FOV of the active camera.
 * 
 * Note: For prototyping purposes only. Should be encapsulated into a custom camera system.
 */
UCLASS(Blueprintable)
class LOCKONTARGET_API UCameraZoomModule : public ULockOnTargetModuleBase
{
	GENERATED_BODY()

	//@TODO: Look at UCameraModifier.

public:

	UCameraZoomModule();

public:

	/** The FOV will be added as a delta. You can safely change the absolute FOV. */
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

	/** Sets a new ActiveCamera. The zoom value will be cleared on the previous camera. */
	UFUNCTION(BlueprintCallable, Category = "Camera Zoom")
	void SetActiveCamera(UCameraComponent* InActiveCamera);

protected:

	virtual void UpdateTimeline(float Value);
	void ClearZoomOnActiveCamera();

protected: /** Overrides */
	
	//ULockOnTargetModuleBase
	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void OnTargetLocked(UTargetComponent* Target, FName Socket) override;
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket) override;
	virtual void Update(float DeltaTime) override;
};
