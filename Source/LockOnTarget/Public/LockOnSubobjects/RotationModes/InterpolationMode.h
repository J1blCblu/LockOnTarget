// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/RotationModes/RotationModeBase.h"
#include "InterpolationMode.generated.h"

/**
 * Interpolation Rotation Mode.
 * Returns the interpolated rotation.
 * 
 * GetRotation() should be overridden.
 * @see URotationModeBase.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget))
class LOCKONTARGET_API UInterpolationMode : public URotationModeBase
{
	GENERATED_BODY()

public:
	UInterpolationMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation", meta = (UIMin = 0.f, ClampMin = 0.f, Units="deg"))
	float InterpolationSpeed;

	/** URotationModeBase */
	virtual FRotator GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime) override;
};
