// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/RotationModes/LazyInterpolationMode.h"
#include "LockOnTargetComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Actor.h"
#include "Utilities/LOTC_BPLibrary.h"

#if WITH_EDITORONLY_DATA
#include "DrawDebugHelpers.h"
#endif

ULazyInterpolationMode::ULazyInterpolationMode()
	: BeginInterpAngle(4.f)
	, StopInterpAngle(3.f)
	, SmoothingAngleRange(7.f)
	, MinInterpSpeedRatio(0.1f)
	, bLazyInterpolationInProgress(false)
{
}

FRotator ULazyInterpolationMode::GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime)
{
	float InterpSpeed = InterpolationSpeed;
	FRotator NewRotation = GetClampedRotationToTarget(InstigatorLocation, TargetLocation);

#if WITH_EDITORONLY_DATA
	DrawDebugInfo();
#endif

	if (!CanLazyInterpolate(NewRotation, CurrentRotation, InterpSpeed))
	{
		return CurrentRotation;
	}

	NewRotation = FMath::RInterpTo(CurrentRotation, NewRotation, DeltaTime, InterpSpeed);

	ApplyRotationAxes(CurrentRotation, NewRotation);

	return NewRotation;
}

bool ULazyInterpolationMode::CanLazyInterpolate_Implementation(const FRotator& NewRotation, const FRotator& CurrentRotation, float& InterpSpeed)
{
	float Angle = ULOTC_BPLibrary::GetAngleDeg(NewRotation.Vector(), CurrentRotation.Vector());

	if (bLazyInterpolationInProgress)
	{
		if (Angle <= StopInterpAngle)
		{
			bLazyInterpolationInProgress = false;
		}
	}
	else
	{
		if (Angle > BeginInterpAngle)
		{
			bLazyInterpolationInProgress = true;
		}
	}

	if (bLazyInterpolationInProgress)
	{
		//Interp speed ratio clamping
		InterpSpeed *= FMath::Clamp((Angle - StopInterpAngle) / SmoothingAngleRange, MinInterpSpeedRatio, 1.f);
	}

	return bLazyInterpolationInProgress;
}

#if WITH_EDITORONLY_DATA

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

void ULazyInterpolationMode::DrawDebugInfo() const
{
	if (bVisualizeOnControlRotation && GetWorld())
	{
		const FVector FocusPointLocation = GetLockOn()->GetCapturedLocation(true);
		const FVector LineOrigin = GetLockOn()->GetCameraLocation();
		const FVector ForwardVector = (GetLockOn()->GetCameraRotation().Quaternion() * OffsetRotation.GetInverse().Quaternion()).Rotator().Vector();
		const FVector LazyFocusPoint = LineOrigin + (ForwardVector * ((FocusPointLocation - LineOrigin) | ForwardVector));

		DrawDebugSphere(GetWorld(), LazyFocusPoint, 8.f, 15, FColor::Yellow, false, 0.f, 7, 2.f);
		DrawDebugLine(GetWorld(), FocusPointLocation, LazyFocusPoint, FColor::Yellow, false, 0.f, 4, 6.f);

		float OuterRadius = (FocusPointLocation - LineOrigin).Size() * FMath::Tan(BeginInterpAngle * (PI / 180.f));
		float InnerRadius = (FocusPointLocation - LineOrigin).Size() * FMath::Tan(StopInterpAngle * (PI / 180.f));
		float SmoothRadius = (FocusPointLocation - LineOrigin).Size() * FMath::Tan((StopInterpAngle + SmoothingAngleRange) * (PI / 180.f));

		const FVector RightVector = GetLockOn()->GetCameraRightVector();
		const FVector UpVector = GetLockOn()->GetCameraUpVector();

		DrawDebugCircle(GetWorld(), FocusPointLocation, OuterRadius, 24, FColor::Yellow, false, 0.f, 4, 3.f, RightVector, UpVector, true);
		DrawDebugCircle(GetWorld(), FocusPointLocation, InnerRadius, 24, FColor::Red, false, 0.f, 3, 3.f, RightVector, UpVector, false);
		DrawDebugCircle(GetWorld(), FocusPointLocation, SmoothRadius, 24, FColor::Blue, false, 0.f, 3, 3.f, RightVector, UpVector, false);
	}
}

#endif
