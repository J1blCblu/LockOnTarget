// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "DefaultModules/RotationModules.h"
#include "LockOnTargetComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

constexpr float MinInterpSpeedRatio = 0.025f;

//----------------------------------------------------------------//
//  UActorRotationModule
//----------------------------------------------------------------//

UActorRotationModule::UActorRotationModule()
	: InterpolationSpeed(15.5f)
	, bIsActive(true)
{
	//Do something.
}

void UActorRotationModule::Update(float DeltaTime)
{
	if (GetLockOnTargetComponent()->IsTargetLocked() && IsRotationActive())
	{
		if (AActor* const Owner = GetLockOnTargetComponent()->GetOwner())
		{
			const FVector ViewLocation = GetViewLocation();
			const FRotator ActorRotation = Owner->GetActorRotation();
			const FVector TargetLocation = GetLockOnTargetComponent()->GetCapturedFocusLocation();

			Owner->SetActorRotation(GetRotation(ActorRotation, ViewLocation, TargetLocation, DeltaTime));
		}
	}
}

FVector UActorRotationModule::GetViewLocation_Implementation() const
{
	return GetLockOnTargetComponent()->GetOwner()->GetActorLocation();
}

FRotator UActorRotationModule::GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& ViewLocation, const FVector& TargetLocation, float DeltaTime)
{
	FRotator OutRotation = FMath::RInterpTo(CurrentRotation, GetRotationUnclamped(ViewLocation, TargetLocation), DeltaTime, InterpolationSpeed);
	OutRotation.Roll = 0.f;
	OutRotation.Pitch = 0.f;
	return OutRotation;
}

FRotator UActorRotationModule::GetRotationUnclamped(const FVector& From, const FVector& To) const
{
	return FRotationMatrix::MakeFromX(To - From).Rotator();
}

void UActorRotationModule::SetRotationIsActive(bool bInActive)
{
	check(IsInitialized());
	bIsActive = bInActive;
}

//----------------------------------------------------------------//
//  UControllerRotationModule
//----------------------------------------------------------------//

UControllerRotationModule::UControllerRotationModule()
	: PitchOffset(-10.f)
	, PitchClamp(-50.f, 25.f)
	, BeginInterpAngle(4.f)
	, StopInterpOffset(1.f)
	, SmoothingRange(14.f)
	, bInterpolationInProgress(false)
{
	InterpolationSpeed = 20.f;
}

void UControllerRotationModule::Update(float DeltaTime)
{
	if (GetLockOnTargetComponent()->IsTargetLocked() && IsRotationActive())
	{
		AController* const Controller = GetController();

		if (Controller && Controller->IsLocalController())
		{
			const FVector ViewLocation = GetViewLocation();
			const FRotator ControlRotation = Controller->GetControlRotation();
			const FVector TargetLocation = GetLockOnTargetComponent()->GetCapturedFocusLocation();

			Controller->SetControlRotation(GetRotation(ControlRotation, ViewLocation, TargetLocation, DeltaTime));
		}
	}
}

FVector UControllerRotationModule::GetViewLocation_Implementation() const
{
	FVector OutLocation = Super::GetViewLocation_Implementation();

	if(GetController() && GetController()->IsPlayerController())
	{
		const auto* const PlayerController = static_cast<APlayerController*>(GetController());

		if (PlayerController->PlayerCameraManager)
		{
			OutLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
		}
	}

	return OutLocation;
}

FRotator UControllerRotationModule::GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& ViewLocation, const FVector& TargetLocation, float DeltaTime)
{
	FRotator OutRotation = GetRotationUnclamped(ViewLocation, TargetLocation);
	OutRotation.Pitch = FRotator::NormalizeAxis(OutRotation.Pitch + PitchOffset);;
	const float DeltaAngle = FMath::RadiansToDegrees(FMath::Acos(OutRotation.Vector() | CurrentRotation.Vector()));
	ClampPitch(OutRotation);

	if (CanInterpolate(DeltaAngle) || IsPitchClamped(OutRotation))
	{
		OutRotation = FMath::RInterpTo(CurrentRotation, OutRotation, DeltaTime, GetInterpolationSpeed(DeltaAngle));
		OutRotation.Roll = 0.f;
	}
	else
	{
		OutRotation = CurrentRotation;
	}

	return OutRotation;
}

void UControllerRotationModule::ClampPitch(FRotator& Rotation) const
{
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch, PitchClamp.X, PitchClamp.Y);
}

bool UControllerRotationModule::IsPitchClamped(const FRotator& Rotation) const
{
	return FMath::IsNearlyEqual(Rotation.Pitch, PitchClamp.X) || FMath::IsNearlyEqual(Rotation.Pitch, PitchClamp.Y);
}

bool UControllerRotationModule::CanInterpolate(float DeltaAngle) const
{
	if (bInterpolationInProgress)
	{
		if (DeltaAngle <= FMath::Max(BeginInterpAngle - StopInterpOffset, 0.f))
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

float UControllerRotationModule::GetInterpolationSpeed(float DeltaAngle) const
{
	return InterpolationSpeed * FMath::Clamp((DeltaAngle - BeginInterpAngle + StopInterpOffset) / FMath::Max(SmoothingRange, 1.f), MinInterpSpeedRatio, 1.2f);
}
