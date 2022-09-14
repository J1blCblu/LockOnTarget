// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/LockOnTargetModuleBase.h"
#include "Templates/SubclassOf.h"
#include "RotationModules.generated.h"

class ULockOnTargetComponent;
class URotationModeBase;

/**
 * Base rotation module.
 */
UCLASS(Abstract, Blueprintable, HideDropdown)
class LOCKONTARGET_API URotationModule : public ULockOnTargetModuleBase
{
	GENERATED_BODY()

public:
	URotationModule();

public:
	/** Rotation Mode subobject to get the rotation. Can be changed at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Module")
	TObjectPtr<URotationModeBase> RotationMode;

public:
	/** Create, assign and return (for setup purposes) a new RotationMode from class. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTargetComponent", meta = (DeterminesOutputType = RotationModeClass))
	URotationModeBase* SetRotationMode(TSubclassOf<URotationModeBase> RotationModeClass);
	
	template<typename Class>
	typename TEnableIf<TPointerIsConvertibleFromTo<Class, URotationModeBase>::Value, Class>::Type*
		SetRotationMode()
	{
		return static_cast<Class*>(SetRotationMode(Class::StaticClass()));
	}

	/** Specifies the view location from which the direction to the Target will be calculated. */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Rotation Module")
	FVector GetViewLocation() const;
	
protected:
	/** Native implementation of the above. */
	virtual FVector GetViewLocation_Implementation() const;
};

/** Modify the control rotation of the LockOnTargetComponent's instigator controller. */
UCLASS(Blueprintable)
class LOCKONTARGET_API UControlRotationModule : public URotationModule
{
	GENERATED_BODY()

public:
	UControlRotationModule();

protected:
	virtual void UpdateOverridable(FVector2D PlayerInput, float DeltaTime) override;
	virtual FVector GetViewLocation_Implementation() const override;
};

/** Modify the rotation of the LockOnTargetComponent's owner. */
UCLASS(Blueprintable)
class LOCKONTARGET_API UOwnerRotationModule : public URotationModule
{
	GENERATED_BODY()

public:
	UOwnerRotationModule();

protected:
	//@TODO: Maybe add a distance check for optimization.
	virtual void UpdateOverridable(FVector2D PlayerInput, float DeltaTiem) override;
};
