// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/RotationModes/InterpolationMode.h"
#include "LazyInterpolationMode.generated.h"

/**
 * Lazy Interpolation Mode. Intended only for ControlRotation.
 * The main advantage is that the camera doesn't shake in free zone when the captured socket is moving along a sinusoid.
 * 
 * Interpolates only if Angle between Target and current rotation is more then BeginInterpAngle.
 * Stop Interpolation when Angle reaches StopInterpAngle.
 * Interpolation Speed "interpolates" in SmoothingAngleRange between SmoothRangeRatioClamp values.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget))
class LOCKONTARGET_API ULazyInterpolationMode : public UInterpolationMode
{
	GENERATED_BODY()

public:
	ULazyInterpolationMode();

	/**
	 * Angle beyond which interpolation starts.
	 * Should be > StopInterpAngle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f))
	float BeginInterpAngle;

	/**
	 * Angle reaching which interpolation stops.
	 * Should be < BeginInterpAngle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f))
	float StopInterpAngle;

	/**
	 * Range starts after StopInterpAngle. e.g. if StopInterpAngle = 3.f and SmoothingAngleRange = 4.f then range will be [3.f, 7.f].
	 * If Angle between Forward vector(camera/owner) and Vector to Target = 5.f then Ratio will be (5.f - 3.f) / 4.f = 0.5f.
	 * This ratio will be clamped by [MinInterpSpeedRatio, 1.f] and then multiplied by InterpSpeed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f))
	float SmoothingAngleRange;
	
	/** 
	 * Minimum interpolation speed ratio. Ratio will be [MinInterpSpeedRatio, 1.f].
	 * The closer the value is to 1.f, the sharper the interpolation will be in SmoothingAngleRange.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lazy Interpolation", meta = (ClampMin = 0.05f, ClampMax = 1.f, UIMin = 0.05f, UIMax = 1.f))
	float MinInterpSpeedRatio;

#if WITH_EDITORONLY_DATA

	/** Intended only for Control rotation. Not work correctly if Rotation actually clamped by PitchClamp. */
	UPROPERTY(EditAnywhere, Category = "Lazy Interpolation")
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
