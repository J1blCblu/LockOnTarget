// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/RotationModes/InterpolationMode.h"

UInterpolationMode::UInterpolationMode()
	: InterpolationSpeed(10.f)
{
	//Do something.
}

FRotator UInterpolationMode::GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime)
{
	FRotator DesiredRotation = GetRotationUnclamped(InstigatorLocation, TargetLocation);
	ClampPitch(DesiredRotation);
	DesiredRotation = FMath::RInterpTo(CurrentRotation, DesiredRotation, DeltaTime, InterpolationSpeed);
	ApplyRotationAxes(CurrentRotation, DesiredRotation);

	return DesiredRotation;
}
