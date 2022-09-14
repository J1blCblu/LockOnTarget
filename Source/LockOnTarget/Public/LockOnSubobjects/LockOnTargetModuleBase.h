// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LockOnTargetModuleBase.generated.h"

class ULockOnTargetComponent;
class UTargetingHelperComponent;

/**
 *	LockOnTarget Module is a base dynamic unit that:
 *	 - Implements LockOnTargetComponent callbacks.
 *	 - Can add some functionality.
 *	 - Can be removed/added at runtime.
 *	Isn't replicated.
 * 
 *	@see UControlRotationModule, UCameraZoomModule, UWidgetModule.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Abstract, EditInlineNew, DefaultToInstanced, HideDropdown)
class LOCKONTARGET_API ULockOnTargetModuleBase : public UObject
{
	GENERATED_BODY()

public:
	ULockOnTargetModuleBase();
	friend ULockOnTargetComponent;

private:
	TWeakObjectPtr<ULockOnTargetComponent> LockOnTargetComponent;
	uint8 bIsInitialized : 1;

public:
	/** Wants to call the Update method. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Module Config")
	uint8 bWantsUpdate : 1;

	/** Should this module call the Update method only while any Target is locked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Module Config", meta = (EditCondition = "bWantsUpdate", EditConditionHides))
	uint8 bUpdateOnlyWhileLocked : 1;

protected: //Module Interface

	/** It's guarantied that component is valid. But in some cases in editor this is not true. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetModule", meta = (BlueprintProtected))
	ULockOnTargetComponent* GetLockOnTargetComponent() const;

	ULockOnTargetComponent* GetLockOnTargetComponentFromOuter() const;

	virtual void Initialize(ULockOnTargetComponent* Instigator);
	
	virtual void Deinitialize(ULockOnTargetComponent* Instigator);
	
	void Update(FVector2D PlayerInput, float DeltaTime);
	
	virtual void UpdateOverridable(FVector2D PlayerInput, float DeltaTime);

	UFUNCTION()
	virtual void OnTargetLocked(UTargetingHelperComponent* Target, FName Socket);

	UFUNCTION()
	virtual void OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket);

	UFUNCTION()
	virtual void OnSocketChanged(UTargetingHelperComponent* CurrentTarget, FName NewSocket, FName OldSocket);

	UFUNCTION()
	virtual void OnTargetNotFound();

protected: //UObject overrides
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

public:
	virtual UWorld* GetWorld() const override;

private:
	void BindToLockOn();
	void UnbindFromLockOn();

protected: //BP callbacks

	/** Called once to initialize Module. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "Initialize")
	void K2_Initialize();
	
	/** Called once to deinitialize Module. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName = "Deinitialize")
	void K2_Deinitialize();
	
	/** Called when the owning LockOnTargetComponent captured the Target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Module Base", DisplayName="On Target Locked")
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