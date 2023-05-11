// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnTargetModuleBase.h"
#include "RotationModules.generated.h"

/**
 * Orients the owning AActor to face the Target.
 * 
 * Note: For prototyping purposes only. Should be encapsulated into a custom movement system.
 */
UCLASS(Blueprintable)
class LOCKONTARGET_API UActorRotationModule : public ULockOnTargetModuleBase
{
	GENERATED_BODY()

public:

	UActorRotationModule();

public:

	/** Rotation interpolation speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation", meta = (UIMin = 0.f, ClampMin = 0.f, Units = "deg"))
	float InterpolationSpeed;

private:

	uint8 bIsActive : 1;

public:

	/** Returns the final rotation to be applied. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Rotation Module")
	FRotator GetRotation(const FRotator& CurrentRotation, const FVector& ViewLocation, const FVector& TargetLocation, float DeltaTime);
	virtual FRotator GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& ViewLocation, const FVector& TargetLocation, float DeltaTime);

	/** Specifies the view location from which the direction to the Target will be calculated. */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Rotation Module")
	FVector GetViewLocation() const;
	virtual FVector GetViewLocation_Implementation() const;

	/** Has the module been activated. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetModule|Activation")
	bool IsRotationActive() const { return bIsActive; }

	/** Activates/deactivates the module. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetModule|Activation")
	void SetRotationIsActive(bool bInActive);

protected:

	FRotator GetRotationUnclamped(const FVector& From, const FVector& To) const;

protected: /** Overrides */

	//ULockOnTargetModuleProxy
	virtual void Update(float DeltaTime) override;
};

/**
 * Orients the owning Controller rotation to face the Target in such a way, 
 * that the rotation doesn't 'shake' in the free range when the captured socket moves along a sinusoid.
 * 
 * Note: For prototyping purposes only. Should be encapsulated into a custom camera system.
 */
UCLASS(Blueprintable)
class LOCKONTARGET_API UControllerRotationModule : public UActorRotationModule
{
	GENERATED_BODY()

public:

	UControllerRotationModule();

public:

	/** Offsets the resulting pitch value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation", meta = (UIMin = -45.f, ClampMin = -45.f, UIMax = 45.f, ClampMax = 45.f, Units = "deg"))
	float PitchOffset;

	/** Clamps the rotation pitch value. Where x - min value, y - max value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation", meta = (ClampMin = -90.f, ClampMax = 90.f, UIMin = -90.f, UIMax = 90.f))
	FVector2D PitchClamp;

	/** Starts interpolation when DeltaAngle exceeds this value. DeltaAngle - angle between new and current rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 0.f, ClampMax = 10, UIMin = 0.f, UIMax = 10.f, Units = "deg"))
	float BeginInterpAngle;

	/** Stops interpolation when DeltaAngle becomes less than BeginInterpAngle - StopInterpOffset. DeltaAngle - angle between new and current rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 0.f, ClampMax = 10.f, UIMin = 0.f, UIMax = 10.f, Units = "deg"))
	float StopInterpOffset;

	/** Smoothes the interpolation speed within this DeltaAngle range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 0.f, ClampMax = 30.f, UIMin = 0.f, UIMax = 30.f, Units = "deg"))
	float SmoothingRange;

private:

	mutable bool bInterpolationInProgress;

protected:

	bool CanInterpolate(float DeltaAngle) const;
	float GetInterpolationSpeed(float DeltaAngle) const;
	void ClampPitch(FRotator& Rotation) const;
	bool IsPitchClamped(const FRotator& Rotation) const;

protected: /** Overrides */

	//UActorRotationModule
	virtual void Update(float DeltaTime) override;
	virtual FVector GetViewLocation_Implementation() const override;
	virtual FRotator GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime) override;
};
