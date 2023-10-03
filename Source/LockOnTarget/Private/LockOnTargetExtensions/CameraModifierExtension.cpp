// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetExtensions/CameraModifierExtension.h"

#include "GameFramework/PlayerController.h"

/********************************************************************
 * ULockOnTargetCameraModifier_Zoom
 ********************************************************************/

ULockOnTargetCameraModifier_Zoom::ULockOnTargetCameraModifier_Zoom()
	: MaxDeltaFOV(-3.f)
{
	bDisabled = true;
	AlphaInTime = 0.25f;
	AlphaOutTime = 0.15f;
}

void ULockOnTargetCameraModifier_Zoom::ModifyCamera(float DeltaTime, FVector ViewLocation, FRotator ViewRotation, float FOV, FVector& NewViewLocation, FRotator& NewViewRotation, float& NewFOV)
{
	NewFOV += Alpha * MaxDeltaFOV;
}

/********************************************************************
 * UCameraModifierExtension
 ********************************************************************/

UCameraModifierExtension::UCameraModifierExtension()
	: CameraModifierClass(ULockOnTargetCameraModifier_Zoom::StaticClass())
{
	ExtensionTick.bCanEverTick = false;
}

void UCameraModifierExtension::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);

	const APlayerController* const PlayerController = GetPlayerController();

	if (PlayerController && PlayerController->IsLocalController())
	{
		CameraModifier = PlayerController->PlayerCameraManager->AddNewCameraModifier(CameraModifierClass);
	}
}

void UCameraModifierExtension::Deinitialize(ULockOnTargetComponent* Instigator)
{
	Super::Deinitialize(Instigator);

	const APlayerController* const PlayerController = GetPlayerController();

	if (PlayerController && PlayerController->IsLocalController())
	{
		PlayerController->PlayerCameraManager->RemoveCameraModifier(CameraModifier.Get());
	}
}

void UCameraModifierExtension::OnTargetLocked(UTargetComponent* Target, FName Socket)
{
	Super::OnTargetLocked(Target, Socket);

	if (CameraModifier.IsValid())
	{
		CameraModifier->EnableModifier();
	}
}

void UCameraModifierExtension::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);
	
	if (CameraModifier.IsValid())
	{
		CameraModifier->DisableModifier(false);
	}
}
