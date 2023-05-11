// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "GameplayDebuggerCategory.h"

struct FFindTargetContext;
class APlayerController;
class ULockOnTargetComponent;
class UTargetComponent;
class UThirdPersonTargetHandler;

/**
 * Ref: GameplayDebuggerCategory_AI and GameplayDebuggerCategory_EQS
 */
class FGDC_LockOnTarget : public FGameplayDebuggerCategory
{
public:
	FGDC_LockOnTarget();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance()
	{
		return MakeShared<FGDC_LockOnTarget>();
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
	TArray<TPair<FVector, float>, TInlineAllocator<50>> Modifiers;
	float SimulatedPlayerInput;
	uint8 bSimulateTargetHandler : 1;

private: /**  */

	//General Info
	void DisplayDebugInfo(FGameplayDebuggerCanvasContext& CanvasContext) const;
	FString CollectModulesInfo() const;
	FString CollectInvadersInfo() const;
	FString CollectTargetSocketsInfo(const ULockOnTargetComponent* InLockOn) const;

	//Current Target text.
	void DisplayCurrentTarget(FGameplayDebuggerCanvasContext& CanvasContext) const;

	//TargetHandler
	void SimulateTargetHandler(FGameplayDebuggerCanvasContext& CanvasContext);
	void OnModifierCalculated(const FFindTargetContext& TargetContext, float Modifier);
	void DrawSimulation(UThirdPersonTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext);
	void DrawModifiers(FGameplayDebuggerCanvasContext& CanvasContext);
	void DrawScreenOffset(UThirdPersonTargetHandler* const TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext) const;
	void DrawPlayerInput(UThirdPersonTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext);

private: /** Input */
	void OnKeyPressedChangeDebugActor();
	void OnKeyPressedSimulateTargetHandler();
	void OnKeyPressedSwitchTarget();
	void OnRotateGuidanceLineUp();
	void OnRotateGuidanceLineDown();

private: /** Misc */
	FVector2D TrigonometricToVector2D(float TrigAngle, bool bInvertYAxis = false) const;
	void GetScreenOffsetBox(UThirdPersonTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext, FVector2D& StartPos, FVector2D& EndPos) const;
};

#endif //WITH_GAMEPLAY_DEBUGGER
