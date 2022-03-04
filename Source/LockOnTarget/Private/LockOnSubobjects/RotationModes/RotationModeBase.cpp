// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.


#include "LockOnSubobjects/RotationModes/RotationModeBase.h"

URotationModeBase::URotationModeBase()
	: RotationAxes(0xff)
	, PitchClamp(-60.f, 60.f)
{

}

FRotator URotationModeBase::GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime)
{
	FRotator NewRotation = FRotationMatrix::MakeFromX(TargetLocation - InstigatorLocation).Rotator();
	UpdateRotationAxes(CurrentRotation, NewRotation);
	ClampPitch(NewRotation);

	return NewRotation;
}

void URotationModeBase::UpdateRotationAxes(const FRotator& CurrentRotation, FRotator& NewRotation) const
{
	NewRotation = {
		RotationAxes & ERot::E_Pitch ? NewRotation.Pitch : CurrentRotation.Pitch,
		RotationAxes & ERot::E_Yaw ? NewRotation.Yaw : CurrentRotation.Yaw,
		RotationAxes & ERot::E_Roll ? NewRotation.Roll : CurrentRotation.Roll};
}

void URotationModeBase::ClampPitch(FRotator& Rotation) const
{
	if (RotationAxes & ERot::E_Pitch)
	{
		float ClampedPitch = FMath::ClampAngle(Rotation.Pitch - 360.f, PitchClamp.X, PitchClamp.Y);

		if (ClampedPitch < 0)
		{
			ClampedPitch += 360.f;
		}

		Rotation.Pitch = ClampedPitch;
	}
}

#if WITH_EDITORONLY_DATA

void URotationModeBase::PostEditChangeProperty(struct FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	FName PropertyName = Event.GetPropertyName();
	FName MemberPropertyName = Event.MemberProperty->GetFName();

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(URotationModeBase, PitchClamp))
	{
		PitchClamp = PitchClamp.ClampAxes(-180.f, 180.f);

		if (PropertyName == GET_MEMBER_NAME_CHECKED(FVector2D, X))
		{
			if (PitchClamp.X > PitchClamp.Y)
			{
				PitchClamp.Y = PitchClamp.X;
			}
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(FVector2D, Y))
		{
			if (PitchClamp.Y < PitchClamp.X)
			{
				PitchClamp.X = PitchClamp.Y;
			}
		}
	}
}

#endif
