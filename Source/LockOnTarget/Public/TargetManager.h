// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LockOnTargetTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TargetManager.generated.h"

class UTargetComponent;
class UWorld;

/** 
 * A simple manager that keeps track of registered Targets.
 */
UCLASS()
class LOCKONTARGET_API UTargetManager final : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	UTargetManager();
	static UTargetManager& Get(UWorld& InWorld);

public: 

	//Target registration
	bool RegisterTarget(UTargetComponent* Target);
	bool UnregisterTarget(UTargetComponent* Target);
	bool IsTargetRegistered(UTargetComponent * Target) const { return Targets.Contains(Target); }

	//Get all registered Targets
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget Manager")
	const TSet<UTargetComponent*>& GetAllTargets() const { return Targets; }

	//Get the number of registered Targets
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget Manager")
	int32 GetTargetsNum() const { return Targets.Num(); }

protected: /** Overrides */
	
	//UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type Type) const override;

private: /** Internal */

	//All registered Targets.
	TSet<UTargetComponent*> Targets;
};
