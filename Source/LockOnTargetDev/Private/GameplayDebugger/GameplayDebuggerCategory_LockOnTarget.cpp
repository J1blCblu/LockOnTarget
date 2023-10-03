// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebugger/GameplayDebuggerCategory_LockOnTarget.h"

#include "TargetComponent.h"
#include "LockOnTargetComponent.h"
#include "TargetHandlers/WeightedTargetHandler.h"
#include "LockOnTargetExtensions/LockOnTargetExtensionBase.h"

#include "InputCoreTypes.h"
#include "GameplayDebuggerTypes.h"
#include "Serialization/Archive.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"

#define BOOL_TO_TCHAR(bValue) ((bValue) ? TEXT("true") : TEXT("false"))
#define BOOL_TO_TCHAR_COLORED(bValue, TrueColor, FalseColor) ((bValue) ? TEXT("{" #TrueColor "}") TEXT("true{white}") : TEXT("{" #FalseColor "}") TEXT("false{white}"))

//The guidance line rotation stepping in degrees.
constexpr float GuidanceLineRotationStep = 5.f;

namespace
{
	FString GetClassNameSafe(const UObject* Obj)
	{
		return Obj ? Obj->GetClass()->GetName() : FString("None");
	}

	FVector2D DecomposeAngle(float TrigAngle, bool bInvertYAxis = false)
	{
		const float Radians = FMath::DegreesToRadians(TrigAngle);
		const float X = FMath::Cos(Radians);
		const float Y = FMath::Sin(Radians);
		return { X, bInvertYAxis ? -Y : Y };
	}
}

FGameplayDebuggerCategory_LockOnTarget::FGameplayDebuggerCategory_LockOnTarget()
	: LockOn(nullptr)
	, SimulatedPlayerInput(0.f)
	, bSimulateTargetHandler(false)
{
	//SetDataPackReplication(&RepData);
	bShowOnlyWithDebugActor = false;
	CollectDataInterval = 0.1f;
	bShowCategoryName = true;

	//0
	const FGameplayDebuggerInputHandlerConfig ChangeDebugActorConfig(TEXT("Debug Target"), EKeys::Enter.GetFName());
	BindKeyPress(ChangeDebugActorConfig, this, &FGameplayDebuggerCategory_LockOnTarget::OnKeyPressedChangeDebugActor);
	//1
	const FGameplayDebuggerInputHandlerConfig TargetHandlerSimulationConfig(TEXT("Simulate Handler"), EKeys::K.GetFName());
	BindKeyPress(TargetHandlerSimulationConfig, this, &FGameplayDebuggerCategory_LockOnTarget::OnKeyPressedSimulateTargetHandler);
	//2
	const FGameplayDebuggerInputHandlerConfig SwitchTargetConfig(TEXT("Switch Target"), EKeys::L.GetFName());
	BindKeyPress(SwitchTargetConfig, this, &FGameplayDebuggerCategory_LockOnTarget::OnKeyPressedSwitchTarget);
	//3
	const FGameplayDebuggerInputHandlerConfig RotatePlayerInputUp(TEXT("Rotate Guidance Line Up"), EKeys::MouseScrollUp.GetFName(), FGameplayDebuggerInputModifier::Shift);
	BindKeyPress(RotatePlayerInputUp, this, &FGameplayDebuggerCategory_LockOnTarget::OnRotateGuidanceLineUp);
	//4
	const FGameplayDebuggerInputHandlerConfig RotatePlayerInputDown(TEXT("Rotate Guidance Line Down"), EKeys::MouseScrollDown.GetFName(), FGameplayDebuggerInputModifier::Shift);
	BindKeyPress(RotatePlayerInputDown, this, &FGameplayDebuggerCategory_LockOnTarget::OnRotateGuidanceLineDown);
}

void FGameplayDebuggerCategory_LockOnTarget::DrawData(APlayerController* Controller, FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (Controller && Controller->GetPawn())
	{
		LockOn = Controller->GetPawn()->FindComponentByClass<ULockOnTargetComponent>();

		if (LockOn.IsValid() && LockOn->GetWorld())
		{
			DisplayDebugInfo(CanvasContext);
			DisplayCurrentTarget(CanvasContext);
			SimulateTargetHandler(CanvasContext);
		}
	}
}

void FGameplayDebuggerCategory_LockOnTarget::DisplayDebugInfo(FGameplayDebuggerCanvasContext& CanvasContext) const
{
	//LockOnTarget
	CanvasContext.Printf(TEXT("Can capture Target: %s"), BOOL_TO_TCHAR_COLORED(LockOn->CanCaptureTarget(), green, red));
	CanvasContext.Printf(TEXT("TargetHandler: {yellow}%s"), *GetClassNameSafe(LockOn->GetTargetHandler()));
	CanvasContext.Printf(TEXT("Current Target: {yellow}%s"), *GetNameSafe(LockOn->GetTargetActor()));
	CanvasContext.Printf(TEXT("Captured Socket: %s"), *CollectTargetSocketsInfo(LockOn.Get()));
	CanvasContext.Printf(TEXT("Duration: {yellow}%.1f{white}s"), LockOn->GetTargetingDuration());

	//Other Invaders
	if (LockOn->IsTargetLocked() && LockOn->GetTargetComponent()->GetInvaders().Num() > 1)
	{
		CanvasContext.Print(CollectInvadersInfo());
	}
	
	//Extensions
	CanvasContext.MoveToNewLine();
	CanvasContext.Print(TEXT("{green}[Extensions]"));
	CanvasContext.Print(CollectExtensionsInfo());

	//Input
	CanvasContext.MoveToNewLine();
	CanvasContext.Print(TEXT("{green}[Player Input]"));
	CanvasContext.Printf(TEXT("Input delay is active: %s {yellow}%.2f{white}s"), BOOL_TO_TCHAR_COLORED(LockOn->IsInputDelayActive(), red, green), LockOn->InputProcessingDelay);
	CanvasContext.Printf(TEXT("Input is frozen: %s, Unfreeze Threshold: {yellow}%.2f"), BOOL_TO_TCHAR_COLORED(LockOn->bInputFrozen, red, green), LockOn->UnfreezeThreshold);
	CanvasContext.Printf(TEXT("Input Buffer / Buffer Threshold: {yellow}%.2f / {orange}%.2f"), LockOn->InputBuffer.Size(), LockOn->InputBufferThreshold);

	//Controls
	CanvasContext.MoveToNewLine();
	CanvasContext.Print(TEXT("{green}[Controls]"));
	CanvasContext.Printf(TEXT("[{yellow}%s{white}]: %s"), *GetInputHandlerDescription(0), LockOn->IsTargetLocked() ? TEXT("Set the current Target as the debug actor.") : TEXT("Clear the debug actor."));

	if (Cast<UWeightedTargetHandler>(LockOn->TargetHandlerImplementation))
	{
		CanvasContext.Printf(TEXT("[{yellow}%s{white}]: %s WeightedTargetHandler."), *GetInputHandlerDescription(1), bSimulateTargetHandler ? TEXT("Stop simulating") : TEXT("Simulate"));

		if (bSimulateTargetHandler && LockOn->IsTargetLocked())
		{
			CanvasContext.Printf(TEXT("[{yellow}%s{white}]: Switch the Target in the guidance line direction."), *GetInputHandlerDescription(2));
			CanvasContext.Printf(TEXT("[{yellow}%s{white} and {yellow}%s{white}]: Rotate the guidance line."), *GetInputHandlerDescription(3), *GetInputHandlerDescription(4));
		}
	}
}

FString FGameplayDebuggerCategory_LockOnTarget::CollectExtensionsInfo() const
{
	FString Info;

	for (int32 i = 0; i < LockOn->Extensions.Num(); ++i)
	{
		if (const ULockOnTargetExtensionBase* const Extension = LockOn->Extensions[i])
		{
			Info += FString::Printf(TEXT("{white}%i: {yellow}%s"), i, *GetClassNameSafe(Extension));

			if (i + 1 != LockOn->Extensions.Num())
			{
				Info += TEXT("\n");
			}
		}
	}

	return Info;
}

FString FGameplayDebuggerCategory_LockOnTarget::CollectInvadersInfo() const
{
	FString Info;

	if (LockOn->IsTargetLocked())
	{
		for (const ULockOnTargetComponent* const Invader : LockOn->GetTargetComponent()->GetInvaders())
		{
			if (Invader != LockOn.Get())
			{
				Info += FString::Printf(TEXT("\n{yellow}%s{white}, Duration: {yellow}%.1f{white}s, Socket: %s"), *GetNameSafe(Invader->GetOwner()), Invader->GetTargetingDuration(), *CollectTargetSocketsInfo(Invader));
			}
		}
	}

	return Info;
}

FString FGameplayDebuggerCategory_LockOnTarget::CollectTargetSocketsInfo(const ULockOnTargetComponent* InLockOn) const
{
	FString Info;

	if (InLockOn->IsTargetLocked())
	{
		for (const FName Socket : InLockOn->GetTargetComponent()->GetSockets())
		{
			if (Socket == InLockOn->GetCapturedSocket())
			{
				Info += FString::Printf(TEXT("{yellow}%s{white}, "), *Socket.ToString());
			}
			else
			{
				Info += FString::Printf(TEXT("%s, "), *Socket.ToString());
			}
		}

		Info.RemoveFromEnd(TEXT(", "));
	}
	else
	{
		Info = TEXT("None");
	}

	return Info;
}

void FGameplayDebuggerCategory_LockOnTarget::DisplayCurrentTarget(FGameplayDebuggerCanvasContext& CanvasContext) const
{
	if (const AActor* const CurrentTarget = LockOn->GetTargetActor())
	{
		const FVector OverheadLocation = CurrentTarget->GetActorLocation() + FVector(0, 0, CurrentTarget->GetSimpleCollisionHalfHeight() + 7.f);

		if (CanvasContext.IsLocationVisible(OverheadLocation))
		{
			FGameplayDebuggerCanvasContext OverheadContext(CanvasContext);
			OverheadContext.Font = GEngine->GetSmallFont();
			OverheadContext.FontRenderInfo.bEnableShadow = true;

			const FVector2D ScreenLoc = OverheadContext.ProjectLocation(OverheadLocation);
			const FString ActorDesc(TEXT("Current Target"));

			float SizeX = 0.f, SizeY = 0.f;
			OverheadContext.MeasureString(ActorDesc, SizeX, SizeY);
			OverheadContext.PrintAt(ScreenLoc.X - (SizeX * 0.5f), ScreenLoc.Y - (SizeY * 1.2f), ActorDesc);
		}
	}
}

void FGameplayDebuggerCategory_LockOnTarget::SimulateTargetHandler(FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (bSimulateTargetHandler)
	{
		if (auto* const TargetHandler = Cast<UWeightedTargetHandler>(LockOn->GetTargetHandler()))
		{
			FFindTargetRequestParams Params;
			Params.PlayerInput = DecomposeAngle(SimulatedPlayerInput, true);
			Params.bGenerateDetailedResponse = true;

			const FFindTargetRequestResponse Response = TargetHandler->FindTarget(Params);

			DrawWeights(CanvasContext, Response);
			DrawPlayerInput(TargetHandler, CanvasContext);

		}
		else
		{
			bSimulateTargetHandler = false;
		}
	}
}

void FGameplayDebuggerCategory_LockOnTarget::DrawWeights(FGameplayDebuggerCanvasContext& CanvasContext, const FFindTargetRequestResponse& Response)
{
	if (auto* const DetailedResponse = Cast<UWeightedTargetHandlerDetailedResponse>(Response.Payload))
	{
		int32 TargetsNum = DetailedResponse->TargetsData.Num();

		for (int32 i = 0; i < TargetsNum; ++i)
		{
			const FColor DisplayColor = i == 0 ? FColor::Yellow : FColor::White;
			const FVector2D ScreenLoc = CanvasContext.ProjectLocation(DetailedResponse->TargetsData[i].Location);
			const FString ModifierString = FString::Printf(TEXT("%.2f"), DetailedResponse->TargetsData[i].Weight);

			float ModifierXSize, ModifierYSize;
			CanvasContext.MeasureString(ModifierString, ModifierXSize, ModifierYSize);
			CanvasContext.PrintAt(ScreenLoc.X - ModifierXSize / 2.f, ScreenLoc.Y - ModifierYSize / 2.f, DisplayColor, ModifierString);
		}
	}
}

void FGameplayDebuggerCategory_LockOnTarget::DrawPlayerInput(UWeightedTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (LockOn->IsTargetLocked())
	{
		const FVector2D CapturedLocation = CanvasContext.ProjectLocation(LockOn->GetCapturedSocketLocation());

		auto DrawLine = [=, &CanvasContext](FVector2D Direction, const FColor& Color, float LineThickness = 1.f)
			{
				Direction *= FVector2D(CanvasContext.Canvas->SizeX, CanvasContext.Canvas->SizeY).Size();
				Direction.X += CapturedLocation.X;
				Direction.Y = CapturedLocation.Y - Direction.Y;

				FCanvasLineItem CanvasLine(CapturedLocation, Direction);
				CanvasLine.LineThickness = LineThickness;
				CanvasLine.SetColor(Color);

				CanvasContext.Canvas->DrawItem(CanvasLine);
			};

		DrawLine(DecomposeAngle(SimulatedPlayerInput), FColor::Yellow, 2.f);
		DrawLine(DecomposeAngle(SimulatedPlayerInput - TargetHandler->PlayerInputAngularRange), FColor::Red);
		DrawLine(DecomposeAngle(SimulatedPlayerInput + TargetHandler->PlayerInputAngularRange), FColor::Red);
	}
}

void FGameplayDebuggerCategory_LockOnTarget::OnKeyPressedChangeDebugActor()
{
	if (AGameplayDebuggerCategoryReplicator* const Replicator = GetReplicator())
	{
		Replicator->SetDebugActor(LockOn.IsValid() ? LockOn->GetTargetActor() : nullptr);
	}
}

void FGameplayDebuggerCategory_LockOnTarget::OnKeyPressedSimulateTargetHandler()
{
	if (LockOn.IsValid() && Cast<UWeightedTargetHandler>(LockOn->TargetHandlerImplementation))
	{
		bSimulateTargetHandler ^= true;
	}
}

void FGameplayDebuggerCategory_LockOnTarget::OnKeyPressedSwitchTarget()
{
	if (LockOn.IsValid() && bSimulateTargetHandler)
	{
		LockOn->SwitchTargetManual(DecomposeAngle(SimulatedPlayerInput, true));
	}
}

void FGameplayDebuggerCategory_LockOnTarget::OnRotateGuidanceLineUp()
{
	if (LockOn.IsValid() && LockOn->IsTargetLocked() && bSimulateTargetHandler)
	{
		SimulatedPlayerInput += GuidanceLineRotationStep;
	}
}

void FGameplayDebuggerCategory_LockOnTarget::OnRotateGuidanceLineDown()
{
	if (LockOn.IsValid() && LockOn->IsTargetLocked() && bSimulateTargetHandler)
	{
		SimulatedPlayerInput -= GuidanceLineRotationStep;
	}
}

#undef BOOL_TO_TCHAR
#undef BOOL_TO_TCHAR_COLORED

#endif //WITH_GAMEPLAY_DEBUGGER
