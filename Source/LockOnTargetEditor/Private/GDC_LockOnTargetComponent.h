// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"
#include "UObject/WeakObjectPtr.h"

struct FFindTargetContext;
class APlayerController;
class ULockOnTargetComponent;
class UTargetingHelperComponent;
class UDefaultTargetHandler;
class URotationModeBase;

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
	uint8 bSimulateRotationModes : 1;

private: /**  */

	//General Info
	void DisplayDebugInfo(FGameplayDebuggerCanvasContext& CanvasContext) const;
	FString CollectModulesInfo() const;
	FString CollectInvadersInfo() const;
	FString CollectTargetSocketsInfo() const;

	//Current Target text.
	void DisplayCurrentTarget(FGameplayDebuggerCanvasContext& CanvasContext) const;
	
	//RotationMode
	void SimulateRotationModes(FGameplayDebuggerCanvasContext& CanvasContext);
	void DrawAxesTile(FGameplayDebuggerCanvasContext& CanvasContext, URotationModeBase* RotationMode, FVector2D TileCenter, const FRotator& CurrentRotation, const FVector& LocationFrom, const FString& TileTitle);

	//TargetHandler
	void SimulateTargetHandler(FGameplayDebuggerCanvasContext& CanvasContext);
	void OnModifierCalculated(const FFindTargetContext& TargetContext, float Modifier);
	void DrawSimulation(UDefaultTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext);
	void DrawModifiers(FGameplayDebuggerCanvasContext& CanvasContext);
	void DrawScreenOffset(UDefaultTargetHandler* const TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext) const;
	void DrawPlayerInput(UDefaultTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext);

private: /** Input */
	void OnKeyPressedChangeDebugActor();
	void OnKeyPressedSimulateTargetHandler();
	void OnKeyPressedSwitchTarget();
	void OnKeyPressedAdd();
	void OnKeyPressedSubtract();
	void OnKeyPressedSimulateRotationModes();

private: /** Misc */
	FVector2D TrigonometricToVector2D(float TrigAngle, bool bInvertYAxis = false) const;
	FVector2D GetScreenOffset(UDefaultTargetHandler* const TargetHandler) const;
	void GetScreenOffsetBox(UDefaultTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext, FVector2D& StartPos, FVector2D& EndPos) const;
	FString GetScreenOffsetName() const;
};

#endif //WITH_GAMEPLAY_DEBUGGER
