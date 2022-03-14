// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/RotationModes/InterpolationMode.h"
#include "LazyInterpolationModeRotator.generated.h"

/**
 * Experimental Lazy Interpolation Mode.
 */
UCLASS(ClassGroup = (LockOnTarget), HideDropdown)
class LOCKONTARGET_API ULazyInterpolationModeRotator : public UInterpolationMode
{
	GENERATED_BODY()

public:
	ULazyInterpolationModeRotator();

	/**
	 * Angle beyond which interpolation starts.
	 * Should be > StopAngle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FRotator StartRotator = {3.f, 5.f, 0.f};

	/**
	 * Angle reaching which interpolation stops.
	 * Should be < StartAngle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FRotator StopRotator = {1.5f, 3.f, 0.f};

	/**
	 * Range starts after StopAngle. e.g. if StopAngle = 3.f and SmoothingAngleRange = 4.f then range will be [3.f, 7.f].
	 * If Angle between Forward vector(camera/owner) and Vector to Target = 5.f then Ratio will be (5.f - 3.f) / 4.f = 0.5f.
	 * This ratio will be clamped by SmoothingRangeInterpSpeedSlamping and then multiplied by InterpSpeed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float SmoothingAngleRange = 8.f;

	/** Clamps ratio(read above field) which will be multiplied to InterpSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FVector2D SmoothRangeRatioClamp = FVector2D(0.2f, 1.f);

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bDebug = false;
#endif

/*******************************************************************************************/
/*******************************  Override Methods   ***************************************/
/*******************************************************************************************/
public:
	virtual FRotator GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime) override;

/*******************************************************************************************/
/*******************************  Native Methods   *****************************************/
/*******************************************************************************************/
private:
	mutable bool bLazyInterpolationInProgress;
	bool CanLazyInterpolate(const FRotator& NewRotation, const FRotator& CurrentRotation, float& InterpSpeed, uint8& RotFlags);

#if WITH_EDITORONLY_DATA
	void DrawDebugInfo() const;
#endif
};
