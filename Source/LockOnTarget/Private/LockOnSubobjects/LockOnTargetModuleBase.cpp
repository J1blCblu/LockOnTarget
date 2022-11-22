// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/LockOnTargetModuleBase.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetDefines.h"

ULockOnTargetModuleProxy::ULockOnTargetModuleProxy()
	: LockOnTargetComponent(nullptr)
	, bIsInitialized(false)
	, bWantsUpdate(true)
	, bUpdateOnlyWhileLocked(true)
{
	//Do something.
}

void ULockOnTargetModuleProxy::BeginDestroy()
{
	// If we're in the process of being garbage collected it is unsafe to call out to blueprints.
	checkf(!bIsInitialized, TEXT("Module should be deinitialized first. Maybe it was marked as garbage manually."));

	Super::BeginDestroy();
}

UWorld* ULockOnTargetModuleProxy::GetWorld() const
{
	return (IsValid(GetOuter()) && (!GIsEditor || GIsPlayInEditorWorld)) ? GetOuter()->GetWorld() : nullptr;
}

ULockOnTargetComponent* ULockOnTargetModuleProxy::GetLockOnTargetComponent() const
{
	return LockOnTargetComponent.IsValid() ? LockOnTargetComponent.Get() : GetLockOnTargetComponentFromOuter();
}

ULockOnTargetComponent* ULockOnTargetModuleProxy::GetLockOnTargetComponentFromOuter() const
{
	//In some cases the outer can be a transient package in the editor.
	check(GetOuter() && GetOuter()->IsA(ULockOnTargetComponent::StaticClass()));
	return StaticCast<ULockOnTargetComponent*>(GetOuter());
}

void ULockOnTargetModuleProxy::InitializeModule(ULockOnTargetComponent* Instigator)
{
	check(!bIsInitialized);
	check(Instigator && Instigator == GetOuter());

	LockOnTargetComponent = Instigator;
	BindToLockOn();
	Initialize(Instigator);
	K2_Initialize();
	bIsInitialized = true;

	//If we have been created at runtime and any Target is locked then call the OnTargetLockedcallback.
	if (LockOnTargetComponent->IsTargetLocked())
	{
		OnTargetLocked(LockOnTargetComponent->GetHelperComponent(), LockOnTargetComponent->GetCapturedSocket());
	}
}

void ULockOnTargetModuleProxy::Initialize(ULockOnTargetComponent* Instigator)
{
	//Unimplemented.
}

void ULockOnTargetModuleProxy::BindToLockOn()
{
	LockOnTargetComponent->OnTargetLocked.AddDynamic(this, &ThisClass::OnTargetLockedPrivate);
	LockOnTargetComponent->OnTargetUnlocked.AddDynamic(this, &ThisClass::OnTargetUnlockedPrivate);
	LockOnTargetComponent->OnSocketChanged.AddDynamic(this, &ThisClass::OnSocketChangedPrivate);
	LockOnTargetComponent->OnTargetNotFound.AddDynamic(this, &ThisClass::OnTargetNotFoundPrivate);
}

void ULockOnTargetModuleProxy::DeinitializeModule(ULockOnTargetComponent* Instigator)
{
	check(bIsInitialized);
	//Module must be deinitialized by the same Instigator.
	check(Instigator && Instigator == LockOnTargetComponent.Get());

	//If we have been destroyed while the Target is locked then call the OnTargetUnlocked callback.
	if (LockOnTargetComponent->IsTargetLocked())
	{
		OnTargetUnlocked(LockOnTargetComponent->GetHelperComponent(), LockOnTargetComponent->GetCapturedSocket());
	}

	UnbindFromLockOn();
	Deinitialize(Instigator);
	K2_Deinitialize();
	LockOnTargetComponent = nullptr;
	bIsInitialized = false;
	MarkAsGarbage();
}

void ULockOnTargetModuleProxy::Deinitialize(ULockOnTargetComponent* Instigator)
{
	//Unimplemented.
}

void ULockOnTargetModuleProxy::UnbindFromLockOn()
{
	LockOnTargetComponent->OnTargetLocked.RemoveDynamic(this, &ThisClass::OnTargetLockedPrivate);
	LockOnTargetComponent->OnTargetUnlocked.RemoveDynamic(this, &ThisClass::OnTargetUnlockedPrivate);
	LockOnTargetComponent->OnSocketChanged.RemoveDynamic(this, &ThisClass::OnSocketChangedPrivate);
	LockOnTargetComponent->OnTargetNotFound.RemoveDynamic(this, &ThisClass::OnTargetNotFoundPrivate);
}

void ULockOnTargetModuleProxy::OnTargetLockedPrivate(UTargetingHelperComponent* Target, FName Socket)
{
	check(Target);
	OnTargetLocked(Target, Socket);
	K2_OnTargetLocked(Target, Socket);
}

void ULockOnTargetModuleProxy::OnTargetLocked(UTargetingHelperComponent* Target, FName Socket)
{
	//Unimplemented.
}

void ULockOnTargetModuleProxy::OnTargetUnlockedPrivate(UTargetingHelperComponent* UnlockedTarget, FName Socket)
{
	check(UnlockedTarget);
	OnTargetUnlocked(UnlockedTarget, Socket);
	K2_OnTargetUnlocked(UnlockedTarget, Socket);
}

void ULockOnTargetModuleProxy::OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket)
{
	//Unimplemented.
}

void ULockOnTargetModuleProxy::OnSocketChangedPrivate(UTargetingHelperComponent* CurrentTarget, FName NewSocket, FName OldSocket)
{
	check(CurrentTarget);
	OnSocketChanged(CurrentTarget, NewSocket, OldSocket);
	K2_OnSocketChanged(CurrentTarget, NewSocket, OldSocket);
}

void ULockOnTargetModuleProxy::OnSocketChanged(UTargetingHelperComponent* CurrentTarget, FName NewSocket, FName OldSocket)
{
	//Unimplemented.
}

void ULockOnTargetModuleProxy::OnTargetNotFoundPrivate()
{
	OnTargetNotFound();
	K2_OnTargetNotFound();
}

void ULockOnTargetModuleProxy::OnTargetNotFound()
{
	//Unimplemented.
}

void ULockOnTargetModuleProxy::Update(FVector2D PlayerInput, float DeltaTime)
{
	if (bWantsUpdate && (!bUpdateOnlyWhileLocked || GetLockOnTargetComponent()->IsTargetLocked()))
	{
		LOT_EVENT(ModuleUpdate, White);

		UpdateOverridable(PlayerInput, DeltaTime);
		K2_Update(PlayerInput, DeltaTime);
	}
}

void ULockOnTargetModuleProxy::UpdateOverridable(FVector2D PlayerInput, float DeltaTime)
{
	//Unimplemented.
}

#if WITH_EDITOR

bool ULockOnTargetModuleProxy::CanEditChange(const FProperty* InProperty) const
{
	bool bSuper = Super::CanEditChange(InProperty);

	const FName PropertyName = InProperty->GetFName();

	//Don't allow to modify properties in native classes, though allow to modify in BP.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bWantsUpdate)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bUpdateOnlyWhileLocked))
	{
		return bSuper && GetClass() && !GetClass()->HasAnyClassFlags(CLASS_Native);
	}

	return bSuper;
}

#endif
