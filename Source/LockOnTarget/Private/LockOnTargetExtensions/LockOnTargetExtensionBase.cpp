// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetExtensions/LockOnTargetExtensionBase.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetDefines.h"

#include "GameFramework/PlayerController.h"

/********************************************************************
 * FLockOnTargetExtensionTickFunction
 ********************************************************************/

void FLockOnTargetExtensionTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	LOT_SCOPED_EVENT(ExtensionUpdate);
	
	if (IsValid(TargetExtension))
	{
		TargetExtension->Update(DeltaTime);
	}
}

FString FLockOnTargetExtensionTickFunction::DiagnosticMessage()
{
	return TargetExtension->GetFullName() + TEXT("[Update]");
}

FName FLockOnTargetExtensionTickFunction::DiagnosticContext(bool bDetailed)
{
	if (bDetailed)
	{
		//Format is "ExtensionClass/OwningActorClass/ExtensionName"
		const FString OwnerClassName = TargetExtension->GetLockOnTargetComponent()->GetOwner()->GetClass()->GetName();
		const FString ContextString = FString::Printf(TEXT("%s/%s/%s"), *TargetExtension->GetClass()->GetName(), *OwnerClassName, *TargetExtension->GetName());
		return FName(*ContextString);
	}
	else
	{
		return TargetExtension->GetClass()->GetFName();
	}
}

/********************************************************************
 * ULockOnTargetExtensionProxy
 ********************************************************************/

ULockOnTargetExtensionProxy::ULockOnTargetExtensionProxy()
	: LockOnTargetComponent(nullptr)
	, bIsInitialized(false)
{
	ExtensionTick.TickGroup = TG_DuringPhysics;
	ExtensionTick.EndTickGroup = TG_PostPhysics;
	ExtensionTick.bAllowTickOnDedicatedServer = true;
	ExtensionTick.bCanEverTick = true; //For BP implementations.
	ExtensionTick.bHighPriority = false;
	ExtensionTick.bStartWithTickEnabled = true; //For BP implementations.
	ExtensionTick.bTickEvenWhenPaused = false;
}

void ULockOnTargetExtensionProxy::Initialize(ULockOnTargetComponent* Instigator)
{
	if (ensure(!IsInitialized() && Instigator))
	{
		if (const AActor* const InstigatorOwner = Instigator->GetOwner())
		{
			LockOnTargetComponent = Instigator;
			
			if (ExtensionTick.bCanEverTick && !IsTemplate())
			{
				ExtensionTick.TargetExtension = this;
				ExtensionTick.SetTickFunctionEnable(ExtensionTick.bStartWithTickEnabled);
				ExtensionTick.RegisterTickFunction(InstigatorOwner->GetLevel());
			}

			K2_Initialize(Instigator);
			bIsInitialized = true;
		}
	}
}

void ULockOnTargetExtensionProxy::Deinitialize(ULockOnTargetComponent* Instigator)
{
	if (ensure(IsInitialized() && GetLockOnTargetComponent() == Instigator))
	{
		bIsInitialized = false;

		K2_Deinitialize(Instigator);
		LockOnTargetComponent = nullptr;

		if (ExtensionTick.IsTickFunctionRegistered())
		{
			ExtensionTick.UnRegisterTickFunction();
		}
	}
}

void ULockOnTargetExtensionProxy::SetTickEnabled(bool bInTickEnabled)
{
	if (IsInitialized() && ExtensionTick.bCanEverTick)
	{
		ExtensionTick.SetTickFunctionEnable(bInTickEnabled);
	}
}

UWorld* ULockOnTargetExtensionProxy::GetWorld() const
{
	return (IsValid(GetOuter()) && (!GIsEditor || GIsPlayInEditorWorld)) ? GetOuter()->GetWorld() : nullptr;
}

ULockOnTargetComponent* ULockOnTargetExtensionProxy::GetLockOnTargetComponent() const
{
	return IsInitialized() ? LockOnTargetComponent : GetTypedOuter<ULockOnTargetComponent>();
}

AController* ULockOnTargetExtensionProxy::GetInstigatorController() const
{
	const AActor* const Owner = GetLockOnTargetComponent()->GetOwner();
	return Owner ? Owner->GetInstigatorController() : nullptr;
}

APlayerController* ULockOnTargetExtensionProxy::GetPlayerController() const
{
	AController* const InstigatorController = GetInstigatorController();
	return InstigatorController && InstigatorController->IsPlayerController() ? static_cast<APlayerController*>(InstigatorController) : nullptr;
}

APawn* ULockOnTargetExtensionProxy::GetInstigatorPawn() const
{
	const AActor* const Owner = GetLockOnTargetComponent()->GetOwner();
	return Owner ? Owner->GetInstigator() : nullptr;
}

void ULockOnTargetExtensionProxy::OnTargetLocked(UTargetComponent* Target, FName Socket)
{
	K2_OnTargetLocked(Target, Socket);
}

void ULockOnTargetExtensionProxy::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	K2_OnTargetUnlocked(UnlockedTarget, Socket);
}

void ULockOnTargetExtensionProxy::OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket)
{
	K2_OnSocketChanged(CurrentTarget, NewSocket, OldSocket);
}

void ULockOnTargetExtensionProxy::OnTargetNotFound(bool bIsTargetLocked)
{
	K2_OnTargetNotFound(bIsTargetLocked);
}

void ULockOnTargetExtensionProxy::Update(float DeltaTime)
{
	K2_Update(DeltaTime);
}
