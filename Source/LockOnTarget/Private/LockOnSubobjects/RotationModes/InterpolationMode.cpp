// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.


#include "LockOnSubobjects/RotationModes/InterpolationMode.h"

UInterpolationMode::UInterpolationMode()
	: InterpolationSpeed(10.f)
{

}

FRotator UInterpolationMode::GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime)
{
	FRotator NewRotation = FRotationMatrix::MakeFromX(TargetLocation - InstigatorLocation).Rotator();
	NewRotation = FMath::RInterpTo(CurrentRotation, NewRotation, DeltaTime, InterpolationSpeed);
	UpdateRotationAxes(CurrentRotation, NewRotation);
	ClampPitch(NewRotation);

	return NewRotation;
}
