// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/LockOnTargetModuleBase.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetDefines.h"

ULockOnTargetModuleBase::ULockOnTargetModuleBase()
	: LockOnTargetComponent(nullptr)
	, bIsInitialized(false)
	, bWantsUpdate(true)
	, bUpdateOnlyWhileLocked(true)
{
	//Do something.
}

void ULockOnTargetModuleBase::BeginDestroy()
{
	// If we're in the process of being garbage collected it is unsafe to call out to blueprints.
	checkf(!bIsInitialized, TEXT("Module should be deinitialized first. Maybe it was marked as garbage manually."));

	Super::BeginDestroy();
}

UWorld* ULockOnTargetModuleBase::GetWorld() const
{
	return (IsValid(GetOuter()) && (!GIsEditor || GIsPlayInEditorWorld)) ? GetOuter()->GetWorld() : nullptr;
}

ULockOnTargetComponent* ULockOnTargetModuleBase::GetLockOnTargetComponent() const
{
	return LockOnTargetComponent.IsValid() ? LockOnTargetComponent.Get() : GetLockOnTargetComponentFromOuter();
}

ULockOnTargetComponent* ULockOnTargetModuleBase::GetLockOnTargetComponentFromOuter() const
{
	//In some cases the outer can be a transient package in the editor.
	check(GetOuter() && GetOuter()->IsA(ULockOnTargetComponent::StaticClass()));
	return StaticCast<ULockOnTargetComponent*>(GetOuter());
}

void ULockOnTargetModuleBase::InitializeModule(ULockOnTargetComponent* Instigator)
{
	check(!bIsInitialized);
	check(Instigator);

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

void ULockOnTargetModuleBase::Initialize(ULockOnTargetComponent* Instigator)
{
	//Unimplemented.
}

void ULockOnTargetModuleBase::BindToLockOn()
{
	LockOnTargetComponent->OnTargetLocked.AddDynamic(this, &ThisClass::OnTargetLockedPrivate);
	LockOnTargetComponent->OnTargetUnlocked.AddDynamic(this, &ThisClass::OnTargetUnlockedPrivate);
	LockOnTargetComponent->OnSocketChanged.AddDynamic(this, &ThisClass::OnSocketChangedPrivate);
	LockOnTargetComponent->OnTargetNotFound.AddDynamic(this, &ThisClass::OnTargetNotFoundPrivate);
}

void ULockOnTargetModuleBase::DeinitializeModule(ULockOnTargetComponent* Instigator)
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

void ULockOnTargetModuleBase::Deinitialize(ULockOnTargetComponent* Instigator)
{
	//Unimplemented.
}

void ULockOnTargetModuleBase::UnbindFromLockOn()
{
	LockOnTargetComponent->OnTargetLocked.RemoveDynamic(this, &ThisClass::OnTargetLockedPrivate);
	LockOnTargetComponent->OnTargetUnlocked.RemoveDynamic(this, &ThisClass::OnTargetUnlockedPrivate);
	LockOnTargetComponent->OnSocketChanged.RemoveDynamic(this, &ThisClass::OnSocketChangedPrivate);
	LockOnTargetComponent->OnTargetNotFound.RemoveDynamic(this, &ThisClass::OnTargetNotFoundPrivate);
}

void ULockOnTargetModuleBase::OnTargetLockedPrivate(UTargetingHelperComponent* Target, FName Socket)
{
	OnTargetLocked(Target, Socket);
	K2_OnTargetLocked(Target, Socket);
}

void ULockOnTargetModuleBase::OnTargetLocked(UTargetingHelperComponent* Target, FName Socket)
{
	//Unimplemented.
}

void ULockOnTargetModuleBase::OnTargetUnlockedPrivate(UTargetingHelperComponent* UnlockedTarget, FName Socket)
{
	OnTargetUnlocked(UnlockedTarget, Socket);
	K2_OnTargetUnlocked(UnlockedTarget, Socket);
}

void ULockOnTargetModuleBase::OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket)
{
	//Unimplemented.
}

void ULockOnTargetModuleBase::OnSocketChangedPrivate(UTargetingHelperComponent* CurrentTarget, FName NewSocket, FName OldSocket)
{
	OnSocketChanged(CurrentTarget, NewSocket, OldSocket);
	K2_OnSocketChanged(CurrentTarget, NewSocket, OldSocket);
}

void ULockOnTargetModuleBase::OnSocketChanged(UTargetingHelperComponent* CurrentTarget, FName NewSocket, FName OldSocket)
{
	//Unimplemented.
}

void ULockOnTargetModuleBase::OnTargetNotFoundPrivate()
{
	OnTargetNotFound();
	K2_OnTargetNotFound();
}

void ULockOnTargetModuleBase::OnTargetNotFound()
{
	//Unimplemented.
}

void ULockOnTargetModuleBase::Update(FVector2D PlayerInput, float DeltaTime)
{
	if (bWantsUpdate && (!bUpdateOnlyWhileLocked || GetLockOnTargetComponent()->IsTargetLocked()))
	{
		LOT_EVENT(ModuleUpdate, White);

		UpdateOverridable(PlayerInput, DeltaTime);
		K2_Update(PlayerInput, DeltaTime);
	}
}

void ULockOnTargetModuleBase::UpdateOverridable(FVector2D PlayerInput, float DeltaTime)
{
	//Unimplemented.
}

#if WITH_EDITOR

bool ULockOnTargetModuleBase::CanEditChange(const FProperty* InProperty) const
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
