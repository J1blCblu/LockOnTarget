// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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

private: /** Internal */

	//All registered Targets.
	TSet<UTargetComponent*> RegisteredTargets;

public: 

	//Target registration
	bool RegisterTarget(UTargetComponent* Target);
	bool UnregisterTarget(UTargetComponent* Target);
	bool IsTargetRegistered(UTargetComponent * Target) const { return RegisteredTargets.Contains(Target); }

	//Gets all registered Targets
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget Manager")
	const TSet<UTargetComponent*>& GetRegisteredTargets() const { return RegisteredTargets; }

	//Gets the number of registered Targets
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget Manager")
	int32 GetRegisteredTargetsNum() const { return RegisteredTargets.Num(); }

protected: /** Overrides */
	
	//UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type Type) const override;
};
