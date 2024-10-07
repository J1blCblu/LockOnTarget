// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetExtensions/PawnRotationExtension.h"
#include "LockOnTargetComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Components/SceneComponent.h"

UPawnRotationExtension::UPawnRotationExtension()
	: RotationRate(720.f)
{
	ExtensionTick.TickGroup = TG_PrePhysics;
	ExtensionTick.bCanEverTick = true;
	ExtensionTick.bStartWithTickEnabled = false;
	ExtensionTick.bAllowTickOnDedicatedServer = true;
}

void UPawnRotationExtension::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);

	//Tick before the movement component.
	if (UPawnMovementComponent* const MovementComponent = GetMovementComponent())
	{
		MovementComponent->PrimaryComponentTick.AddPrerequisite(this, ExtensionTick);
	}
}

void UPawnRotationExtension::Deinitialize(ULockOnTargetComponent* Instigator)
{
	Super::Deinitialize(Instigator);

	if (UPawnMovementComponent* const MovementComponent = GetMovementComponent())
	{
		MovementComponent->PrimaryComponentTick.RemovePrerequisite(this, ExtensionTick);
	}
}

void UPawnRotationExtension::OnTargetLocked(UTargetComponent* Target, FName Socket)
{
	Super::OnTargetLocked(Target, Socket);
	SetTickEnabled(true);
}

void UPawnRotationExtension::OnTargetUnlocked(UTargetComponent* Target, FName Socket)
{
	Super::OnTargetUnlocked(Target, Socket);
	SetTickEnabled(false);
}

void UPawnRotationExtension::Update(float DeltaTime)
{
	if (GetLockOnTargetComponent()->IsTargetLocked())
	{
		UPawnMovementComponent* const MovementComponent = GetMovementComponent();

		if (MovementComponent && MovementComponent->UpdatedComponent)
		{
			const FVector Pivot = MovementComponent->UpdatedComponent->GetComponentLocation();
			const FRotator CurrentRotation = MovementComponent->UpdatedComponent->GetComponentRotation();
			const FVector TargetLocation = GetLockOnTargetComponent()->GetCapturedFocusPointLocation();

			const FVector ToTarget = TargetLocation - Pivot;
			const double DesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));

			if (!FMath::IsNearlyEqual(CurrentRotation.Yaw, DesiredYaw, 1e-3))
			{
				FRotator TargetRotation = CurrentRotation;
				TargetRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, DesiredYaw, GetDeltaYaw(DeltaTime));
				MovementComponent->MoveUpdatedComponent(FVector::ZeroVector, TargetRotation, /*bSweep*/ false);
			}
		}
	}
}

UPawnMovementComponent* UPawnRotationExtension::GetMovementComponent() const
{
	UPawnMovementComponent* MovementComponent = nullptr;

	if (APawn* const Pawn = GetInstigatorPawn())
	{
		MovementComponent = Pawn->GetMovementComponent();
	}

	return MovementComponent;
}

float UPawnRotationExtension::GetDeltaYaw(float DeltaTime)
{
	return (RotationRate >= 0.f) ? FMath::Min(RotationRate * DeltaTime, 360.f) : 360.f;
}
