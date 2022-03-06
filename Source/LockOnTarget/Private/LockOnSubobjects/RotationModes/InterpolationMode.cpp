// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.


#include "LockOnSubobjects/RotationModes/InterpolationMode.h"

UInterpolationMode::UInterpolationMode()
	: InterpolationSpeed(10.f)
{

}

FRotator UInterpolationMode::GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime)
{
	FRotator NewRotation = GetClampedRotationToTarget(InstigatorLocation, TargetLocation);
	NewRotation = FMath::RInterpTo(CurrentRotation, NewRotation, DeltaTime, InterpolationSpeed);
	ApplyRotationAxes(CurrentRotation, NewRotation);

	return NewRotation;
}
