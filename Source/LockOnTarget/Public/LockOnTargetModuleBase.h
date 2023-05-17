// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LockOnTargetModuleBase.generated.h"

class ULockOnTargetComponent;
class UTargetComponent;
class ULockOnTargetModuleProxy;
class ULockOnTargetModuleBase;
class AController;
class APlayerController;

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

private: /** Internal */

	TWeakObjectPtr<ULockOnTargetComponent> LockOnTargetComponent;
	mutable TWeakObjectPtr<AController> CachedController;
	uint8 bIsInitialized : 1;

public: /** Polls */

	/** Has the module been successfully initialized. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetModule")
	bool IsInitialized() const { return bIsInitialized; }

	/** Returns the owning LockOnTargetComponent. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetModule")
	ULockOnTargetComponent* GetLockOnTargetComponent() const;

	/** Returns the owning Controller. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetModule")
	AController* GetController() const;

	/** Returns the owning PlayerController. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetModule")
	APlayerController* GetPlayerController() const;

protected: /** Module Interface */

	//Module lifetime.
	virtual void Initialize(ULockOnTargetComponent* Instigator);
	virtual void Deinitialize(ULockOnTargetComponent* Instigator);
	virtual void Update(float DeltaTime);
	
	//LockOnTargetComponent callbacks.
	virtual void OnTargetLocked(UTargetComponent* Target, FName Socket);
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket);
	virtual void OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket);
	virtual void OnTargetNotFound(bool bIsTargetLocked);

private: /** Internal */

	AController* TryFindController() const;

public: /** Overrides */

	//UObject.
	virtual UWorld* GetWorld() const override;

protected: /** BP */

	/** Called once to initialize Module. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "Initialize")
	void K2_Initialize(ULockOnTargetComponent* Instigator);

	/** Called once to deinitialize Module. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "Deinitialize")
	void K2_Deinitialize(ULockOnTargetComponent* Instigator);

	/** Called when the owning LockOnTargetComponent captured the Target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "On Target Locked")
	void K2_OnTargetLocked(UTargetComponent* Target, FName Socket);

	/** Called when the owning LockOnTargetComponent released the Target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "On Target Unlocked")
	void K2_OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket);

	/** Called when the Socket is changed within the current Target, if it has more than 1 socket. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "On Socket Changed")
	void K2_OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket);

	/** Called when the owning LockOnTargetComponent didn't find the Target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "On Target Not Found")
	void K2_OnTargetNotFound(bool bIsTargetLocked);

	/** Basic tick method. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "Update")
	void K2_Update(float DeltaTime);
};

/**
 * Adds some optional dynamic functionality to the owning LockOnTargetComponent in such a way 
 * that it can be replaced with an analogue from another system.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Abstract, EditInlineNew, DefaultToInstanced, HideDropdown, CustomConstructor)
class LOCKONTARGET_API ULockOnTargetModuleBase : public ULockOnTargetModuleProxy
{
	GENERATED_BODY()

public:

	ULockOnTargetModuleBase() = default;

	//Some code related only to modules, not including TargetHandler.
};
