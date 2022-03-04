// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/RotationModes/RotationModeBase.h"
#include "InterpolationMode.generated.h"

/**
 * Interpolation Rotation Mode.
 * Will return interpolated rotation.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Within = LockOnTargetComponent)
class LOCKONTARGET_API UInterpolationMode : public URotationModeBase
{
	GENERATED_BODY()

public:
	UInterpolationMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (UIMin = 0.f, ClampMin = 0.f))
	float InterpolationSpeed;

	virtual FRotator GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime) override;
};
