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
 * Interpolation speed "interpolates" in SmoothingAngleRange between the values of SmoothRangeRatioClamp.
 * 
 * GetRotation() should be overridden.
 * @see URotationModeBase.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget))
class LOCKONTARGET_API ULazyInterpolationMode : public UInterpolationMode
{
	GENERATED_BODY()

public:
	ULazyInterpolationMode();

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f, Units="deg"))
	float SmoothingAngleRange;
	
	/** 
	 * Minimum interpolation speed ratio. The full ratio will be [MinInterpSpeedRatio, 1.f].
	 * The closer the value is to 1.f, the sharper the interpolation will be in the SmoothingAngleRange.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 0.05f, ClampMax = 1.f, UIMin = 0.05f, UIMax = 1.f, Units="x"))
	float MinInterpSpeedRatio;

#if WITH_EDITORONLY_DATA

	/** Intended only for the ControlRotation. Not visualize correctly if Rotation actually clamped by PitchClamp. */
	UPROPERTY(EditAnywhere, Category = "Lazy Interpolation")
	bool bVisualizeOnControlRotation = false;
#endif

/*******************************************************************************************/
/*******************************  Override Methods   ***************************************/
/*******************************************************************************************/
public:
	/** URotationModeBase */
	virtual FRotator GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime) override;
	/** ~URotationModeBase */

protected:
	UFUNCTION(BlueprintNativeEvent)
	bool CanLazyInterpolate(const FRotator& NewRotation, const FRotator& CurrentRotation, float& InterpSpeed);

/*******************************************************************************************/
/*******************************  Native Methods   *****************************************/
/*******************************************************************************************/
private:
	bool bLazyInterpolationInProgress;

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event) override;
	void DrawDebugInfo() const;
#endif
};
