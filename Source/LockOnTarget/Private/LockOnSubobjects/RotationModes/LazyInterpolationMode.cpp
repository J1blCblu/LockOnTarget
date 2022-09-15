// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/RotationModes/LazyInterpolationMode.h"
#include "LOT_Math.h"

constexpr float MinInterpSpeedRatio = 0.05f;

ULazyInterpolationMode::ULazyInterpolationMode()
	: BeginInterpAngle(3.7f)
	, StopInterpAngle(2.7f)
	, SmoothingRange(10.f)
	, bInterpolationInProgress(false)
{
	InterpolationSpeed = 20.f;
}

FRotator ULazyInterpolationMode::GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime)
{
	FRotator DesiredRotation = GetRotationUnclamped(InstigatorLocation, TargetLocation);
	const float DeltaAngle = LOT_Math::GetAngle(DesiredRotation.Vector(), CurrentRotation.Vector());
	ClampPitch(DesiredRotation);

	if (CanInterpolate(DeltaAngle) || IsPitchClamped(DesiredRotation))
	{
		DesiredRotation = FMath::RInterpTo(CurrentRotation, DesiredRotation, DeltaTime, GetInterpolationSpeed(DeltaAngle));
		ApplyRotationAxes(CurrentRotation, DesiredRotation);
	}
	else
	{
		DesiredRotation = CurrentRotation;
	}

	return DesiredRotation;
}

bool ULazyInterpolationMode::CanInterpolate(float DeltaAngle) const
{
	if (bInterpolationInProgress)
	{
		if (DeltaAngle <= StopInterpAngle)
		{
			bInterpolationInProgress = false;
		}
	}
	else
	{
		if (DeltaAngle > BeginInterpAngle)
		{
			bInterpolationInProgress = true;
		}
	}

	return bInterpolationInProgress;
}

float ULazyInterpolationMode::GetInterpolationSpeed(float DeltaAngle) const
{
	//TODO: If pitch is clamped then we can increase interpolation speed.
	return InterpolationSpeed * FMath::Clamp((DeltaAngle - StopInterpAngle) / FMath::Max(SmoothingRange, 1.f), MinInterpSpeedRatio, 1.f);
}

#if WITH_EDITOR

void ULazyInterpolationMode::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	FName PropertyName = Event.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ULazyInterpolationMode, BeginInterpAngle))
	{
		if (BeginInterpAngle < StopInterpAngle)
		{
			StopInterpAngle = FMath::Clamp(BeginInterpAngle - 1.f, 0.f, BeginInterpAngle);
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ULazyInterpolationMode, StopInterpAngle))
	{
		if (StopInterpAngle > BeginInterpAngle)
		{
			BeginInterpAngle = FMath::Clamp(StopInterpAngle + 1.f, 0.f, 180.f);
		}
	}
}

#endif
