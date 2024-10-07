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
class ULockOnTargetExtensionBase;
class ULockOnTargetExtensionProxy;

struct FFindTargetRequestParams;
struct FFindTargetRequestResponse;

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnTargetLocked, ULockOnTargetComponent, OnTargetLocked, class UTargetComponent*, Target, FName, Socket);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnTargetUnlocked, ULockOnTargetComponent, OnTargetUnlocked, class UTargetComponent*, UnlockedTarget, FName, Socket);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnSocketChanged, ULockOnTargetComponent, OnSocketChanged, class UTargetComponent*, CurrentTarget, FName, NewSocket, FName, OldSocket);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnTargetNotFound, ULockOnTargetComponent, OnTargetNotFound, bool, bIsTargetLocked);

/**
 *	LockOnTargetComponent gives a locally controlled APawn the ability to find and store a Target.
 *	The Target can be controlled directly by the component or through an optional TargetHandler.
 *	Independent logic such as widget attachment, camera focusing and etc. is encapsulated inside LockOnTargetExtensions.
 * 
 *	@see UTargetComponent, UTargetHandlerBase, ULockOnTargetExtensionBase.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), HideCategories = (Components, Activation, Cooking, Sockets, Collision), meta = (BlueprintSpawnableComponent))
class LOCKONTARGET_API ULockOnTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	//@TODO: Rework input processing to support different update rates. Look at GF/PawnMovementComponent.h.

	ULockOnTargetComponent();
	friend class FGameplayDebuggerCategory_LockOnTarget; //Gameplay Debugger
	
private: /** Core Config */

	/** Can capture any Target. SetCanCaptureTarget() and CanCaptureTarget(). */
	UPROPERTY(EditAnywhere, Category = "Default Settings")
	bool bCanCaptureTarget;

	/** Special extension that handles the Target. Get/SetTargetHandler(). */
	UPROPERTY(Instanced, EditDefaultsOnly, Category = "Default Settings")
	TObjectPtr<UTargetHandlerBase> TargetHandlerImplementation;
	
	/** Set of customizable independent features. Add/RemoveExtensionByClass(). */
	UPROPERTY(Instanced, EditDefaultsOnly, Category = "Extensions", meta = (DisplayName = "Default Extensions", NoResetToDefault))
	TArray<TObjectPtr<ULockOnTargetExtensionBase>> Extensions;

public: /** Input Config */

	/** When the InputBuffer overflows the threshold by the input, the switch method will be called. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player Input", meta = (ClampMin = 0.f))
	float InputBufferThreshold;

	/** InputBuffer is reset to 0.f at this frequency. Be careful with very small values. The InputBuffer can be reset too soon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player Input", meta = (ClampMin = 0.1f, Units="s"))
	float BufferResetFrequency;

	/** Player input axes are clamped by these values. X - min value, Y - max value. Usually these values should be opposite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player Input")
	FVector2D ClampInputVector;

	/** Block the Input for a while after the player has successfully performed an action. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player Input", meta = (ClampMin = 0.f, Units="s"))
	float InputProcessingDelay;

	/** Freeze the InputBuffer filling until the input reaches the UnfreezeThreshold after a successful switch. */
	UPROPERTY(EditDefaultsOnly, Category = "Player Input")
	bool bUseInputFreezing;

	/** Unfreeze the InputBuffer filling if the input is less than the threshold. */
	UPROPERTY(EditDefaultsOnly, Category = "Player Input", meta = (ClampMin = 0.f, EditCondition = "bUseInputFreezing", EditConditionHides))
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

	/** Returns the currently locked TargetComponent, if exists, otherwise nullptr. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	UTargetComponent* GetTargetComponent() const { return IsTargetLocked() ? CurrentTargetInternal.TargetComponent : nullptr; }

	/** Returns the captured socket, if exists, otherwise NAME_None. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	FName GetCapturedSocket() const { return IsTargetLocked() ? CurrentTargetInternal.Socket : NAME_None; }

	/** Returns the currently locked AActor, if exists, otherwise nullptr. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	AActor* GetTargetActor() const;

	/** Returns the targeting duration in seconds. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	float GetTargetingDuration() const { return TargetingDuration; }

	/** Returns the World location of the captured Socket. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	FVector GetCapturedSocketLocation() const;

	/** Returns the 'focus point' location of the Target. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Polls")
	FVector GetCapturedFocusPointLocation() const;

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
	virtual void RequestFindTarget(const FFindTargetRequestParams& RequestParams);

	//Processes the Target returned by the TargetHandler.
	virtual void ProcessTargetHandlerResponse(const FFindTargetRequestResponse& Response);

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
	virtual void NotifyTargetCaptured(const FTargetInfo& Target);
	
	//Reacts on Target release.
	virtual void NotifyTargetReleased(const FTargetInfo& Target);

	//Reacts on Socket change within the same Target.
	virtual void NotifyTargetSocketChanged(FName OldSocket);

	//Reacts on Socket change within the same Target.
	virtual void NotifyTargetNotFound();

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

	/** Returns the current TargetHandler. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Target Handler")
	UTargetHandlerBase* GetTargetHandler() const { return TargetHandlerImplementation; }

	/**
	 * Updates the default TargetHandlerImplementation.
	 * 
	 * @warning Should only be used during construction! At runtime, use SetTargetHandlerByClass().
	 */
	void SetDefaultTargetHandler(UTargetHandlerBase* InDefaultTargetHandler);

	/** Creates and returns a new TargetHandler from class. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Target Handler", meta = (DeterminesOutputType = TargetHandlerClass))
	UTargetHandlerBase* SetTargetHandlerByClass(TSubclassOf<UTargetHandlerBase> TargetHandlerClass);

	template<typename Class>
	Class* SetTargetHandlerByClass()
	{
		static_assert(TPointerIsConvertibleFromTo<Class, const UTargetHandlerBase>::Value, "Invalid template param.");
		return static_cast<Class*>(SetTargetHandlerByClass(Class::StaticClass()));
	}

	/** Destroys the current TargetHandler. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Target Handler")
	void ClearTargetHandler();

public: /** Extensions */

	/** Returns all extensions. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Extensions")
	const TArray<ULockOnTargetExtensionBase*>& GetAllExtensions() const { return Extensions; }

	/** Finds an extension by class. Complexity O(n). */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Extensions", meta = (DeterminesOutputType = ExtensionClass))
	ULockOnTargetExtensionBase* FindExtensionByClass(TSubclassOf<ULockOnTargetExtensionBase> ExtensionClass) const;

	template<typename Class>
	Class* FindExtensionByClass() const
	{
		static_assert(TPointerIsConvertibleFromTo<Class, const ULockOnTargetExtensionBase>::Value, "Invalid template param.");
		return static_cast<Class*>(FindExtensionByClass(Class::StaticClass()));
	}

	/**
	 * Adds a new default extension to the DefaultExtensions.
	 * 
	 * @warning Should only be used during construction! At runtime, use SetTargetHandlerByClass().
	 */
	void AddDefaultExtension(ULockOnTargetExtensionBase* InDefaultExtension);

	/** Creates and returns a new extension by class. Complexity amortized constant. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Extensions", meta = (DeterminesOutputType = ExtensionClass))
	ULockOnTargetExtensionBase* AddExtensionByClass(TSubclassOf<ULockOnTargetExtensionBase> ExtensionClass);

	template<typename Class>
	Class* AddExtensionByClass()
	{
		static_assert(TPointerIsConvertibleFromTo<Class, const ULockOnTargetExtensionBase>::Value, "Invalid template param.");
		return static_cast<Class*>(AddExtensionByClass(Class::StaticClass()));
	}

	/** Destroys an extension by class. Complexity O(n). */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Extensions")
	bool RemoveExtensionByClass(TSubclassOf<ULockOnTargetExtensionBase> ExtensionClass);

	template<typename Class>
	bool RemoveExtensionByClass()
	{
		static_assert(TPointerIsConvertibleFromTo<Class, const ULockOnTargetExtensionBase>::Value, "Invalid template param.");
		return RemoveExtensionByClass(Class::StaticClass());
	}

	/** Destroys all extensions. Complexity O(n). */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Extensions")
	void RemoveAllExtensions();

private: /** Subobject General. */

	void InitializeSubobject(ULockOnTargetExtensionProxy* Subobject);
	void DestroySubobject(ULockOnTargetExtensionProxy* Subobject);
	TArray<ULockOnTargetExtensionProxy*, TInlineAllocator<8>> GetAllSubobjects() const;

	template<typename Func>
	void ForEachSubobject(Func InFunc)
	{
		for (auto* const Subobject : GetAllSubobjects())
		{
			InFunc(Subobject);
		}
	}
};
