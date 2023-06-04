// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "LockOnTargetTypes.h"
#include "LockOnTargetComponent.generated.h"

class ULockOnTargetComponent;
class UTargetComponent;
class AActor;
class UTargetHandlerBase;
class ULockOnTargetModuleBase;
class ULockOnTargetModuleProxy;

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnTargetLocked, ULockOnTargetComponent, OnTargetLocked, class UTargetComponent*, Target, FName, Socket);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnTargetUnlocked, ULockOnTargetComponent, OnTargetUnlocked, class UTargetComponent*, UnlockedTarget, FName, Socket);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnSocketChanged, ULockOnTargetComponent, OnSocketChanged, class UTargetComponent*, CurrentTarget, FName, NewSocket, FName, OldSocket);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnTargetNotFound, ULockOnTargetComponent, OnTargetNotFound, bool, bIsTargetLocked);

/**
 *	LockOnTargetComponent gives the locally controlled AActor the ability to find and store the Target along with the Socket.
 *	The Target can be controlled directly by the component or through an optional TargetHandler.
 *	The component may contain a set of optional Modules, which can be used to add custom cosmetic features.
 * 
 *	@see UTargetComponent, UTargetHandlerBase, ULockOnTargetModuleBase.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), HideCategories = (Components, Activation, Cooking, Sockets, Collision), meta = (BlueprintSpawnableComponent))
class LOCKONTARGET_API ULockOnTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	ULockOnTargetComponent();
	friend class FGDC_LockOnTarget; //Gameplay Debugger
	
private: /** Core Config */

	/** Can capture any Target. SetCanCaptureTarget() and CanCaptureTarget(). */
	UPROPERTY(EditAnywhere, Category = "Default Settings")
	bool bCanCaptureTarget;

	/** Special module that handles the Target. Get/SetTargetHandler(). */
	UPROPERTY(Instanced, EditDefaultsOnly, Category = "Default Settings", meta = (NoResetToDefault))
	TObjectPtr<UTargetHandlerBase> TargetHandlerImplementation;
	
	/** Set of customizable dynamic features. The order isn't defined. Add/RemoveModuleByClass(). */
	UPROPERTY(Instanced, EditDefaultsOnly, Category = "Modules", meta = (DisplayName = "Default Modules", NoResetToDefault))
	TArray<TObjectPtr<ULockOnTargetModuleBase>> Modules;

public: /** Input Config */

	/** When the InputBuffer overflows the threshold by the input, the switch method will be called. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player Input", meta = (ClampMin = 0.f, UIMin = 0.f))
	float InputBufferThreshold;

	/** InputBuffer is reset to 0.f at this frequency. Be careful with very small values. The InputBuffer can be reset too soon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player Input", meta = (ClampMin = 0.1f, UIMin = 0.1, Units="s"))
	float BufferResetFrequency;

	/** Player input axes are clamped by these values. X - min value, Y - max value. Usually these values should be opposite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player Input")
	FVector2D ClampInputVector;

	/** Block the Input for a while after the player has successfully performed an action. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player Input", meta = (ClampMin = 0.f, UIMin = 0.f, Units="s"))
	float InputProcessingDelay;

	/** Freeze the InputBuffer filling until the input reaches the UnfreezeThreshold after a successful switch. */
	UPROPERTY(EditDefaultsOnly, Category = "Player Input")
	bool bFreezeInputAfterSwitch;

	/** Unfreeze the InputBuffer filling if the input is less than the threshold. */
	UPROPERTY(EditDefaultsOnly, Category = "Player Input", meta = (ClampMin = 0.f, UIMin = 0.f, EditCondition = "bFreezeInputAfterSwitch", EditConditionHides))
	float UnfreezeThreshold;
	
public: /** Callbacks */

	/** Called if any Target has been successfully captured. */
	UPROPERTY(BlueprintAssignable, Category = "LockOnTargetComponent|Delegates")
	FOnTargetLocked OnTargetLocked;

	/** Called if any Target has been successfully released. */
	UPROPERTY(BlueprintAssignable, Category = "LockOnTargetComponent|Delegates")
	FOnTargetUnlocked OnTargetUnlocked;

	/** Called if the same Target with another Socket has been successfully captured. */
	UPROPERTY(BlueprintAssignable, Category = "LockOnTargetComponent|Delegates")
	FOnSocketChanged OnSocketChanged;
	
	/** Called if TargetHandler hasn't found any Target. */
	UPROPERTY(BlueprintAssignable, Category = "LockOnTargetComponent|Delegates")
	FOnTargetNotFound OnTargetNotFound;

private: /** Internal */

	//Information about the captured Target.
	UPROPERTY(Transient, ReplicatedUsing = OnTargetInfoUpdated)
	FTargetInfo CurrentTargetInternal;

	//The time during which the Target is captured.
	UPROPERTY(Transient, Replicated)
	float TargetingDuration;

	//Is any Target captured.
	bool bIsTargetLocked;

protected: /** Input Internal */

	bool bInputFrozen;
	FTimerHandle InputProcessingDelayHandler;
	FTimerHandle BufferResetHandler;
	FVector2D InputBuffer;
	FVector2D InputVector;

public: /** Polls */

	/** Is any Target locked. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	bool IsTargetLocked() const { return bIsTargetLocked; }

	/** Gets the currently locked TargetComponent, if exists, otherwise nullptr. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	UTargetComponent* GetTargetComponent() const { return IsTargetLocked() ? CurrentTargetInternal.TargetComponent : nullptr; }

	/** Gets the captured socket, if exists, otherwise NAME_None. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	FName GetCapturedSocket() const { return IsTargetLocked() ? CurrentTargetInternal.Socket : NAME_None; }

	/** Gets the currently locked AActor, if exists, otherwise nullptr. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	AActor* GetTargetActor() const;

	/** Gets the targeting duration in seconds. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	float GetTargetingDuration() const { return TargetingDuration; }

	/** Returns the World location of the captured Socket. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	FVector GetCapturedSocketLocation() const;

	/** Returns the FocusPoint location of the Target. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	FVector GetCapturedFocusLocation() const;

	/** Are we ready/able to capture Targets. Also checks for ownership and completeness of initialization. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Can Capture Target")
	bool CanCaptureTarget() const;

	/** Sets the new bCanCaptureTarget state. Clears the Target if necessary. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Can Capture Target")
	void SetCanCaptureTarget(bool bInCanCaptureTarget);

public: /** Class Main Interface */

	/** Tries to find a new Target through the TargetHandler if there is no locked Target, otherwise clears it. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Player Input")
	void EnableTargeting();

	/** Feeds Yaw input to the InputBuffer. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Player Input")
	void SwitchTargetYaw(float YawAxis);

	/** Feeds Pitch input to the InputBuffer. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Player Input")
	void SwitchTargetPitch(float PitchAxis);

	/** Tries to capture the Target manually. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Manual Handling", meta = (DisplayName = "Set Lock On Target Manual (by AActor)", AutoCreateRefTerm = "Socket"))
	void SetLockOnTargetManual(AActor* NewTarget, FName Socket = NAME_None);

	/** Tries to capture the Target manually. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Manual Handling", meta = (DisplayName = "Set Lock On Target Manual (by FTargetInfo)"))
	void SetLockOnTargetManualByInfo(const FTargetInfo& TargetInfo);

	/** Clears the Target manually. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Manual Handling")
	void ClearTargetManual();

	/** Passes the input to the TargetHandler, which should handle it and try to find a new Target. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Manual Handling")
	void SwitchTargetManual(FVector2D PlayerInput);

protected: /** Target Handling */

	//Performs to find a Target. Default implementation uses the TargetHandler.
	//@Warning - Potential reliable server call. Use carefully without wrapper.
	virtual void TryFindTarget(FVector2D OptionalInput = FVector2D(0.f));

	//Processes the Target returned by the TargetHandler.
	virtual void ProcessTargetHandlerResult(const FTargetInfo& TargetInfo);

	//Checks the Target state. Usually between updates.
	virtual void CheckTargetState(float DeltaTime);

	//Only the locally controlled Owner can control the Target.
	bool HasAuthorityOverTarget() const;

public: /** Target Validation */

	//Can the Target be captured.
	bool CanTargetBeCaptured(const FTargetInfo& TargetInfo) const;

	//Whether the target meets all the requirements for being captured.
	bool IsTargetValid(const UTargetComponent* Target) const;

protected: /** Target Synchronization */

	//Updates the CurrentTargetInternal locally and sends it to the server.
	void UpdateTargetInfo(const FTargetInfo& TargetInfo);

	//Updates the Target on the server.
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_UpdateTargetInfo(const FTargetInfo& TargetInfo);
	void Server_UpdateTargetInfo_Implementation(const FTargetInfo& TargetInfo);
	bool Server_UpdateTargetInfo_Validate(const FTargetInfo& TargetInfo);

private: /** Target State Workhorse */

	UFUNCTION()
	void OnTargetInfoUpdated(const FTargetInfo& OldTarget);
	
protected: /** System Callbacks */

	//Reacts on Target capture.
	virtual void OnTargetCaptured(const FTargetInfo& Target);
	
	//Reacts on Target release.
	virtual void OnTargetReleased(const FTargetInfo& Target);

	//Reacts on Socket change within the same Target.
	virtual void OnTargetSocketChanged(FName OldSocket);

public:

	//Handles Target exception/interrupt messages.
	virtual void ReceiveTargetException(ETargetExceptionType Exception);

public: /** Overrides */

	//UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected: /** Input */

	virtual void ProcessAnalogInput(float DeltaInput);
	FVector2D ConsumeInput();
	bool IsInputDelayActive() const;
	void ActivateInputDelay();
	bool CanInputBeProcessed(FVector2D Input);
	void ClearInputBuffer();

public: /** TargetHandler */

	/** Gets the current TargetHandler. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Target Handler")
	UTargetHandlerBase* GetTargetHandler() const { return TargetHandlerImplementation; }

	/** Creates and returns a new TargetHandler from class. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Target Handler", meta = (DeterminesOutputType = TargetHandlerClass))
	UTargetHandlerBase* SetTargetHandlerByClass(TSubclassOf<UTargetHandlerBase> TargetHandlerClass);

	template<typename Class>
	Class* SetTargetHandlerByClass()
	{
		static_assert(TPointerIsConvertibleFromTo<Class, const UTargetHandlerBase>::Value, "Invalid template param.");
		return static_cast<Class*>(SetTargetHandlerByClass(Class::StaticClass()));
	}

	/** Destroys TargetHandler. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Target Handler")
	void ClearTargetHandler();

public: /** Modules */

	/** Gets all modules. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Modules")
	const TArray<ULockOnTargetModuleBase*>& GetAllModules() const { return Modules; }

	/** Finds a module by class. Complexity O(n). */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Modules", meta = (DeterminesOutputType = ModuleClass))
	ULockOnTargetModuleBase* FindModuleByClass(TSubclassOf<ULockOnTargetModuleBase> ModuleClass) const;

	template<typename Class>
	Class* FindModuleByClass() const
	{
		static_assert(TPointerIsConvertibleFromTo<Class, const ULockOnTargetModuleBase>::Value, "Invalid template param.");
		return static_cast<Class*>(FindModuleByClass(Class::StaticClass()));
	}

	/** Creates and returns a new module by class. Complexity amortized constant. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Modules", meta = (DeterminesOutputType = ModuleClass))
	ULockOnTargetModuleBase* AddModuleByClass(TSubclassOf<ULockOnTargetModuleBase> ModuleClass);

	template<typename Class>
	Class* AddModuleByClass()
	{
		static_assert(TPointerIsConvertibleFromTo<Class, const ULockOnTargetModuleBase>::Value, "Invalid template param.");
		return static_cast<Class*>(AddModuleByClass(Class::StaticClass()));
	}

	/** Destroys a module by class. Complexity O(n). */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Modules")
	bool RemoveModuleByClass(TSubclassOf<ULockOnTargetModuleBase> ModuleClass);

	template<typename Class>
	void RemoveModuleByClass()
	{
		static_assert(TPointerIsConvertibleFromTo<Class, const ULockOnTargetModuleBase>::Value, "Invalid template param.");
		return RemoveModuleByClass(Class::StaticClass());
	}

	/** Destroys all modules. Complexity O(n). */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Modules")
	void RemoveAllModules();

private: /** Subobject General. */

	void InitializeSubobject(ULockOnTargetModuleProxy* Subobject);
	void DestroySubobject(ULockOnTargetModuleProxy* Subobject);
	TArray<ULockOnTargetModuleProxy*, TInlineAllocator<8>> GetAllSubobjects() const;

	template<typename Func>
	void ForEachSubobject(Func InFunc)
	{
		for (auto* const Subobject : GetAllSubobjects())
		{
			InFunc(Subobject);
		}
	}
};
