// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Utilities/Structs.h"
#include "LockOnTargetComponent.generated.h"

class UTargetingHelperComponent;
class AController;
class APlayerController;
class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetLocked, AActor*, LockedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetNotFound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetUnlocked, AActor*, UnlockedActor);

/**
 *	LockOnTargetComponent gives the Owner(Player) the ability to lock onto(capture) the Target that the Camera and/or Owner will follow.
 * 
 *	LockOnTargetComponent is a wrapper over the next systems:
 *	1. Target storage (TargetingHelperComponent and Socket). Synchronized with the server.
 *	2. Processing the input. Processed locally.
 *	3. TargetHandler subobject - used for Target handling(find, switch, maintenance). Processed locally.
 *	4. RotationMode subobject - used to get the rotation for the Control/Owner rotation. Mostly processed locally.
 * 
 *	Main advantages:
 *	- Capture any Actor with a TargetingHelperComponent which can be removed and added at runtime.
 *  - Target can have multiple sockets, which can be added/removed at runtime.
 *  - Network synchronization.
 *  - Switch Targets in any direction (in screen space).
 *  - Custom rules for finding, switching, maintenance the Target.
 *  - Custom owner/control rotation rules.
 *  - Flexible input processing settings.
 *  - Several useful methods for the Owner.
 * 
 *	Network Design Philosophy:
 *	Lock On Target is just a system which finds a Target, stores and synchronizes it over the network. 
 *	Finding the Target is not a quick operation to process it on the server for each player. 
 *	Due to this and network relevancy opportunities (not relevant Target doesn't exist in the world) finding the Target processed locally on the owning client.
 * 
 *	@see UTargetingHelperComponent, UTargetHandlerBase, URotationModeBase.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), HideCategories = (Components, Activation, Cooking, ComponentTick, Sockets, Collision, ComponentReplication), meta = (BlueprintSpawnableComponent))
class LOCKONTARGET_API ULockOnTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULockOnTargetComponent();
	
protected:
	//UActorComponent
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	//~UActorComponent

public:
	/** Can this Component capture Targets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings")
	bool bCanCaptureTarget;

protected:
	/** Implementation of handling Target (find, switch, maintenance). */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Instanced, Category = "Default Settings")
	class UTargetHandlerBase* TargetHandlerImplementation;

public:
	/** 
	 * Rotation mode for the control Rotation. 
	 * 
	 * Also note that you may need to control the following fields yourself:
	 *	+ bOrientRotationToMovement in UCharacterMovementComponent.
	 *	+ bUseControllerRotationYaw in APawn.
	 *	(e.g. via OnTargetLocked/OnTargetUnlocked delegates)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Lock Settings")
	class URotationModeBase* ControlRotationModeConfig;

	/** 
	 * Rotation mode for Owner Rotation.
	 * 
	 * Also note that you may need to control the following fields yourself:
	 *	+ bOrientRotationToMovement in UCharacterMovementComponent.
	 *	+ bUseControllerRotationYaw in APawn.
	 *	(e.g. via OnTargetLocked/OnTargetUnlocked delegates)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Lock Settings")
	class URotationModeBase* OwnerRotationModeConfig;

	/** Should the component tick while the no Target is locked. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "Lock Settings")
	bool bDisableTickWhileUnlocked;

	/**
	 * When the InputBuffer overflows the threshold by the player's input, the switch method will be called.
	 * Player's input will be converted to a 2D direction vector.
	 * Buffer resets to 0.f when the InputBuffer overflows and at certain intervals (can be filled below).
	 *
	 * i.e. if the threshold = 0.5f, then for it to overflow, the player input (2D vector length) should have a minimum average value of 0.5f per sec.
	 * 
	 * Attention! Be careful with BufferResetFrequency low values. It can reset the buffer too early.
	 * 
	 * You can override GetInputBufferThreshold() for a particular analog controller.
	 * 
	 * This field only works with the default native player's input handling via the SwitchTargetYaw/Pitch() methods.
	 * You can write your own input handler and call the SwitchTargetManual() method with an input direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Switching", meta = (ClampMin = 0.f, UIMin = 0.f))
	float InputBufferThreshold;

	/** 
	 * InputBuffer resets to 0.f at this frequency.
	 * 
	 * This field only works with the default native player's input handling via the SwitchTargetYaw/Pitch() methods.
	 * You can write your own input handler and call the SwitchTargetManual() method with an input direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Switching", meta = (ClampMin = 0.1f, UIMin = 0.1))
	float BufferResetFrequency;

	/** 
	 * Player input axes are clamped by these values.
	 * X - min value, Y - max value. Usually these values should be opposite.
	 * 
	 * This field only works with the default native player's input handling via the SwitchTargetYaw/Pitch() methods.
	 * You can write your own input handler and call the SwitchTargetManual() method with an input direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Switching")
	FVector2D ClampInputVector;

	/** 
	 * Switching to a new Target isn't allowed after a successful switch for a while.
	 * 
	 * This field only works with the default native player's input handling via the SwitchTargetYaw/Pitch() methods.
	 * You can write your own input handler and call the SwitchTargetManual() method with a input direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Switching", meta = (ClampMin = 0.f, UIMin = 0.f))
	float SwitchDelay;

	/** 
	 * Freeze the filling of the InputBuffer until the player input reaches the UnfreezeThreshold after a successful switch. 
	 * 
	 * This field only works with the default native player's input handling via the SwitchTargetYaw/Pitch() methods.
	 * You can write your own input handler and call the SwitchTargetManual() method with an input direction.
	 */
	UPROPERTY(EditAnywhere, Category = "Target Switching")
	bool bFreezeInputAfterSwitch;

	/** 
	 * Unfreeze the filling of the InputBuffer if the player input is less than a threshold.
	 * 
	 * This field only works with the default native player's input handling via the SwitchTargetYaw/Pitch() methods.
	 * You can write your own input handler and call the SwitchTargetManual() method with an input direction.
	 */
	UPROPERTY(EditAnywhere, Category = "Target Switching", meta = (ClampMin = 0.f, UIMin = 0.f, EditCondition = "bFreezeInputAfterSwitch", EditConditionHides))
	float UnfreezeThreshold;

#if WITH_EDITORONLY_DATA
	
	/** Display some information about the state of the Targeting. */
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bShowTargetInfo = false;

	/** Display the player's trigonometric input. */
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bShowPlayerInput = false;

	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (HideAlphaChannel, EditCondition = "bShowPlayerInput || bShowTargetInfo", EditConditionHides))
	FColor DebugInfoColor = FColor::Black;

#endif

/*******************************************************************************************/
/*******************************  Delegates  ***********************************************/
/*******************************************************************************************/
public:
	/** Called when the Target is locked. */
	UPROPERTY(BlueprintAssignable, Category = "LockOnTarget")
	FOnTargetLocked OnTargetLocked;

	/** Called if the Target isn't found after finding or switching. */
	UPROPERTY(BlueprintAssignable, Category = "LockOnTarget")
	FOnTargetNotFound OnTargetNotFound;

	/** Called when the Target is unlocked. */
	UPROPERTY(BlueprintAssignable, Category = "LockOnTarget")
	FOnTargetUnlocked OnTargetUnlocked;

/*******************************************************************************************/
/*******************************  BP Methods  **********************************************/
/*******************************************************************************************/
public:
	/** Main method of capturing and clearing a Target. */
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component|Player Input")
	void EnableTargeting();

	/** Feed Yaw input to the InputBuffer. */
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component|Player Input")
	void SwitchTargetYaw(float YawAxis);

	/** Feed Pitch input to the InputBuffer. */
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component|Player Input")
	void SwitchTargetPitch(float PitchAxis);

	/** Manual Target switching. Useful for custom input processing. */
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component|Manual Handling")
	bool SwitchTargetManual(float TrigonometricInput);

	/** Manual Target capturing. Useful for custom input processing. */
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component|Manual Handling", meta = (AutoCreateRefTerm = "Socket"))
	void SetLockOnTargetManual(AActor* NewTarget, const FName& Socket = NAME_None);

	/** Manual Target clearing. Useful for custom input processing. */
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component|Manual Handling")
	void ClearTargetManual(bool bAutoFindNewTarget = false);

	/** Is any Target locked. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	bool IsTargetLocked() const { return bIsTargetLocked; }

	/** Currently locked Target. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	AActor* GetTarget() const;

	/** Current locked Target's TargetingHelperComponent. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	UTargetingHelperComponent* GetHelperComponent() const { return PrivateTargetInfo.HelperComponent; }

	/** Targeting duration in sec. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	float GetTargetingDuration() const { return TargetingDuration; }

	/** WorldLocation of the locked Target's socket. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	FVector GetCapturedLocation(bool bWithOffset = false) const;

	/** Captured socket, if exists. NAME_None otherwise. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	FName GetCapturedSocket() const { return PrivateTargetInfo.SocketForCapturing; }

/*******************************************************************************************/
/*******************************  BP Overridable  ******************************************/
/*******************************************************************************************/
protected:

	/** Override the InputBufferThreshold for a particular analog controller. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget")
	float GetInputBufferThreshold() const;

/*******************************************************************************************/
/*******************************  Native   *************************************************/
/*******************************************************************************************/
private:
	//Information about the captured Target. Contains TargetingHelperComponent and Socket.
	FTargetInfo PrivateTargetInfo;

	//Has this component captured the Target.
	bool bIsTargetLocked;

	//The time during which the Target is captured. 
	UPROPERTY(Transient, Replicated)
	float TargetingDuration;

private:
	/** Network. */

	//Replicated TargetInfo which is locally compared with the PrivateTargetInfo.
	UPROPERTY(Transient, ReplicatedUsing = OnTargetInfoUpdated)
	FTargetInfo Rep_TargetInfo;

	//Main method which controls the Target state.
	UFUNCTION()
	void OnTargetInfoUpdated();

	//Sends the desired Target to the server.
	UFUNCTION(Server, Reliable)
	void Server_UpdateTargetInfo(const FTargetInfo& TargetInfo);

	/** ~Network. */

private:
	/** Capturing the Target. */
	
	//Updates Rep_TargetIndo locally and sends to the server.
	void UpdateTargetInfo(const FTargetInfo& TargetInfo);

	//Called to capture the Target.
	virtual void SetLockOnTargetNative();
	
	//Called to release the Target.
	virtual void ClearTargetNative();

	//Called to change the Target's socket (if the Target has > 1 sockets).
	virtual void UpdateTargetSocket();
	
	/** ~Capturing the Target. */

private:
	/** Finding and Switching. */
	
	//Called when the Owner performs to find the Target.
	virtual void FindTarget();

	//Called when the Owner performs to switch the Target.
	virtual bool SwitchTarget(float PlayerInput);

	//Called during tick to check the Target validity.
	bool CanContinueTargeting() const;
	
	/** ~Finding and Switching. */

private:
	/** Processing Input. */
	FTimerHandle SwitchDelayHandler;
	FTimerHandle BufferResetHandler;
	FVector2D InputBuffer;
	FVector2D InputVector;
	mutable bool bInputFrozen;

	void ProcessAnalogInput();
	bool CanInputBeProcessed(float PlayerInput) const;
	void ClearInputBuffer();
	/** ~Processing Input. */

private:
	/** Tick calculations. */
	void TickControlRotationCalc(float DeltaTime, const FVector& TargetLocation);
	void TickOwnerRotationCalc(float DeltaTime, const FVector& TargetLocation);
	/** ~Tick calculations. */
	
public:
	/**  Helpers Methods. */
	AController* GetController() const;
	bool IsOwnerLocallyControlled() const;
	APlayerController* GetPlayerController() const;
	FRotator GetRotationToTarget() const;
	FRotator GetOwnerRotation() const;
	FVector GetOwnerLocation() const;
	FRotator GetCameraRotation() const;
	FVector GetCameraLocation() const;
	FVector GetCameraUpVector() const;
	FVector GetCameraForwardVector() const;
	FVector GetCameraRightVector() const;
	/** ~Helpers Methods. */

/*******************************************************************************************/
/******************************* Debug and Editor Only  ************************************/
/*******************************************************************************************/
#if WITH_EDITORONLY_DATA
private:
	//Display and visualize debug info.
	virtual void DebugOnTick() const;
#endif
};
