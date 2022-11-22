// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LockOnTargetModuleBase.generated.h"

class ULockOnTargetComponent;
class UTargetingHelperComponent;
class ULockOnTargetModuleProxy;
class ULockOnTargetModuleBase;

/** 
 * Just a proxy class to exclude TargetHandlers from the LockOnTarget module hierarchy.
 * Shouldn't be directly referenced! Just to save shared functionality and dependencies.
 */
UCLASS(Abstract, ClassGroup(LockOnTarget), HideDropdown)
class LOCKONTARGET_API ULockOnTargetModuleProxy : public UObject
{
	GENERATED_BODY()

public:
	ULockOnTargetModuleProxy();
	friend ULockOnTargetComponent;

private:
	TWeakObjectPtr<ULockOnTargetComponent> LockOnTargetComponent;
	bool bIsInitialized;

public:
	/** Wants to call the Update method. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Module Config")
	bool bWantsUpdate;

	/** Should this module call the Update method only while any Target is locked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Module Config", meta = (EditCondition = "bWantsUpdate", EditConditionHides))
	bool bUpdateOnlyWhileLocked;

protected: //Module Interface

	/** It's guarantied that the component is valid. But in some cases in the editor this is not true. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetModule", meta = (BlueprintProtected))
	ULockOnTargetComponent* GetLockOnTargetComponent() const;

	//All descriptions can be found in the "BP callbacks" section below.
	virtual void Initialize(ULockOnTargetComponent* Instigator);
	virtual void Deinitialize(ULockOnTargetComponent* Instigator);
	virtual void UpdateOverridable(FVector2D PlayerInput, float DeltaTime);
	virtual void OnTargetLocked(UTargetingHelperComponent* Target, FName Socket);
	virtual void OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket);
	virtual void OnSocketChanged(UTargetingHelperComponent* CurrentTarget, FName NewSocket, FName OldSocket);
	virtual void OnTargetNotFound();

protected: //UObject overrides
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

public:
	virtual UWorld* GetWorld() const override;

private:
	ULockOnTargetComponent* GetLockOnTargetComponentFromOuter() const;
	void BindToLockOn();
	void UnbindFromLockOn();

private: //Should only be used by the LockOnTargetComponent.
	void InitializeModule(ULockOnTargetComponent* Instigator);
	void DeinitializeModule(ULockOnTargetComponent* Instigator);
	void Update(FVector2D PlayerInput, float DeltaTime);

	UFUNCTION()
	virtual void OnTargetLockedPrivate(UTargetingHelperComponent* Target, FName Socket);

	UFUNCTION()
	virtual void OnTargetUnlockedPrivate(UTargetingHelperComponent* UnlockedTarget, FName Socket);

	UFUNCTION()
	virtual void OnSocketChangedPrivate(UTargetingHelperComponent* CurrentTarget, FName NewSocket, FName OldSocket);

	UFUNCTION()
	virtual void OnTargetNotFoundPrivate();

protected: //BP callbacks

	/** Called once to initialize Module. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "Initialize")
	void K2_Initialize();

	/** Called once to deinitialize Module. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "Deinitialize")
	void K2_Deinitialize();

	/** Called when the owning LockOnTargetComponent captured the Target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "On Target Locked")
	void K2_OnTargetLocked(UTargetingHelperComponent* Target, FName Socket);

	/** Called when the owning LockOnTargetComponent released the Target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "On Target Unlocked")
	void K2_OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket);

	/** Called when the Socket is changed within the current Target, if it has more than 1 socket. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "On Socket Changed")
	void K2_OnSocketChanged(UTargetingHelperComponent* CurrentTarget, FName NewSocket, FName OldSocket);

	/** Called when the owning LockOnTargetComponent didn't find the Target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "On Target Not Found")
	void K2_OnTargetNotFound();

	/** Basic tick method. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "Update")
	void K2_Update(FVector2D PlayerInput, float DeltaTime);
};

/**
 *	LockOnTarget Module is a base dynamic unit that:
 *	 - Implements LockOnTargetComponent callbacks.
 *	 - Can add some functionality.
 *	 - Can be removed/added at runtime.
 *	Isn't replicated.
 * 
 * Lifecycle is managed by the LockOnTargetComponent, 
 * so it's always safe to refer to it at runtime.
 * 
 *	@see UControlRotationModule, UCameraZoomModule, UWidgetModule.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Abstract, EditInlineNew, DefaultToInstanced, HideDropdown, CustomConstructor)
class LOCKONTARGET_API ULockOnTargetModuleBase : public ULockOnTargetModuleProxy
{
	GENERATED_BODY()

public:

	ULockOnTargetModuleBase(){}

	//Some code related only to modules, not including TargetHandler.

};
