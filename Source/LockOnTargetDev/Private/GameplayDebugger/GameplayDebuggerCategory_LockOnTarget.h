// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "GameplayDebuggerCategory.h"

struct FFindTargetContext;
struct FTargetContext;
struct FFindTargetRequestResponse;

class APlayerController;
class ULockOnTargetComponent;
class UTargetComponent;
class UWeightedTargetHandler;

/**
 * Ref: GameplayDebuggerCategory_AI and GameplayDebuggerCategory_EQS
 */
class FGameplayDebuggerCategory_LockOnTarget : public FGameplayDebuggerCategory
{
public:

	//@TODO: Maybe we can use a debug actor rather than just an owning pawn. Might be useful for debugging non-player controlled pawns.
	//@TODO: Maybe draw a corrected Target focus location from ControllerRotationExtension.

	FGameplayDebuggerCategory_LockOnTarget();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance()
	{
		return MakeShared<FGameplayDebuggerCategory_LockOnTarget>();
	}

	/** Not implemented as LockOnTarget draws local info. */
	//virtual void CollectData(APlayerController* Controller, AActor* Actor) override;

	virtual void DrawData(APlayerController* Controller, FGameplayDebuggerCanvasContext& CanvasContext) override;

protected:

	//struct FRepData
	//{
	//	void Serialize(FArchive& Ar);
	//} RepData;

private:

	TWeakObjectPtr<ULockOnTargetComponent> LockOn;
	float SimulatedPlayerInput;
	uint8 bSimulateTargetHandler : 1;

private:

	//General Info
	void DisplayDebugInfo(FGameplayDebuggerCanvasContext& CanvasContext) const;
	FString CollectExtensionsInfo() const;
	FString CollectInvadersInfo() const;
	FString CollectTargetSocketsInfo(const ULockOnTargetComponent* InLockOn) const;

	//Current Target text.
	void DisplayCurrentTarget(FGameplayDebuggerCanvasContext& CanvasContext) const;

	//TargetHandler
	void SimulateTargetHandler(FGameplayDebuggerCanvasContext& CanvasContext);
	void DrawWeights(FGameplayDebuggerCanvasContext& CanvasContext, const FFindTargetRequestResponse& Response);
	void DrawPlayerInput(UWeightedTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext);

	//Input
	void OnKeyPressedChangeDebugActor();
	void OnKeyPressedSimulateTargetHandler();
	void OnKeyPressedSwitchTarget();
	void OnRotateGuidanceLineUp();
	void OnRotateGuidanceLineDown();
};

#endif //WITH_GAMEPLAY_DEBUGGER
