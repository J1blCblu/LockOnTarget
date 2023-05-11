// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebugger/GDC_LockOnTargetComponent.h"

#include "TargetComponent.h"
#include "LockOnTargetComponent.h"
#include "TargetHandlers/ThirdPersonTargetHandler.h"
#include "LockOnTargetModuleBase.h"

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

static FString GetClassNameSafe(const UObject* Obj)
{
	return Obj ? Obj->GetClass()->GetName() : FString("None");
}

//The guidance line rotation stepping in degrees.
constexpr float GuidanceLineRotationStep = 5;

FGDC_LockOnTarget::FGDC_LockOnTarget()
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
	BindKeyPress(ChangeDebugActorConfig, this, &FGDC_LockOnTarget::OnKeyPressedChangeDebugActor);
	//1
	const FGameplayDebuggerInputHandlerConfig TargetHandlerSimulationConfig(TEXT("Simulate Handler"), EKeys::K.GetFName());
	BindKeyPress(TargetHandlerSimulationConfig, this, &FGDC_LockOnTarget::OnKeyPressedSimulateTargetHandler);
	//2
	const FGameplayDebuggerInputHandlerConfig SwitchTargetConfig(TEXT("Switch Target"), EKeys::L.GetFName());
	BindKeyPress(SwitchTargetConfig, this, &FGDC_LockOnTarget::OnKeyPressedSwitchTarget);
	//3
	const FGameplayDebuggerInputHandlerConfig RotatePlayerInputUp(TEXT("Rotate Guidance Line Up"), EKeys::MouseScrollUp.GetFName(), FGameplayDebuggerInputModifier::Shift);
	BindKeyPress(RotatePlayerInputUp, this, &FGDC_LockOnTarget::OnRotateGuidanceLineUp);
	//4
	const FGameplayDebuggerInputHandlerConfig RotatePlayerInputDown(TEXT("Rotate Guidance Line Down"), EKeys::MouseScrollDown.GetFName(), FGameplayDebuggerInputModifier::Shift);
	BindKeyPress(RotatePlayerInputDown, this, &FGDC_LockOnTarget::OnRotateGuidanceLineDown);
}

void FGDC_LockOnTarget::DrawData(APlayerController* Controller, FGameplayDebuggerCanvasContext& CanvasContext)
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

void FGDC_LockOnTarget::DisplayDebugInfo(FGameplayDebuggerCanvasContext& CanvasContext) const
{
	//LockOnTarget
	CanvasContext.MoveToNewLine();
	CanvasContext.Print(TEXT("{green}[LockOnTarget]"));
	CanvasContext.Printf(TEXT("Can capture Target: %s"), BOOL_TO_TCHAR_COLORED(LockOn->CanCaptureTarget(), green, red));
	CanvasContext.Printf(TEXT("TargetHandler: {yellow}%s"), *GetClassNameSafe(LockOn->GetTargetHandler()));
	CanvasContext.Printf(TEXT("Current Target: {yellow}%s"), *GetNameSafe(LockOn->GetTargetActor()));

	//Invaders
	CanvasContext.MoveToNewLine();
	CanvasContext.Print(TEXT("{green}[Target Invaders]"));
	CanvasContext.Print(CollectInvadersInfo());

	//Modules
	CanvasContext.MoveToNewLine();
	CanvasContext.Print(TEXT("{green}[Active Modules]"));
	CanvasContext.Print(CollectModulesInfo());

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

	if (Cast<UThirdPersonTargetHandler>(LockOn->TargetHandlerImplementation))
	{
		CanvasContext.Printf(TEXT("[{yellow}%s{white}]: Simulate ThirdPersonTargetHandler."), *GetInputHandlerDescription(1));

		if (bSimulateTargetHandler && LockOn->IsTargetLocked())
		{
			CanvasContext.Printf(TEXT("[{yellow}%s{white}]: Switch the Target in the guidance line direction."), *GetInputHandlerDescription(2));
			CanvasContext.Printf(TEXT("[{yellow}%s{white} and {yellow}%s{white}]: Rotate the guidance line."), *GetInputHandlerDescription(3), *GetInputHandlerDescription(4));
		}
	}
}

FString FGDC_LockOnTarget::CollectModulesInfo() const
{
	FString Info;

	for (int32 i = 0; i < LockOn->Modules.Num(); ++i)
	{
		if (const ULockOnTargetModuleBase* const Module = LockOn->Modules[i])
		{
			Info += FString::Printf(TEXT("{white}%i: {yellow}%s"), i, *GetClassNameSafe(Module));

			if (i + 1 != LockOn->Modules.Num())
			{
				Info += TEXT("\n");
			}
		}
	}

	return Info;
}

FString FGDC_LockOnTarget::CollectInvadersInfo() const
{
	FString Info;

	if(LockOn->IsTargetLocked())
	{
		const auto& Invaders = LockOn->GetTargetComponent()->GetInvaders();

		for (int i = 0; i < Invaders.Num(); ++i)
		{
			if (const auto* const Invader = Invaders[i])
			{
				Info += FString::Printf(TEXT("Invader #%i: %s {yellow}%s{white}, "), i + 1, Invader->GetOwner() == LockOn->GetOwner() ? TEXT("{blue}[self]{white}") : TEXT(""), *GetNameSafe(Invader->GetOwner()));
				Info += FString::Printf(TEXT("Duration: {yellow}%.1f{white}s, "), Invader->GetTargetingDuration());
				Info += FString::Printf(TEXT("Socket: %s"), *CollectTargetSocketsInfo(Invader));

				if (i + 1 != Invaders.Num())
				{
					Info += TEXT("\n");
				}
			}
		}
	}

	return Info;
}

FString FGDC_LockOnTarget::CollectTargetSocketsInfo(const ULockOnTargetComponent* InLockOn) const
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

void FGDC_LockOnTarget::DisplayCurrentTarget(FGameplayDebuggerCanvasContext& CanvasContext) const
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

void FGDC_LockOnTarget::SimulateTargetHandler(FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (!bSimulateTargetHandler)
	{
		return;
	}

	if (auto* const TargetHandler = Cast<UThirdPersonTargetHandler>(LockOn->GetTargetHandler()))
	{
		FDelegateHandle CalculateModifierHandle = TargetHandler->OnModifierCalculated.AddRaw(this, &FGDC_LockOnTarget::OnModifierCalculated);
		TargetHandler->FindTarget(TrigonometricToVector2D(SimulatedPlayerInput, true));
		TargetHandler->OnModifierCalculated.Remove(CalculateModifierHandle);

		DrawSimulation(TargetHandler, CanvasContext);
		Modifiers.Empty();
	}
}

void FGDC_LockOnTarget::DrawSimulation(UThirdPersonTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext)
{
	DrawModifiers(CanvasContext);
	DrawScreenOffset(TargetHandler, CanvasContext);
	DrawPlayerInput(TargetHandler, CanvasContext);
}

void FGDC_LockOnTarget::DrawModifiers(FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (Modifiers.Num() == 0)
	{
		return;
	}

	//@TODO: Maybe add some color interpolation from the best to the worst modifier. (red->green)
	Modifiers.Sort([](const auto& lhs, const auto& rhs)
		{
			return lhs.Value < rhs.Value;
		});

	for (int32 i = 0; i < Modifiers.Num(); ++i)
	{
		const FColor DisplayColor = i == 0 ? FColor::Yellow : FColor::White;
		const FVector2D ScreenLoc = CanvasContext.ProjectLocation(Modifiers[i].Key);
		const FString ModifierString = FString::Printf(TEXT("%.2f"), Modifiers[i].Value);

		float ModifierXSize, ModifierYSize;
		CanvasContext.MeasureString(ModifierString, ModifierXSize, ModifierYSize);
		CanvasContext.PrintAt(ScreenLoc.X - ModifierXSize / 2.f, ScreenLoc.Y - ModifierYSize / 2.f, DisplayColor, ModifierString);
	}
}

void FGDC_LockOnTarget::DrawScreenOffset(UThirdPersonTargetHandler* const TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext) const
{
	if (TargetHandler->bScreenCapture)
	{
		//Draw screen offset box.
		FVector2D BoxStart, BoxEnd;
		GetScreenOffsetBox(TargetHandler, CanvasContext, BoxStart, BoxEnd);
		BoxEnd -= BoxStart;

		FCanvasBoxItem Box(BoxStart, BoxEnd);
		Box.SetColor(FLinearColor::Red);
		CanvasContext.Canvas->DrawItem(Box);

		//Print description.
		float StrX, StrY;
		const FString ScreenOffsetStr = GET_MEMBER_NAME_CHECKED(UThirdPersonTargetHandler, ScreenOffset).ToString();
		CanvasContext.MeasureString(ScreenOffsetStr, StrX, StrY);
		CanvasContext.PrintfAt(BoxStart.X + 5.f, BoxEnd.Y + BoxStart.Y - StrY - 5.f, TEXT("{yellow}%s{white}. Only Targets within this box can be processed."), *ScreenOffsetStr);
	}
	else
	{
		//@TODO: Visualize the UThirdPersonTargetHandler::CaptureAngle.
	}
}

void FGDC_LockOnTarget::GetScreenOffsetBox(UThirdPersonTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext, FVector2D& StartPos, FVector2D& EndPos) const
{
	const FVector2D ScreenOffset = TargetHandler->ScreenOffset;
	const float SizeX = static_cast<const float>(CanvasContext.Canvas->SizeX);
	const float SizeY = static_cast<const float>(CanvasContext.Canvas->SizeY);

	StartPos.X = SizeX * ScreenOffset.X / 100.f;
	StartPos.Y = SizeY * ScreenOffset.Y / 100.f;
	EndPos.X = SizeX - StartPos.X;
	EndPos.Y = SizeY - StartPos.Y;
}

void FGDC_LockOnTarget::DrawPlayerInput(UThirdPersonTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (!LockOn->IsTargetLocked())
	{
		return;
	}

	const FVector2D UpVector{ 0.f, 1.f };
	const FVector2D CapturedLocation = CanvasContext.ProjectLocation(LockOn->GetCapturedSocketLocation());
	FVector2D BoxStart, BoxEnd;
	GetScreenOffsetBox(TargetHandler, CanvasContext, BoxStart, BoxEnd);

	auto GetLineLength = [=](FVector2D Direction)
	{
		if (TargetHandler->bScreenCapture)
		{
			const FVector2D AbsDirection = { FMath::Abs(Direction.X), FMath::Abs(Direction.Y) };

			FVector2D BoundaryDir;
			BoundaryDir.Y = FMath::Abs((Direction.Y >= 0.f ? BoxStart.Y : BoxEnd.Y) - CapturedLocation.Y);
			BoundaryDir.X = FMath::Abs((Direction.X >= 0.f ? BoxEnd.X : BoxStart.X) - CapturedLocation.X);

			const float BoundaryAngle = FMath::RadiansToDegrees(FMath::Acos(UpVector | BoundaryDir.GetSafeNormal()));
			const float DirDeltaAngle = FMath::RadiansToDegrees(FMath::Acos(UpVector | AbsDirection));

			if (DirDeltaAngle < BoundaryAngle)
			{
				const float DeltaY = Direction.Y >= 0.f ? CapturedLocation.Y - BoxStart.Y : BoxEnd.Y - CapturedLocation.Y;
				return DeltaY / FMath::Cos(FMath::DegreesToRadians(DirDeltaAngle));
			}
			else
			{
				const float DeltaX = Direction.X >= 0.f ? BoxEnd.X - CapturedLocation.X : CapturedLocation.X - BoxStart.X;
				return DeltaX / FMath::Sin(FMath::DegreesToRadians(DirDeltaAngle));
			}
		}
		else
		{
			//@TODO: Calculate the line length for the CaptureAngle.
			return 1000.f;
		}
	};

	auto DrawClampedLine = [=, &CanvasContext](FVector2D Direction, const FColor& Color, float LineThickness = 1.f) mutable
	{
		Direction *= GetLineLength(Direction);
		Direction.X += CapturedLocation.X;
		Direction.Y = CapturedLocation.Y - Direction.Y;

		FCanvasLineItem CanvasLine(CapturedLocation, Direction);
		CanvasLine.LineThickness = LineThickness;
		CanvasLine.SetColor(Color);
		CanvasContext.Canvas->DrawItem(CanvasLine);
	};

	DrawClampedLine(TrigonometricToVector2D(SimulatedPlayerInput), FColor::Yellow, 2.f);
	DrawClampedLine(TrigonometricToVector2D(SimulatedPlayerInput - TargetHandler->AngleRange), FColor::Red);
	DrawClampedLine(TrigonometricToVector2D(SimulatedPlayerInput + TargetHandler->AngleRange), FColor::Red);

	//Description
	FString TrigAngleStr = FString::Printf(TEXT("%s %.1f"), *GET_MEMBER_NAME_CHECKED(UThirdPersonTargetHandler, AngleRange).ToString(), TargetHandler->AngleRange);
	float StrSizeX, StrSizeY;
	CanvasContext.MeasureString(TrigAngleStr, StrSizeX, StrSizeY);
	CanvasContext.PrintAt(CapturedLocation.X - StrSizeX / 2.f, CapturedLocation.Y - 5.f + StrSizeY, FColor::Yellow, TrigAngleStr);
}

void FGDC_LockOnTarget::OnModifierCalculated(const FFindTargetContext& TargetContext, float Modifier)
{
	//We need this as by default this check may be ignored if the modifier isn't small enough.
	if (TargetContext.TargetHandler->PostModifierCalculationCheck(TargetContext))
	{
		Modifiers.Push({ TargetContext.IteratorTarget.Location, Modifier });
	}
}

FVector2D FGDC_LockOnTarget::TrigonometricToVector2D(float TrigAngle, bool bInvertYAxis) const
{
	const float X = FMath::Cos(FMath::DegreesToRadians(TrigAngle));
	const float Y = FMath::Sin(FMath::DegreesToRadians(TrigAngle));

	return { X, bInvertYAxis ? -Y : Y };
}

void FGDC_LockOnTarget::OnKeyPressedChangeDebugActor()
{
	if (AGameplayDebuggerCategoryReplicator* const Replicator = GetReplicator())
	{
		Replicator->SetDebugActor(LockOn.IsValid() ? LockOn->GetTargetActor() : nullptr);
	}
}

void FGDC_LockOnTarget::OnKeyPressedSimulateTargetHandler()
{
	if (LockOn.IsValid() && Cast<UThirdPersonTargetHandler>(LockOn->TargetHandlerImplementation))
	{
		bSimulateTargetHandler ^= true;
	}
}

void FGDC_LockOnTarget::OnKeyPressedSwitchTarget()
{
	if (LockOn.IsValid() && bSimulateTargetHandler)
	{
		LockOn->SwitchTargetManual(TrigonometricToVector2D(SimulatedPlayerInput, true));
	}
}

void FGDC_LockOnTarget::OnRotateGuidanceLineUp()
{
	if (LockOn.IsValid() && LockOn->IsTargetLocked() && bSimulateTargetHandler)
	{
		SimulatedPlayerInput += GuidanceLineRotationStep;
	}
}

void FGDC_LockOnTarget::OnRotateGuidanceLineDown()
{
	if (LockOn.IsValid() && LockOn->IsTargetLocked() && bSimulateTargetHandler)
	{
		SimulatedPlayerInput -= GuidanceLineRotationStep;
	}
}

#undef BOOL_TO_TCHAR
#undef BOOL_TO_TCHAR_COLORED

#endif //WITH_GAMEPLAY_DEBUGGER
