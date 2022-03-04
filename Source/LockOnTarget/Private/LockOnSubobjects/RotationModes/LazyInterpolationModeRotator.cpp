// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/RotationModes/LazyInterpolationModeRotator.h"
#include "LockOnTargetComponent.h"

#if WITH_EDITORONLY_DATA
#include "DrawDebugHelpers.h"
#endif

ULazyInterpolationModeRotator::ULazyInterpolationModeRotator()
{
}

FRotator ULazyInterpolationModeRotator::GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime)
{
	float InterpSpeed = InterpolationSpeed;
	FRotator NewRotation = FRotationMatrix::MakeFromX(TargetLocation - InstigatorLocation).Rotator();
	ClampPitch(NewRotation);

	uint8 RotFlags = 0;

#if WITH_EDITORONLY_DATA
	DrawDebugInfo();
#endif

	if (!CanLazyInterpolate(NewRotation, CurrentRotation, InterpSpeed, RotFlags))
	{
		return CurrentRotation;
	}

	NewRotation = FMath::RInterpTo(CurrentRotation, NewRotation, DeltaTime, InterpSpeed);

	NewRotation = {
		(RotationAxes & ERot::E_Pitch && RotFlags & ERot::E_Pitch) ? NewRotation.Pitch : CurrentRotation.Pitch,
		(RotationAxes & ERot::E_Yaw && RotFlags & ERot::E_Yaw) ? NewRotation.Yaw : CurrentRotation.Yaw,
		(RotationAxes & ERot::E_Roll && RotFlags & ERot::E_Roll) ? NewRotation.Roll : CurrentRotation.Roll};

	return NewRotation;
}

bool ULazyInterpolationModeRotator::CanLazyInterpolate(const FRotator& NewRotation, const FRotator& CurrentRotation, float& InterpSpeed, uint8& RotFlags)
{
	FRotator DeltaRot = NewRotation - CurrentRotation;
	DeltaRot.Normalize();

	if (!bLazyInterpolationInProgress)
	{
		if (FMath::Abs(DeltaRot.Pitch) > StartRotator.Pitch)
		{
			bLazyInterpolationInProgress = true;
			RotFlags |= static_cast<uint8>(ERot::E_Pitch);
		}

		if (FMath::Abs(DeltaRot.Yaw) > StartRotator.Yaw)
		{
			bLazyInterpolationInProgress = true;
			RotFlags |= static_cast<uint8>(ERot::E_Yaw);
		}

		if (FMath::Abs(DeltaRot.Roll) > StartRotator.Roll)
		{
			bLazyInterpolationInProgress = true;
			RotFlags |= static_cast<uint8>(ERot::E_Roll);
		}
	}
	else
	{
		if (FMath::Abs(DeltaRot.Pitch) > StopRotator.Pitch)
		{
			RotFlags |= static_cast<uint8>(ERot::E_Pitch);
		}

		if (FMath::Abs(DeltaRot.Yaw) > StopRotator.Yaw)
		{
			RotFlags |= static_cast<uint8>(ERot::E_Yaw);
		}

		if (FMath::Abs(DeltaRot.Roll) > StopRotator.Roll)
		{
			RotFlags |= static_cast<uint8>(ERot::E_Roll);
		}
	}

	if (FMath::Abs(DeltaRot.Pitch) <= StopRotator.Pitch && FMath::Abs(DeltaRot.Yaw) <= StopRotator.Yaw && FMath::Abs(DeltaRot.Roll) <= StopRotator.Roll)
	{
		bLazyInterpolationInProgress = false;
	}

	if (bLazyInterpolationInProgress)
	{
		float BigDelta = FMath::Max(RotFlags & ERot::E_Yaw ? FMath::Abs(DeltaRot.Yaw) : 0.f, RotFlags & ERot::E_Pitch ? FMath::Abs(DeltaRot.Pitch) : 0.f);
		float Ratio = BigDelta - FMath::Max3(RotFlags & ERot::E_Pitch ? StopRotator.Pitch : 0.f, RotFlags & ERot::E_Yaw ? StopRotator.Yaw : 0.f, RotFlags & ERot::E_Roll ? StopRotator.Roll : 0.f);
		Ratio /= SmoothingAngleRange;
		InterpSpeed *= FMath::Clamp(Ratio, SmoothRangeRatioClamp.X, SmoothRangeRatioClamp.Y);
	}

	return bLazyInterpolationInProgress;
}

#if WITH_EDITORONLY_DATA
void ULazyInterpolationModeRotator::DrawDebugInfo() const
{
	if (bDebug && GetWorld())
	{
		const FVector FocusPointLocation = GetLockOn()->GetCapturedLocation(true);
		const FVector LineOrigin = GetLockOn()->GetCameraLocation();
		const FVector Distance = FocusPointLocation - LineOrigin;
		const FVector ForwardVector = GetLockOn()->GetCameraForwardVector();
		const FVector LazyFocusPoint = LineOrigin + (ForwardVector * ((FocusPointLocation - LineOrigin) | ForwardVector));

		DrawDebugSphere(GetWorld(), LazyFocusPoint, 8.f, 15, FColor::Yellow, false, 0.f, 7, 2.f);
		DrawDebugLine(GetWorld(), FocusPointLocation, LazyFocusPoint, FColor::Yellow, false, 0.f, 4, 6.f);

		TArray<FVector> verts;
		verts.Reserve(4);

		float up = Distance.Size() * FMath::Tan(StartRotator.Pitch * (PI / 180.f));
		float right = Distance.Size() * FMath::Tan(StartRotator.Yaw * (PI / 180.f));

		const FVector UpVector = GetLockOn()->GetCameraUpVector();
		const FVector RightVector = GetLockOn()->GetCameraRightVector();

		verts.Push(FocusPointLocation + RightVector * right + UpVector * up);
		verts.Push(FocusPointLocation + RightVector * right - UpVector * up);
		verts.Push(FocusPointLocation - RightVector * right - UpVector * up);
		verts.Push(FocusPointLocation - RightVector * right + UpVector * up);

		for (uint8 i = 0; i < 3; ++i)
		{
			DrawDebugLine(GetWorld(), verts[i], verts[i + 1], FColor::Yellow, false, 0.f, 4, 4.f);
		}
		DrawDebugLine(GetWorld(), verts[0], verts[3], FColor::Yellow, false, 0.f, 4, 4.f);

		up = Distance.Size() * FMath::Tan(StopRotator.Pitch * (PI / 180.f));
		right = Distance.Size() * FMath::Tan(StopRotator.Yaw * (PI / 180.f));

		verts[0] = FocusPointLocation + RightVector * right + UpVector * up;
		verts[1] = FocusPointLocation + RightVector * right - UpVector * up;
		verts[2] = FocusPointLocation - RightVector * right - UpVector * up;
		verts[3] = FocusPointLocation - RightVector * right + UpVector * up;

		for (uint8 i = 0; i < 3; ++i)
		{
			DrawDebugLine(GetWorld(), verts[i], verts[i + 1], FColor::Red, false, 0.f, 4, 4.f);
		}

		DrawDebugLine(GetWorld(), verts[0], verts[3], FColor::Red, false, 0.f, 4, 4.f);
	}
}
#endif
