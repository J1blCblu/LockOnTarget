// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineBaseTypes.h"
#include "LockOnTargetExtensionBase.generated.h"

class ULockOnTargetComponent;
class UTargetComponent;
class ULockOnTargetExtensionProxy;
class ULockOnTargetExtensionBase;
class AController;
class APlayerController;
class APawn;

/** 
 * Simple tick function that calls ULockOnTargetExtensionProxy::Update()
 */
USTRUCT()
struct LOCKONTARGET_API FLockOnTargetExtensionTickFunction : public FTickFunction
{
	GENERATED_BODY()

public:

	ULockOnTargetExtensionProxy* TargetExtension;

public: //Overrides

	//FTickFunction
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;
};

template<>
struct TStructOpsTypeTraits<FLockOnTargetExtensionTickFunction> : public TStructOpsTypeTraitsBase2<FLockOnTargetExtensionTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

/** 
 * Just a proxy class to exclude TargetHandlers from the LockOnTarget extensions hierarchy.
 * Shouldn't be directly referenced! Just to save shared functionality and dependencies.
 */
UCLASS(Abstract, ClassGroup(LockOnTarget), HideDropdown)
class LOCKONTARGET_API ULockOnTargetExtensionProxy : public UObject
{
	GENERATED_BODY()

public:

	ULockOnTargetExtensionProxy();

public:

	/** Main tick function for the Extension. */
	UPROPERTY(EditDefaultsOnly, Category="Tick")
	FLockOnTargetExtensionTickFunction ExtensionTick;

private: /** Internal */

	ULockOnTargetComponent* LockOnTargetComponent;
	uint8 bIsInitialized : 1;

public: /** Polls */

	/** Has the extension been successfully initialized. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetExtension")
	bool IsInitialized() const { return bIsInitialized; }

	/** Returns the owning LockOnTargetComponent. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetExtension")
	ULockOnTargetComponent* GetLockOnTargetComponent() const;

	/** Returns the owning Controller. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetExtension")
	AController* GetInstigatorController() const;

	/** Returns the owning PlayerController. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetExtension")
	APlayerController* GetPlayerController() const;

	/** Returns the owning Pawn. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetExtension")
	APawn* GetInstigatorPawn() const;

public: /** Tick */

	/** Whether the tick is enabled. */
	UFUNCTION(BlueprintPure, Category="LockOnTargetExtension|Tick")
	bool IsTickEnabled() const { return ExtensionTick.IsTickFunctionEnabled(); }

	/** Set the new tick state. */
	UFUNCTION(BlueprintCallable, Category="LockOnTargetExtension|Tick")
	void SetTickEnabled(bool bInTickEnabled);

public: /** Extension Interface */

	//Extension lifetime.
	virtual void Initialize(ULockOnTargetComponent* Instigator);
	virtual void Deinitialize(ULockOnTargetComponent* Instigator);
	virtual void Update(float DeltaTime);
	
	//LockOnTargetComponent callbacks.
	virtual void OnTargetLocked(UTargetComponent* Target, FName Socket);
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket);
	virtual void OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket);
	virtual void OnTargetNotFound(bool bIsTargetLocked);

public: /** Overrides */

	//UObject.
	virtual UWorld* GetWorld() const override;

protected: /** BP */

	/** Called once to initialize extension. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ExtensionBase", DisplayName = "Initialize")
	void K2_Initialize(ULockOnTargetComponent* Instigator);

	/** Called once to deinitialize extension. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ExtensionBase", DisplayName = "Deinitialize")
	void K2_Deinitialize(ULockOnTargetComponent* Instigator);

	/** Called when the owning LockOnTargetComponent captured the Target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ExtensionBase", DisplayName = "On Target Locked")
	void K2_OnTargetLocked(UTargetComponent* Target, FName Socket);

	/** Called when the owning LockOnTargetComponent released the Target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ExtensionBase", DisplayName = "On Target Unlocked")
	void K2_OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket);

	/** Called when the Socket is changed within the current Target, if it has more than 1 socket. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ExtensionBase", DisplayName = "On Socket Changed")
	void K2_OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket);

	/** Called when the owning LockOnTargetComponent didn't find the Target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ExtensionBase", DisplayName = "On Target Not Found")
	void K2_OnTargetNotFound(bool bIsTargetLocked);

	/** Basic tick method. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ExtensionBase", DisplayName = "Update")
	void K2_Update(float DeltaTime);
};

/**
 * Adds some optional dynamic functionality to the owning LockOnTargetComponent.
 * 
 * @see UWidgetExtension
 * @see UControllerRotationExtension
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Abstract, EditInlineNew, DefaultToInstanced, HideDropdown, CustomConstructor)
class LOCKONTARGET_API ULockOnTargetExtensionBase : public ULockOnTargetExtensionProxy
{
	GENERATED_BODY()

public:

	ULockOnTargetExtensionBase() = default;
};
