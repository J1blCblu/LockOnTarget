// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/RotationModes/RotationModeBase.h"

URotationModeBase::URotationModeBase()
	: PitchClamp(-50.f, 25.f)
	, OffsetRotation(0.f)
	, RotationAxes(0b00000111)
{
	//Do something.
}

FRotator URotationModeBase::GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime)
{
	FRotator DesiredRotation = GetRotationUnclamped(InstigatorLocation, TargetLocation);
	ClampPitch(DesiredRotation);
	ApplyRotationAxes(CurrentRotation, DesiredRotation);

	return DesiredRotation;
}

FRotator URotationModeBase::GetRotationUnclamped(const FVector& From, const FVector& To) const
{
	//@TODO: Maybe extract the up vector from the CurrentRotation and use the MakeFromXZ version.
	return FRotator(FRotationMatrix::MakeFromX(To - From).ToQuat() * FQuat(OffsetRotation));
}

void URotationModeBase::ClampPitch(FRotator& Rotation) const
{
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch, PitchClamp.X, PitchClamp.Y);
}

bool URotationModeBase::IsPitchClamped(const FRotator& Rotation) const
{
	return FMath::IsNearlyEqual(Rotation.Pitch, PitchClamp.X) || FMath::IsNearlyEqual(Rotation.Pitch, PitchClamp.Y);
}

void URotationModeBase::ApplyRotationAxes(const FRotator& CurrentRotation, FRotator& NewRotation) const
{
	NewRotation.Pitch = HasAnyRotFlags(RotationAxes, ERot::Pitch) ? NewRotation.Pitch : CurrentRotation.Pitch;
	NewRotation.Yaw = HasAnyRotFlags(RotationAxes, ERot::Yaw) ? NewRotation.Yaw : CurrentRotation.Yaw;
	NewRotation.Roll = HasAnyRotFlags(RotationAxes, ERot::Roll) ? NewRotation.Roll : CurrentRotation.Roll;
}

#if WITH_EDITOR

void URotationModeBase::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(URotationModeBase, PitchClamp))
	{
		PitchClamp = PitchClamp.ClampAxes(-90.f, 90.f);
	}
}

#endif
