// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/RotationModes/InterpolationMode.h"
#include "LazyInterpolationMode.generated.h"

/**
 * Lazy Interpolation Mode.
 * The main advantage is that the camera doesn't shake in free zone when the captured socket is moving along a sinusoid.
 * 
 * Interpolates only if Angle between Target and current rotation is more then BeginInterpAngle.
 * Stop Interpolation when Angle reaches StopInterpAngle.
 * Interpolation Speed "interpolates" in SmoothingAngleRange between SmoothRangeRatioClamp values.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Within = LockOnTargetComponent)
class LOCKONTARGET_API ULazyInterpolationMode : public UInterpolationMode
{
	GENERATED_BODY()

public:
	ULazyInterpolationMode();

	/**
	 * Angle beyond which interpolation starts.
	 * Should be > StopInterpAngle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f))
	float BeginInterpAngle;

	/**
	 * Angle reaching which interpolation stops.
	 * Should be < BeginInterpAngle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f))
	float StopInterpAngle;

	/**
	 * Range starts after StopInterpAngle. e.g. if StopInterpAngle = 3.f and SmoothingAngleRange = 4.f then range will be [3.f, 7.f].
	 * If Angle between Forward vector(camera/owner) and Vector to Target = 5.f then Ratio will be (5.f - 3.f) / 4.f = 0.5f.
	 * This ratio will be clamped by SmoothingRangeInterpSpeedSlamping and then multiplied by InterpSpeed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f))
	float SmoothingAngleRange;

#if WITH_EDITORONLY_DATA

	/** Intended only for Control rotation. Not work correctly if Rotation actually clamped by PitchClamp. */
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bVisualizeOnControlRotation = false;
#endif

/*******************************************************************************************/
/*******************************  Override Methods   ***************************************/
/*******************************************************************************************/
public:
	virtual FRotator GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime) override;

protected:
	UFUNCTION(BlueprintNativeEvent)
	bool CanLazyInterpolate(const FRotator& NewRotation, const FRotator& CurrentRotation, float& InterpSpeed);

/*******************************************************************************************/
/*******************************  Native Methods   *****************************************/
/*******************************************************************************************/
private:
	bool bLazyInterpolationInProgress;

#if WITH_EDITORONLY_DATA
	void DrawDebugInfo() const;
#endif
};
