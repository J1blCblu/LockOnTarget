// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/LockOnTargetModuleBase.h"
#include "TargetInfo.h"
#include "TargetHandlerBase.generated.h"

/**
 * LockOnTargetComponent's special abstract module which is used to handle the Target.
 * Responsible for finding and maintaining the Target.
 * 
 * FindTarget() and CanContinueTargeting() must be overridden.
 * HandleTargetEndPlay() and HandleSocketRemoval() are optional.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Abstract, DefaultToInstanced, EditInlineNew, HideDropdown)
class LOCKONTARGET_API UTargetHandlerBase : public ULockOnTargetModuleProxy
{
	GENERATED_BODY()

public:
	UTargetHandlerBase() = default;

public: /** Target Handler Interface */

	/**
	 * Used by the LockOnTargetComponent to encapsulate Target finding.
	 * To actually capture a Target, should return a valid HelperComponent with a socket.
	 * The Target will be considered not found if an invalid HelperComponent is returned,
	 * or the already captured HelperComponent with the same socket is returned.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LockOnTarget|Target Handler Base")
	FTargetInfo FindTarget(FVector2D PlayerInput = FVector2D(0.f, 0.f));

	/**
	 * Can the Target be locked until the next update.
	 * You can perform the Target pointer validity and the HelperComponent's CanBeTargeted() result here and react appropriately, e.g. Auto find a new Target.
	 * Or leave these checks to the LockOnTargetComponent which will just clear the Target.
	 * Won't be called and the Target validity will be automatically processed on the non-locally controlled Server and Simulated proxies.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Target Handler Base")
	bool CanContinueTargeting();

	/** Chance to handle Target EndPlay before LockOnTargetComponent. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Target Handler Basse")
	void HandleTargetEndPlay(UTargetingHelperComponent* HelperComponent);

	/** Chance to handle Target SocketRemoval before LockOnTargetComponent. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget| Target Handler Base")
	void HandleSocketRemoval(FName RemovedSocket);

private: /** Internal */
	virtual FTargetInfo FindTarget_Implementation(FVector2D PlayerInput);
	virtual bool CanContinueTargeting_Implementation();
	virtual void HandleTargetEndPlay_Implementation(UTargetingHelperComponent* HelperComponent);
	virtual void HandleSocketRemoval_Implementation(FName RemovedSocket);
};
