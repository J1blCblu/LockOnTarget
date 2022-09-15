// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/Modules/RotationModules.h"
#include "LockOnSubobjects/RotationModes/RotationModeBase.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetDefines.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"

//----------------------------------------------------------------//
//  URotationModule
//----------------------------------------------------------------//

URotationModule::URotationModule()
{
	bWantsUpdate = true;
	bUpdateOnlyWhileLocked = true;
}

URotationModeBase* URotationModule::SetRotationMode(TSubclassOf<URotationModeBase> RotationModeClass)
{
	return RotationMode = NewObject<URotationModeBase>(this, RotationModeClass, MakeUniqueObjectName(this, RotationModeClass, TEXT("RotationMode")));
}

FVector URotationModule::GetViewLocation_Implementation() const
{
	return GetLockOnTargetComponent()->GetOwnerLocation();
}

//----------------------------------------------------------------//
//  UControlRotationModule
//----------------------------------------------------------------//

UControlRotationModule::UControlRotationModule()
{
	//Do something.
}

FVector UControlRotationModule::GetViewLocation_Implementation() const
{
	return GetLockOnTargetComponent()->GetCameraLocation();
}

void UControlRotationModule::UpdateOverridable(FVector2D PlayerInput, float DeltaTime)
{
	if (IsValid(RotationMode))
	{
		AController* const Controller = GetLockOnTargetComponent()->GetController();

		if (Controller && Controller->IsLocalController())
		{
			const FVector ViewLocationPoint = GetViewLocation();
			const FRotator ControlRotation = Controller->GetControlRotation();
			const FVector TargetLocation = GetLockOnTargetComponent()->GetCapturedFocusLocation();
			//@TODO: Maybe use owner's yaw and camera's pitch combined rotation (to avoid the existence of a Target between the owner and the camera).
			Controller->SetControlRotation(RotationMode->GetRotation(ControlRotation, ViewLocationPoint, TargetLocation, DeltaTime));
		}
	}
}

//----------------------------------------------------------------//
//  UOwnerRotationModule
//----------------------------------------------------------------//

UOwnerRotationModule::UOwnerRotationModule()
{
	//Do something.
}

void UOwnerRotationModule::UpdateOverridable(FVector2D PlayerInput, float DeltaTime)
{
	if (IsValid(RotationMode))
	{
		if (AActor* const Owner = GetLockOnTargetComponent()->GetOwner())
		{
			const FVector ViewLocationPoint = GetViewLocation();
			const FRotator OwnerRotation = Owner->GetActorRotation();
			const FVector TargetLocation = GetLockOnTargetComponent()->GetCapturedFocusLocation();

			Owner->SetActorRotation(RotationMode->GetRotation(OwnerRotation, ViewLocationPoint, TargetLocation, DeltaTime));
		}
	}
}
