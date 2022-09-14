// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/RotationModes/InterpolationMode.h"
#include "LazyInterpolationMode.generated.h"

/**
 * Lazy Interpolation Mode. Intended only for the ControlRotation.
 * The main advantage is that the camera doesn't shake in the free range when the captured socket is moving along a sinusoid.
 * 
 * Interpolates only if the angle between the Target and the current rotation is more then BeginInterpAngle.
 * Stops interpolation when the angle reaches StopInterpAngle.
 * Interpolation speed is "linearly interpolated" within SmoothingAngleRange.
 * 
 * @see URotationModeBase.
 */
UCLASS(Blueprintable)
class LOCKONTARGET_API ULazyInterpolationMode : public UInterpolationMode
{
	GENERATED_BODY()

public:
	ULazyInterpolationMode();

public:
	/**
	 * Angle beyond which the interpolation starts.
	 * Should be > StopInterpAngle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f, Units="deg"))
	float BeginInterpAngle;

	/**
	 * Angle reaching which the interpolation stops.
	 * Should be < BeginInterpAngle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f, Units="deg"))
	float StopInterpAngle;

	/**
	 * Angle range starts after the StopInterpAngle. e.g. if StopInterpAngle = 3.f and SmoothingAngleRange = 4.f then the range will be [3.f, 7.f].
	 * If the angle between the forward vector(camera/owner) and the vector to the Target = 5.f then the ratio will be (5.f - 3.f) / 4.f = 0.5f.
	 * This ratio will be clamped in [MinInterpSpeedRatio, 1.f] and then multiplied by the InterpSpeed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 1.f, ClampMax = 180.f, UIMin = 1.f, UIMax = 180.f, Units = "deg"))
	float SmoothingRange;

private:
	mutable bool bInterpolationInProgress;

public:
	/** URotationModeBase */
	virtual FRotator GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime) override;

protected:
	virtual bool CanInterpolate(float DeltaAngle) const;
	virtual float GetInterpolationSpeed(float DeltaAngle) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event) override;
#endif
};
