// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnTargetModuleBase.h"
#include "LockOnTargetTypes.h"
#include "TargetHandlerBase.generated.h"

/**
 * LockOnTargetComponent's special abstract module which is used to handle the Target.
 * Responsible for finding and maintaining the Target.
 * 
 * FindTarget() must be overridden.
 * CheckTargetState() and HandleTargetException() are optional.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Abstract, DefaultToInstanced, EditInlineNew, HideDropdown)
class LOCKONTARGET_API UTargetHandlerBase : public ULockOnTargetModuleProxy
{
	GENERATED_BODY()

public:

	UTargetHandlerBase();

public: /** Target Handler Interface */

	/**
	 * Finds and returns a Target to be captured by LockOnTargetComponent.
	 * 
	 * @param PlayerInput - Input from the player. May be empty.
	 * @return - Target to be captured or NULL_TARGET.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LockOnTarget|Target Handler Base")
	FTargetInfo FindTarget(FVector2D PlayerInput = FVector2D::ZeroVector);

	/**
	 * (Optional) Checks the Target state between updates.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Target Handler Base")
	void CheckTargetState(const FTargetInfo& Target, float DeltaTime);

	/**
	 * (Optional) Processes an exception from the Target.
	 *
	 * @Note: Target has already been cleared.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Target Handler Base", meta = (ForceAsFunction))
	void HandleTargetException(const FTargetInfo& Target, ETargetExceptionType Exception);

	/** Whether the target meets all the requirements for being captured. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget|Target Handler Base")
	bool IsTargetValid(const UTargetComponent* Target) const;

private: /** Internal */

	virtual FTargetInfo FindTarget_Implementation(FVector2D PlayerInput);
	virtual void CheckTargetState_Implementation(const FTargetInfo& Target, float DeltaTime);
	virtual void HandleTargetException_Implementation(const FTargetInfo& Target, ETargetExceptionType Exception);

public: /** Deprecated */

	/** (Optional) Target is removed from the level. */
	UFUNCTION(BlueprintImplementableEvent, Category = "LockOnTarget|Target Handler Base", meta = (DeprecatedFunction, DeprecationMessage="HandleTargetException() should be used."))
	void HandleTargetEndPlay(UTargetComponent* Target);

	/** (Optional) Target has removed the previously captured Socket. */
	UFUNCTION(BlueprintImplementableEvent, Category = "LockOnTarget|Target Handler Base", meta = (DeprecatedFunction, DeprecationMessage="HandleTargetException() should be used."))
	void HandleSocketRemoval(FName RemovedSocket);
};
