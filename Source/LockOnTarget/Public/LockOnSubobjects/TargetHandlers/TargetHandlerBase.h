// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/LockOnSubobjectBase.h"
#include "TargetHandlerBase.generated.h"

struct FTargetInfo;

/**
 * LockOnTargetComponent's subobject abstract class which is used to handle the Target.
 * Responsible for finding, switching and maintaining the Target.
 * 
 * FindTarget(), SwitchTarget(), CanContinueTargeting() should be overridden.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Abstract, DefaultToInstanced, EditInlineNew)
class LOCKONTARGET_API UTargetHandlerBase : public ULockOnSubobjectBase
{
	GENERATED_BODY()

public:
	UTargetHandlerBase();
	friend ULockOnTargetComponent;

	/**
	 * Used to find a new Target.
	 * Should return a valid HelperComponent with a Socket.
	 * 
	 * (You can get the TargetingHelperComponent via GetLockOn()->GetHelperComponent()).
	 *
	 * @return - New Target information (TargetingHelperComponent and Socket).
	 */
	UFUNCTION(BlueprintNativeEvent)
	FTargetInfo FindTarget();
	
	/**
	 * Used to switch a Target.
	 * Provide a new HelperComponent and a socket to switch to the new Target . 
	 * Provide the captured HelperComponent and a new Socket to switch the socket on the current Target.
	 * 
	 * (You can get the TargetingHelperComponent via GetLockOn()->GetHelperComponent()).
	 * 
	 * @param TargetInfo - out	New Target info (new TargetingHelperComponent with a socket or a new socket for the current Target).
	 * @param PlayerInput - Player input in the trigonometric deg(0.f, 180.f).
	 * @return - was switch successful (a new Target or a new socket) otherwise should be false.
	 */
	UFUNCTION(BlueprintNativeEvent)
	bool SwitchTarget(FTargetInfo& TargetInfo, float PlayerInput);

	/** Can the Target be locked until the next update. */
	UFUNCTION(BlueprintNativeEvent)
	bool CanContinueTargeting();

protected:
	/** Called when the LockOnTargetComponent successfully locked the Target. */
	UFUNCTION(BlueprintImplementableEvent, meta = (BlueprintProtected))
	void OnTargetLocked();

	/** Called when the LockOnTargetComponent successfully unlocked the Target. */
	UFUNCTION(BlueprintImplementableEvent, meta = (BlueprintProtected))
	void OnTargetUnlocked();

/*******************************************************************************************/
/*******************************  Native   *************************************************/
/*******************************************************************************************/
private:
	/** Only should be called from the LockOnTargetComponent. */
	virtual void OnTargetLockedNative();

	/** Only should be called from the LockOnTargetComponent. */
	virtual void OnTargetUnlockedNative();
};
