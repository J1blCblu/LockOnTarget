// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TargetInfo.h"
#include "TemplateUtilities.h"
#include "LockOnTargetComponent.generated.h"

class UTargetingHelperComponent;
class AController;
class APlayerController;
class AActor;
class UTargetHandlerBase;
class ULockOnTargetModuleBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetLocked, class UTargetingHelperComponent*, Target, FName, Socket);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetUnlocked, class UTargetingHelperComponent*, UnlockedTarget, FName, Socket);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSocketChanged, class UTargetingHelperComponent*, CurrentTarget, FName, NewSocket, FName, OldSocket);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetNotFound);

/**
 *	LockOnTargetComponent gives the Owner(Player) an ability to find a Target and store it.
 * 
 *	LockOnTargetComponent is a wrapper over the next systems:
 *	1. Target storage (TargetingHelperComponent and Socket). Synchronized with the server.
 *	2. Processing the input. Processed locally.
 *	3. TargetHandler - used for Target handling(find, maintenance). Processed locally.
 *	4. Modules - piece of functionality that can be dynamically added/removed.
 * 
 *	Network Design Philosophy:
 *	Lock On Target is just a system which finds a Target, stores and synchronizes it over the network. 
 *	Finding the Target is not a quick operation to process it on the server for each player. 
 *	Due to this and network relevancy opportunities (not relevant Target doesn't exist in the world)
 *	finding the Target processed locally on the owning client.
 * 
 *	@see UTargetingHelperComponent, UTargetHandlerBase, ULockOnTargetModuleBase.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), HideCategories = (Components, Activation, Cooking, ComponentTick, Sockets, Collision), meta = (BlueprintSpawnableComponent))
class LOCKONTARGET_API ULockOnTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULockOnTargetComponent();
	friend class FGDC_LockOnTarget; //Gameplay Debugger
	
private:
	/** Can this Component capture a Target. Not replicated. Have affect only for the local owning client. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Settings", meta = (AllowPrivateAccess))
	uint8 bCanCaptureTarget : 1;

	/** Special module that handles the Target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Default Settings", meta = (AllowPrivateAccess, NoResetToDefault))
	TObjectPtr<UTargetHandlerBase> TargetHandlerImplementation;
	
	/** Set of default Modules. Will be instantiated on all machines (may be changed in the future). Update order is not defined. */
	UPROPERTY(EditDefaultsOnly, Category = "Modules", meta = (DisplayName = "Default Modules", NoResetToDefault))
	TArray<TObjectPtr<ULockOnTargetModuleBase>> Modules;

public:
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

	/** Freeze InputBuffer filling until the input reaches the UnfreezeThreshold after a successful switch. */
	UPROPERTY(EditDefaultsOnly, Category = "Player Input")
	uint8 bFreezeInputAfterSwitch : 1;

	/** Unfreeze InputBuffer filling if the input is less than the threshold. */
	UPROPERTY(EditDefaultsOnly, Category = "Player Input", meta = (ClampMin = 0.f, UIMin = 0.f, EditCondition = "bFreezeInputAfterSwitch", EditConditionHides))
	float UnfreezeThreshold;

	/** Called when the Target is locked. */
	UPROPERTY(BlueprintAssignable, Transient, Category = "LockOnTargetComponent")
	FOnTargetLocked OnTargetLocked;

	/** Called when the Target is unlocked. */
	UPROPERTY(BlueprintAssignable, Transient, Category = "LockOnTargetComponent")
	FOnTargetUnlocked OnTargetUnlocked;

	/** Called when the Socket is changed within the current Target, if it has more than 1. */
	UPROPERTY(BlueprintAssignable, Transient, Category = "LockOnTargetComponent")
	FOnSocketChanged OnSocketChanged;
	
	/** Called if the Target isn't found after finding. */
	UPROPERTY(BlueprintAssignable, Transient, Category = "LockOnTargetComponent")
	FOnTargetNotFound OnTargetNotFound;

private:
	//Information about the captured Target (TargetingHelperComponent and Socket).
	UPROPERTY(Transient, ReplicatedUsing = OnTargetInfoUpdated)
	FTargetInfo CurrentTargetInternal;

	//The time during which the Target is captured.
	UPROPERTY(Transient, Replicated)
	float TargetingDuration;

	//Is any HelperComponent is captured.
	uint8 bIsTargetLocked : 1;

public: /** UActorComponent overrides */

	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public: /** Class Main Interface */

	/** Main method of capturing and clearing a Target. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Player Input")
	void EnableTargeting();

	/** Feed Yaw input to the InputBuffer. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Player Input")
	void SwitchTargetYaw(float YawAxis);

	/** Feed Pitch input to the InputBuffer. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Player Input")
	void SwitchTargetPitch(float PitchAxis);

public: /** Manual Target handling */

	/** Manual Target switching. Useful for custom input processing. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Manual Handling")
	void SwitchTargetManual(FVector2D PlayerInput);

	/** Manual Target capturing. Useful for custom input processing. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Manual Handling", meta = (AutoCreateRefTerm = "Socket"))
	void SetLockOnTargetManual(AActor* NewTarget, FName Socket = NAME_None);

	/** 
	 * Manual Target clearing. Useful for custom input processing. 
	 * If bAutoFindTarget is used then mark the current Target somehow as 'can't be targeted' otherwise it can be captured again.
	 */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Manual Handling")
	void ClearTargetManual(bool bAutoFindNewTarget = false);

private: /** Processing the Target */

	//Performs to find the Target.
	virtual void FindTarget(FVector2D OptionalInput = FVector2D(0.f));

	//Used to reduce the number of reliable calls to the server.
	void ProcessTargetHandlerResult(const FTargetInfo& TargetInfo);

	//Can continue to capture the Target.
	bool CanContinueTargeting();

	//Updates the CurrentTargetInternal locally and sends it to the server.
	void UpdateTargetInfo(const FTargetInfo& TargetInfo);

	//Sets the desired Target on the server.
	UFUNCTION(Server, Reliable /*, WithValidation */)
	void Server_UpdateTargetInfo(const FTargetInfo& TargetInfo);

	//Main method which controls the Target state.
	UFUNCTION()
	void OnTargetInfoUpdated(const FTargetInfo& OldTarget);
	
	void CaptureTarget(const FTargetInfo& Target);
	void ReleaseTarget(const FTargetInfo& Target);

	//Called to change the Target's socket (if the Target has > 1 socket).
	virtual void UpdateTargetSocket(FName OldSocket);
	
private: /** Input processing */

	FTimerHandle InputProcessingDelayHandler;
	FTimerHandle BufferResetHandler;
	FVector2D InputBuffer;
	FVector2D InputVector;
	mutable bool bInputFrozen;

	void ProcessAnalogInput(float DeltaInput);
	bool IsInputDelayActive() const;
	void ActivateInputDelay();
	bool CanInputBeProcessed(float PlayerInput) const;
	void ClearInputBuffer();

public: /** Modules */

	/** Get all modules associated with the component. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Modules")
	const TArray<ULockOnTargetModuleBase*>& GetAllModules() const;

	/** Find a module by class. Complexity O(n). */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent|Modules", meta = (DeterminesOutputType = ModuleClass))
	ULockOnTargetModuleBase* FindModuleByClass(TSubclassOf<ULockOnTargetModuleBase> ModuleClass) const;

	template<typename Class>
	typename TEnableIf<TIsModule_V<Class>, Class>::Type* FindModuleByClass()
	{
		return static_cast<Class*>(FindModuleByClass(Class::StaticClass()));
	}

	/** Create and Initialize a new Module. Complexity amortized constant. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Modules", meta = (DeterminesOutputType = ModuleClass))
	ULockOnTargetModuleBase* AddModuleByClass(TSubclassOf<ULockOnTargetModuleBase> ModuleClass);

	template<typename Class>
	typename TEnableIf<TIsModule_V<Class>, Class>::Type* AddModuleByClass()
	{
		return static_cast<Class*>(AddModuleByClass(Class::StaticClass()));
	}

	/** Deinitialize and Destroy a module by class. Complexity O(n). */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Modules")
	bool RemoveModuleByClass(TSubclassOf<ULockOnTargetModuleBase> ModuleClass);

	template<typename Class>
	typename TEnableIf<TIsModule_V<Class>, bool>::Type RemoveModuleByClass()
	{
		return RemoveModuleByClass(Class::StaticClass());
	}

	/** Deinitialize and Destroy all modules. Complexity O(n). */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent|Modules")
	void RemoveAllModules();

private:
	void UpdateModules(float DeltaTime);
	
public: /** Misc */

	/** Set the CanCaptureTarget state. If set to false while locked then will unlock the Target. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent")
	void SetCanCaptureTarget(bool bInCanCaptureTarget);

	/** Can capture any Target. */
	bool GetCanCaptureTarget() const { return bCanCaptureTarget; };

	/** Set a new TargetHandler by ref. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent")
	void SetTargetHandler(UTargetHandlerBase* NewTargetHandler);

	/** Get current TargetHandler implementation. */
	UTargetHandlerBase* GetTargetHandler() const { return TargetHandlerImplementation; }

	/** Create, assign and return (for setup purposes) a new TargetHandler from class. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent", meta = (DeterminesOutputType = TargetHandlerClass))
	UTargetHandlerBase* SetTargetHandlerByClass(TSubclassOf<UTargetHandlerBase> TargetHandlerClass);

	template<typename Class>
	typename TEnableIf<TIsModule_V<Class>, Class>::Type* SetTargetHandlerByClass()
	{
		SetTargetHandlerByClass(Class::StaticClass());
	}

	/** Is any Target locked. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent")
	bool IsTargetLocked() const { return bIsTargetLocked; };

	/** Current locked Target's TargetingHelperComponent. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent")
	UTargetingHelperComponent* GetHelperComponent() const { return CurrentTargetInternal.HelperComponent; }

	/** Captured socket, if exists. NAME_None otherwise. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent")
	FName GetCapturedSocket() const { return CurrentTargetInternal.SocketForCapturing; }

	/** Currently locked Actor. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent")
	AActor* GetTarget() const;

	/** Targeting duration in sec. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent")
	float GetTargetingDuration() const { return TargetingDuration; }

	/** World location of the locked Target's socket. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent")
	FVector GetCapturedSocketLocation() const;

	/** World location of the locked Target's socket with offset. */
	UFUNCTION(BlueprintPure, Category = "LockOnTargetComponent")
	FVector GetCapturedFocusLocation() const;

public: /** Internal Misc */

	AController* GetController() const;
	bool IsLocallyControlled() const;
	APlayerController* GetPlayerController() const;
	FRotator GetOwnerRotation() const;
	FVector GetOwnerLocation() const;
	FRotator GetCameraRotation() const;
	FVector GetCameraLocation() const;
	FVector GetCameraUpVector() const;
	FVector GetCameraForwardVector() const;
	FVector GetCameraRightVector() const;
};
