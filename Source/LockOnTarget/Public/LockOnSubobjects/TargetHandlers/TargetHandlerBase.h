// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/LockOnSubobjectBase.h"
#include "TargetHandlerBase.generated.h"

struct FTargetInfo;

/**
 * LockOnTargetComponent' subobject abstract class which handle Target.
 * Responsible for finding, switching and maintaining Target.
 * 
 * Should override FindTarget(), SwitchTarget(), CanContinueTargeting().
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Abstract, DefaultToInstanced, EditInlineNew)
class LOCKONTARGET_API UTargetHandlerBase : public ULockOnSubobjectBase
{
	GENERATED_BODY()

public:
	UTargetHandlerBase();
	friend ULockOnTargetComponent;

	/**
	 * Find new Target for Capturing.
	 * Should return valid HelperComponent with Socket.
	 *
	 * @return - New Target information(HelperComponent and Socket).
	 */
	UFUNCTION(BlueprintNativeEvent)
	FTargetInfo FindTarget();
	
	/**
	 * Perform switch Target.
	 * For switching to new Target use new HelperComponent and Socket.
	 * For switching Socket in current Target, set current HelperComponent and new Socket.
	 * 
	 * (You can get HelperComponent via GetLockOn()->GetHelperComponent()).
	 * 
	 * @param TargetInfo - out	New Target info(New Target with socket or new socket for current Target).
	 * @param PlayerInput - Player input in trigonometric deg(0.f, 160.f).
	 * @return - is switching successful(Target found or new socket) otherwise should be false.
	 */
	UFUNCTION(BlueprintNativeEvent)
	bool SwitchTarget(FTargetInfo& TargetInfo, float PlayerInput);

	/** Is Target can be locked until next update. */
	UFUNCTION(BlueprintNativeEvent)
	bool CanContinueTargeting();

protected:
	/** Called when Lock On Component successfully locked Target. */
	UFUNCTION(BlueprintImplementableEvent, meta = (BlueprintProtected))
	void OnTargetLocked();

	/** Called when Lock On Component successfully unlocked Target. */
	UFUNCTION(BlueprintImplementableEvent, meta = (BlueprintProtected))
	void OnTargetUnlocked();

/*******************************************************************************************/
/*******************************  Native   *************************************************/
/*******************************************************************************************/
private:
	/** Only should be called from LockOnComponent. */
	virtual void OnTargetLockedNative();

	/** Only should be called from LockOnComponent. */
	virtual void OnTargetUnlockedNative();
};
