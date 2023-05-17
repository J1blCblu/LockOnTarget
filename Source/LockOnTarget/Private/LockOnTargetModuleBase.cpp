// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetModuleBase.h"
#include "LockOnTargetComponent.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

/********************************************************************
 * ULockOnTargetModuleProxy
 ********************************************************************/

ULockOnTargetModuleProxy::ULockOnTargetModuleProxy()
	: LockOnTargetComponent(nullptr)
	, CachedController(nullptr)
	, bIsInitialized(false)
{
	//Do something.
}

UWorld* ULockOnTargetModuleProxy::GetWorld() const
{
	return (IsValid(GetOuter()) && (!GIsEditor || GIsPlayInEditorWorld)) ? GetOuter()->GetWorld() : nullptr;
}

ULockOnTargetComponent* ULockOnTargetModuleProxy::GetLockOnTargetComponent() const
{
	return IsInitialized() ? LockOnTargetComponent.Get() : GetTypedOuter<ULockOnTargetComponent>();;
}

AController* ULockOnTargetModuleProxy::GetController() const
{
	if(!CachedController.IsValid())
	{
		CachedController = TryFindController();
	}
	
	return CachedController.Get();
}

APlayerController* ULockOnTargetModuleProxy::GetPlayerController() const
{
	return GetController() && GetController()->IsPlayerController() ? static_cast<APlayerController*>(GetController()) : nullptr;
}

AController* ULockOnTargetModuleProxy::TryFindController() const
{
	AController* Controller = nullptr;

	if (APawn* const Pawn = Cast<APawn>(GetLockOnTargetComponent()->GetOwner()))
	{
		Controller = Pawn->GetController();
	}
	
	return Controller;
}

void ULockOnTargetModuleProxy::Initialize(ULockOnTargetComponent* Instigator)
{
	if (ensure(!IsInitialized() && Instigator))
	{
		LockOnTargetComponent = Instigator;
		K2_Initialize(Instigator);
		bIsInitialized = true;
	}
}

void ULockOnTargetModuleProxy::Deinitialize(ULockOnTargetComponent* Instigator)
{
	if (ensure(IsInitialized() && GetLockOnTargetComponent() == Instigator))
	{
		K2_Deinitialize(Instigator);
		LockOnTargetComponent = nullptr;
		bIsInitialized = false;
	}
}

void ULockOnTargetModuleProxy::OnTargetLocked(UTargetComponent* Target, FName Socket)
{
	K2_OnTargetLocked(Target, Socket);
}

void ULockOnTargetModuleProxy::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	K2_OnTargetUnlocked(UnlockedTarget, Socket);
}

void ULockOnTargetModuleProxy::OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket)
{
	K2_OnSocketChanged(CurrentTarget, NewSocket, OldSocket);
}

void ULockOnTargetModuleProxy::OnTargetNotFound(bool bIsTargetLocked)
{
	K2_OnTargetNotFound(bIsTargetLocked);
}

void ULockOnTargetModuleProxy::Update(float DeltaTime)
{
	K2_Update(DeltaTime);
}
