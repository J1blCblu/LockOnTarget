// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/RotationModes/RotationModeBase.h"

URotationModeBase::URotationModeBase()
	: RotationAxes(0b111)
	, PitchClamp(-60.f, 60.f)
	, OffsetRotation(0.f)
{

}

FRotator URotationModeBase::GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime)
{
	FRotator NewRotation = GetRotationToTarget(InstigatorLocation, TargetLocation);
	ApplyRotationAxes(CurrentRotation, NewRotation);

	return NewRotation;
}

FRotator URotationModeBase::GetRotationToTarget(const FVector& LocationFrom, const FVector& LocationTo) const
{
	FRotator RotationToTarget = FRotationMatrix::MakeFromX(LocationTo - LocationFrom).Rotator();

	if(RotationToTarget.Pitch + OffsetRotation.Pitch < PitchClamp.X)
	{
		RotationToTarget.Pitch = PitchClamp.X;
	} 
	else if(RotationToTarget.Pitch + OffsetRotation.Pitch > PitchClamp.Y)
	{
		RotationToTarget.Pitch = PitchClamp.Y;
	}
	else if(!OffsetRotation.IsNearlyZero())
	{
		RotationToTarget = FRotator(FQuat(RotationToTarget) * FQuat(OffsetRotation));
	}

	return RotationToTarget;
}

void URotationModeBase::ApplyRotationAxes(const FRotator& CurrentRotation, FRotator& NewRotation) const
{
	NewRotation = {
		RotationAxes & ERot::Pitch ? NewRotation.Pitch : CurrentRotation.Pitch,
		RotationAxes & ERot::Yaw ? NewRotation.Yaw : CurrentRotation.Yaw,
		RotationAxes & ERot::Roll ? NewRotation.Roll : CurrentRotation.Roll
	};
}

#if WITH_EDITORONLY_DATA

void URotationModeBase::PostEditChangeProperty(struct FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	FName PropertyName = Event.GetPropertyName();
	FName MemberPropertyName = Event.MemberProperty->GetFName();

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(URotationModeBase, PitchClamp))
	{
		PitchClamp = PitchClamp.ClampAxes(-90.f, 90.f);

		if (PropertyName == GET_MEMBER_NAME_CHECKED(FVector2D, X))
		{
			if (PitchClamp.X > PitchClamp.Y)
			{
				PitchClamp.X = PitchClamp.Y;
			}
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(FVector2D, Y))
		{
			if (PitchClamp.Y < PitchClamp.X)
			{
				PitchClamp.Y = PitchClamp.X;
			}
		}
	}
}

#endif
