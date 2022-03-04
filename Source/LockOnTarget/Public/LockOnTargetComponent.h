// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Utilities/Structs.h"
#include "LockOnTargetComponent.generated.h"

class UTargetingHelperComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetLocked, AActor*, LockedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetNotFound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetUnlocked, AActor*, UnlockedActor);

/**
 *	Lock on Target Component gives the Owner(Player) the ability to lock onto(capture) Target that the Camera and/or Owner will follow.
 * 
 *	Lock On Target Component is wrapper over next systems:
 *	+ Target storage(Helper Component and Socket).
 *	+ Processing input.
 *	+ Delegating other work to it subobjects(which can be changed via BP/C++):
 *	+	- TargetHandler used for handle Target(find, switch, maintenance).
 *	+	- Tick features, like rotation mode which return final rotation for Control/Owner Rotation.
 * 
 * Main advantages:
 *	- Targeting to any AActor subclass. Also you can add and remove Helper component from any Actor at runtime.
 *	  (Not only one class and its subclasses, any Actor can be targeted, like a stone on the ground or huge Dragon with multiply capture points).
 *  - Target can have multiple sockets, which can be added/removed at runtime.
 *  - Custom rules for Target finding, switching, maintenance.
 *  - Custom owner/control rotation rules.
 *  - Flexible processing input settings.
 *  - Multiple useful methods for Owner.
 * 
 *	@see UTargetingHelperComponent, UTargetHandlerBase, UWithoutInterpolationRotationMode.
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

	/** Implementation of Handling Target(find, switch, maintenance). */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Instanced, Category = "Default Settings")
	class UTargetHandlerBase* TargetHandlerImplementation;

	/** 
	 * Rotation mode for Control Rotation. 
	 * 
	 * Also note that you may need control yourself next fields:
	 *	+ bOrientRotationToMovement in UCharacterMovementComponent.
	 *	+ bUseControllerRotationYaw in APawn.
	 *	(e.g. via OnTargetLocked/OnTargetUnlocked delegates))
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Instanced, Category = "Lock Settings")
	class URotationModeBase* ControlRotationModeConfig;

	/** 
	 * Rotation mode for Owner Rotation.
	 * 
	 * Also note that you may need control yourself next fields:
	 *	+ bOrientRotationToMovement in UCharacterMovementComponent.
	 *	+ bUseControllerRotationYaw in APawn.
	 *	(e.g. via OnTargetLocked/OnTargetUnlocked delegates))
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Instanced, Category = "Lock Settings")
	class URotationModeBase* OwnerRotationModeConfig;

	/** Should component tick while unlocked. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "Lock Settings")
	bool bDisableTickWhileUnlocked;

	/**
	 * Calls Switch method on InputBuffer overflow with player input.
	 * Player input converts to 2D space vector with direction value of player Analog input.
	 * Resets to 0.f when InputBuffer overflows and at certain intervals(can be filled below).
	 * 
	 * i.e. if value = 0.5f, then for replenish player axis input(2D vector length) should have minimum 0.5f average value per sec.
	 * 
	 * Attention! Be careful with BufferResetFrequency low values. It can reset buffer too early.
	 * 
	 * You can override GetInputBufferThreshold() for custom rules for specific analog controller.
	 * 
	 * This field only work with default native player input handling via SwitchTargetYaw/Pitch() methods.
	 * You can write your own Input handler and calling method SwitchTargetManual() with input Direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Switching", meta = (ClampMin = 0.f, UIMin = 0.f))
	float InputBufferThreshold;

	/** 
	 * Frequency with which InputBuffer resets to 0.f. After it for switching Player should replenish buffer from 0.
	 * 
	 * This field only work with default native player input handling via SwitchTargetYaw/Pitch() methods.
	 * You can write your own Input handler and calling method SwitchTargetManual() with input Direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Switching", meta = (ClampMin = 0.1f, UIMin = 0.1))
	float BufferResetFrequency;

	/** 
	 * Clamp player analog 2D space vector length input with this values.
	 * Where X - min value, Y - max value. Usually these values should be opposite.
	 * 
	 * This field only work with default native player input handling via SwitchTargetYaw/Pitch() methods.
	 * You can write your own Input handler and calling method SwitchTargetManual() with input Direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Switching")
	FVector2D ClampInputVector;

	/** 
	 * Frequency with which target can be changed after switching to new Target.
	 * 
	 * This field only work with default native player input handling via SwitchTargetYaw/Pitch() methods.
	 * You can write your own Input handler and calling method SwitchTargetManual() with input Direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Switching", meta = (ClampMin = 0.f, UIMin = 0.f))
	float SwitchDelay;

	/** 
	 * Freeze player input until it reaches zero after successful switching. 
	 * 
	 * This field only work with default native player input handling via SwitchTargetYaw/Pitch() methods.
	 * You can write your own Input handler and calling method SwitchTargetManual() with input Direction.
	 */
	UPROPERTY(EditAnywhere, Category = "Target Switching")
	bool bFreezeInputAfterSwitch;

#if WITH_EDITORONLY_DATA
	
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bShowTargetInfo = false;

	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bShowPlayerInput = false;

	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (HideAlphaChannel, EditCondition = "bShowPlayerInput || bShowTargetInfo"))
	FColor DebugInfoColor = FColor::Black;

#endif

/*******************************************************************************************/
/*******************************  Delegates  ***********************************************/
/*******************************************************************************************/
public:
	/** Called when Target is Locked. */
	UPROPERTY(BlueprintAssignable, Category = "LockOnTarget")
	FOnTargetLocked OnTargetLocked;

	/** Called if Target not found while finding or switching. Maybe useful IsTargetLocked(). */
	UPROPERTY(BlueprintAssignable, Category = "LockOnTarget")
	FOnTargetNotFound OnTargetNotFound;

	/** Called when Target unlocked. */
	UPROPERTY(BlueprintAssignable, Category = "LockOnTarget")
	FOnTargetUnlocked OnTargetUnlocked;

/*******************************************************************************************/
/*******************************  BP Methods  **********************************************/
/*******************************************************************************************/

public:
	/** Main method for Capturing and Clearing Target.*/
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component|Player Input")
	void EnableTargeting();

	/** Feed Yaw input for input buffer. */
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component|Player Input")
	void SwitchTargetYaw(float YawAxis);

	/** Feed Pitch input for input buffer. */
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component|Player Input")
	void SwitchTargetPitch(float PitchAxis);

	/** Additional method for switching Target while targeting. Useful for custom processing input. */
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component|Player Input")
	bool SwitchTargetManual(float TrigonometricInput);

	/** For manual Capturing TargetActor. Automatic one is EnableTargeting(). */
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component")
	void SetLockOnTarget(AActor* NewTarget, const FName& Socket = FName(TEXT("spine_02")));

	/** For manual Clearing current TargetActor. Automatic one is EnableTargeting(). */
	UFUNCTION(BlueprintCallable, Category = "Lock On Target Component")
	void ClearTarget();

	/** @return - Is any Target Locked On. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	bool IsTargetLocked() const { return bIsTargetLocked; }

	/** @return - Current Locked On Target. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	AActor* GetTarget() const;

	/** @return - Current Locked On Target's HelperComponent. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	UTargetingHelperComponent* GetHelperComponent() const { return PrivateTargetInfo.HelperComponent;}

	/** @return - Targeting duration in sec. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	float GetTargetingDuration() const { return TargetingDuration; }

	/** @return - WorldLocation of Locked On Target's socket. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	FVector GetCapturedLocation(bool bWithOffset = false) const;

	/** @return - Captured socket if exists. None otherwise. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target Component")
	FName GetCapturedSocket() const { return PrivateTargetInfo.SocketForCapturing; }

/*******************************************************************************************/
/*******************************  BP Overridable  ******************************************/
/*******************************************************************************************/
protected:

	/** Can be overridden for adapting to Analog Input sensitivity. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget")
	float GetInputBufferThreshold() const;

/*******************************************************************************************/
/*******************************  Native   *************************************************/
/*******************************************************************************************/
private:
	UPROPERTY(Transient)
	FTargetInfo PrivateTargetInfo;

	bool bIsTargetLocked;

	UPROPERTY(Transient)
	float TargetingDuration;
	
	/** Capturing Target. */
	void SetLockOnTargetNative(const FTargetInfo& TargetInfo);
	void SetupHelperComponent();
	void UpdateTargetSocket(const FName& NewSocket);
	/** ~Capturing Target. */

	/** Listener for HelperComponent UnlockInvader(). */
	FDelegateHandle TargetUnlockDelegateHandle;

	/** Finding and Switching. */
	void FindTarget();
	bool SwitchTarget(float PlayerInput);
	/** ~Finding and Switching. */

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

	/** Tick calculations. */
	void TickControlRotationCalc(float DeltaTime, const FVector& TargetLocation);
	void TickOwnerRotationCalc(float DeltaTime, const FVector& TargetLocation);
	/** ~Tick calculations. */
	
public:
	/**  Helpers Methods. */
	AController* GetController() const;
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
	void DebugOnTick() const;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& event) override;

#endif
};
