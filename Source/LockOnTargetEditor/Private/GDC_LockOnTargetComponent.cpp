// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "GDC_LockOnTargetComponent.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "TargetingHelperComponent.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetEditor.h"
#include "LockOnSubobjects/RotationModes/RotationModeBase.h"
#include "LockOnSubobjects/TargetHandlers/DefaultTargetHandler.h"
#include "LockOnSubobjects/LockOnTargetModuleBase.h"
#include "LockOnSubobjects/Modules/RotationModules.h"
#include "LOT_Math.h"

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

FGDC_LockOnTarget::FGDC_LockOnTarget()
	: LockOn(nullptr)
	, SimulatedPlayerInput(0.f)
	, bSimulateTargetHandler(false)
	, bSimulateRotationModes(false)
{
	//SetDataPackReplication(&RepData);
	bShowOnlyWithDebugActor = false;
	CollectDataInterval = 0.1f;
	bShowCategoryName = true;

	//0
	const FGameplayDebuggerInputHandlerConfig EnterKeyConfig(TEXT("Debug Target"), EKeys::Enter.GetFName());
	BindKeyPress(EnterKeyConfig, this, &FGDC_LockOnTarget::OnKeyPressedChangeDebugActor);
	//1
	const FGameplayDebuggerInputHandlerConfig KKeyConfig(TEXT("Simulate Handler"), EKeys::K.GetFName());
	BindKeyPress(KKeyConfig, this, &FGDC_LockOnTarget::OnKeyPressedSimulateTargetHandler);
	//2
	const FGameplayDebuggerInputHandlerConfig LKeyConfig(TEXT("Switch Target"), EKeys::L.GetFName());
	BindKeyPress(LKeyConfig, this, &FGDC_LockOnTarget::OnKeyPressedSwitchTarget);
	//3
	const FGameplayDebuggerInputHandlerConfig AddKeyConfig(TEXT("Add Input"), EKeys::Add.GetFName());
	BindKeyPress(AddKeyConfig, this, &FGDC_LockOnTarget::OnKeyPressedAdd);
	//4
	const FGameplayDebuggerInputHandlerConfig SubtractKeyConfig(TEXT("Subtract Input"), EKeys::Subtract.GetFName());
	BindKeyPress(SubtractKeyConfig, this, &FGDC_LockOnTarget::OnKeyPressedSubtract);
	//5
	const FGameplayDebuggerInputHandlerConfig MKeyConfig(TEXT("Simulate Rotation Mode"), EKeys::M.GetFName());
	BindKeyPress(MKeyConfig, this, &FGDC_LockOnTarget::OnKeyPressedSimulateRotationModes);
}

void FGDC_LockOnTarget::DrawData(APlayerController* Controller, FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (Controller && Controller->GetPawn())
	{
		LockOn = Controller->GetPawn()->FindComponentByClass<ULockOnTargetComponent>();

		if (LockOn.IsValid())
		{
			check(CanvasContext.Canvas.IsValid());

			DisplayDebugInfo(CanvasContext);
			DisplayCurrentTarget(CanvasContext);
			SimulateRotationModes(CanvasContext);
			SimulateTargetHandler(CanvasContext);
		}
	}
}

void FGDC_LockOnTarget::DisplayDebugInfo(FGameplayDebuggerCanvasContext& CanvasContext) const
{
	//General info
	CanvasContext.Printf(TEXT("Target: {yellow} %s"), *GetNameSafe(LockOn->GetTarget()));
	CanvasContext.Printf(TEXT("Captured Socket: {yellow} %s	{white}Target available sockets: %s"), *LockOn->GetCapturedSocket().ToString(), *CollectTargetSocketsInfo());
	CanvasContext.Printf(TEXT("Duration: {yellow} %.1f{white}s"), LockOn->GetTargetingDuration());
	CanvasContext.MoveToNewLine();
	CanvasContext.Printf(TEXT("TargetHandler: {yellow}%s"), *GetNameSafe(LockOn->GetTargetHandler()));
	CanvasContext.MoveToNewLine();
	CanvasContext.Print(CollectModulesInfo());
	CanvasContext.MoveToNewLine();
	CanvasContext.Print(CollectInvadersInfo());

	//Input
	CanvasContext.MoveToNewLine();
	CanvasContext.Printf(TEXT("Is input delay active: {yellow}%s {white}({yellow}%.2f {white}s)"), BOOL_TO_TCHAR(LockOn->IsInputDelayActive()), LockOn->InputProcessingDelay);

	if (LockOn->bFreezeInputAfterSwitch)
	{
		CanvasContext.Printf(TEXT("Is input frozen: {yellow}%s	{white}Unfreeze Threshold: {yellow}%.2f"), BOOL_TO_TCHAR(LockOn->bInputFrozen), LockOn->UnfreezeThreshold);
	}

	CanvasContext.Printf(TEXT("Input Buffer ratio: {yellow}%.2f / %.2f	{white}InputVector2D: X %.3f, Y %.3f"), LockOn->InputBuffer.Size(), LockOn->InputBufferThreshold, LockOn->InputBuffer.X, LockOn->InputBuffer.Y);

	//Controls
	CanvasContext.MoveToNewLine();
	CanvasContext.Printf(TEXT("Press '{yellow}%s{white}' to set the current Target as the debug actor."), *GetInputHandlerDescription(0));

	if (Cast<UDefaultTargetHandler>(LockOn->TargetHandlerImplementation))
	{
		CanvasContext.Printf(TEXT("Press '{yellow}%s{white}' to simulate the DefaultTargetHandler."), *GetInputHandlerDescription(1));

		if (bSimulateTargetHandler && LockOn->IsTargetLocked())
		{
			CanvasContext.Printf(TEXT("Press '{yellow}%s{white}' to switch the Target with the simulated input."), *GetInputHandlerDescription(2));
			CanvasContext.Printf(TEXT("Press '{yellow}%s/%s{white}' to change the simulated input."), *GetInputHandlerDescription(3), *GetInputHandlerDescription(4));
		}
	}

	bool bHasRotationModules = LockOn->Modules.FindByPredicate([](const ULockOnTargetModuleBase* const Module)
		{
			return Cast<URotationModule>(Module);
		}) != nullptr;

	if (LockOn->IsTargetLocked() && bHasRotationModules)
	{
		CanvasContext.Printf(TEXT("Press '{yellow}%s{white}' to simulate the Rotation Modules."), *GetInputHandlerDescription(5));
	}
}

FString FGDC_LockOnTarget::CollectModulesInfo() const
{
	FString Info = TEXT("Modules: ");

	for(int32 i = 0; i < LockOn->Modules.Num(); ++i)
	{
		if(const ULockOnTargetModuleBase* const Module = LockOn->Modules[i])
		{
			Info += FString::Printf(TEXT("\n%i: {yellow}%s {white}Updated {yellow}%s{white}"), i, *GetNameSafe(Module), BOOL_TO_TCHAR(Module->bWantsUpdate && (!Module->bUpdateOnlyWhileLocked || LockOn->IsTargetLocked())));
		}
	}

	return Info;
}

FString FGDC_LockOnTarget::CollectInvadersInfo() const
{
	FString Info = TEXT("Invaders: ");

	if (LockOn->IsTargetLocked() && LockOn->GetHelperComponent())
	{
		uint32 it = 0;

		for (const auto* const Invader : LockOn->GetHelperComponent()->GetInvaders())
		{
			Info += FString::Printf(TEXT("\n%i: %s "), it, *GetNameSafe(Invader->GetOwner()));
			++it;
		}
	}

	return Info;
}

FString FGDC_LockOnTarget::CollectTargetSocketsInfo() const
{
	FString Info;

	if(LockOn->IsTargetLocked() && LockOn->GetHelperComponent())
	{
		for(const FName Socket : LockOn->GetHelperComponent()->GetSockets())
		{
			Info += FString::Printf(TEXT("%s "), *Socket.ToString());
		}
	}

	return Info;
}

void FGDC_LockOnTarget::DisplayCurrentTarget(FGameplayDebuggerCanvasContext& CanvasContext) const
{
	if (const AActor* const CurrentTarget = LockOn->GetTarget())
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

void FGDC_LockOnTarget::SimulateRotationModes(FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (!bSimulateRotationModes || !LockOn->IsTargetLocked())
	{
		return;
	}

	const float SizeX = static_cast<const float>(CanvasContext.Canvas->SizeX);
	const float SizeY = static_cast<const float>(CanvasContext.Canvas->SizeY);
	const float TileOffsetX = 120.f;

	//@TODO: Support all RotationModules. Not only Owner and Control.

	if (UControlRotationModule* const Module = LockOn->FindModuleByClass<UControlRotationModule>())
	{
		if (URotationModeBase* const RotationMode = Module->RotationMode)
		{
			AController* const Controller = LockOn->GetController();
			const FRotator& ControlRotation = Controller->GetControlRotation();
			const FVector LocationFrom = LockOn->GetCameraLocation();
			DrawAxesTile(CanvasContext, RotationMode, { SizeX / 2.f + TileOffsetX, SizeY - 150.f }, ControlRotation, LocationFrom, *GetNameSafe(Module));
		}
	}

	if (UOwnerRotationModule* const Module = LockOn->FindModuleByClass<UOwnerRotationModule>())
	{
		if(URotationModeBase* const RotationMode = Module->RotationMode)
		{
			const FRotator& OwnerRotation = LockOn->GetOwnerRotation();
			const FVector LocationFrom = LockOn->GetOwnerLocation();
			DrawAxesTile(CanvasContext, RotationMode, { SizeX / 2.f - TileOffsetX, SizeY - 150.f }, OwnerRotation, LocationFrom, *GetNameSafe(Module));
		}
	}
}

void FGDC_LockOnTarget::DrawAxesTile(FGameplayDebuggerCanvasContext& CanvasContext, URotationModeBase* RotationMode, FVector2D TileCenter, const FRotator& CurrentRotation, const FVector& LocationFrom, const FString& TileTitle)
{
	const float DeltaTime = CanvasContext.World->GetDeltaSeconds();
	const FVector& TargetLocation = LockOn->GetCapturedFocusLocation();
	const FRotator FinalRotation = RotationMode->URotationModeBase::GetRotation_Implementation(CurrentRotation, LocationFrom, TargetLocation, DeltaTime);
	const FRotator DesiredRotation = RotationMode->GetRotation(CurrentRotation, LocationFrom, TargetLocation, DeltaTime);
	FRotator DeltaRot = FinalRotation - DesiredRotation;
	DeltaRot.Normalize();

	auto DrawAxes = [&CanvasContext](URotationModeBase* RotationMode, FVector2D Center, float HalfLen, const FLinearColor& Color = FLinearColor::White, float Thickness = 0.f)
	{
		//X Axis
		FCanvasLineItem LineX{ FVector2D{ Center.X - HalfLen, Center.Y }, FVector2D{ Center.X + HalfLen, Center.Y } };
		LineX.SetColor(Color);
		LineX.LineThickness = Thickness;
		CanvasContext.Canvas->DrawItem(LineX);

		//Y Axis
		FCanvasLineItem LineY{ FVector2D{ Center.X, Center.Y - HalfLen }, FVector2D{ Center.X, Center.Y + HalfLen } };
		LineY.SetColor(Color);
		LineY.LineThickness = Thickness;
		CanvasContext.Canvas->DrawItem(LineY);
	};

	const float ParentAxisHalfSize = 100.f;
	const FVector2D DeltaAxes{ FMath::Clamp(FMath::Clamp(DeltaRot.Yaw, -90.f, 90.f) / 10.f, -1.f, 1.f) * ParentAxisHalfSize, FMath::Clamp(DeltaRot.Pitch / 10.f, -1.f, 1.f) * ParentAxisHalfSize };
	DrawAxes(RotationMode, TileCenter, ParentAxisHalfSize);
	DrawAxes(RotationMode, { TileCenter.X - DeltaAxes.X, TileCenter.Y + DeltaAxes.Y }, 10.f, FLinearColor::Yellow, 3.f);

	float TitleLenX, TitleLenY;
	CanvasContext.MeasureString(TileTitle, TitleLenX, TitleLenY);
	CanvasContext.PrintAt(TileCenter.X - TitleLenX / 2.f, TileCenter.Y + ParentAxisHalfSize + 5.f, TileTitle);
}

void FGDC_LockOnTarget::SimulateTargetHandler(FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (!bSimulateTargetHandler)
	{
		return;
	}

	if (auto* const TargetHandler = Cast<UDefaultTargetHandler>(LockOn->GetTargetHandler()))
	{
		FDelegateHandle CalculateModifierHandle = TargetHandler->OnModifierCalculated.AddRaw(this, &FGDC_LockOnTarget::OnModifierCalculated);
		TargetHandler->FindTarget(TrigonometricToVector2D(SimulatedPlayerInput, true));
		TargetHandler->OnModifierCalculated.Remove(CalculateModifierHandle);

		DrawSimulation(TargetHandler, CanvasContext);
		Modifiers.Empty();
	}
}

void FGDC_LockOnTarget::DrawSimulation(UDefaultTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext)
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

void FGDC_LockOnTarget::DrawScreenOffset(UDefaultTargetHandler* const TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext) const
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
		const FString ScreenOffsetName = GetScreenOffsetName();
		float StrX, StrY;
		CanvasContext.MeasureString(ScreenOffsetName, StrX, StrY);
		CanvasContext.PrintfAt(BoxStart.X + 5.f, BoxEnd.Y + BoxStart.Y - StrY - 5.f, TEXT("{yellow}%s{white}. Only Targets within this box can be processed."), *ScreenOffsetName);
	}
	else
	{
		//@TODO: Visualize the UDefaultTargetHandler::CaptureAngle.
	}
}

FVector2D FGDC_LockOnTarget::GetScreenOffset(UDefaultTargetHandler* const TargetHandler) const
{
	return LockOn->IsTargetLocked() ? TargetHandler->SwitchingScreenOffset : TargetHandler->FindingScreenOffset;
}

void FGDC_LockOnTarget::GetScreenOffsetBox(UDefaultTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext, FVector2D& StartPos, FVector2D& EndPos) const
{
	const FVector2D ScreenOffset = GetScreenOffset(TargetHandler);
	const float SizeX = static_cast<const float>(CanvasContext.Canvas->SizeX);
	const float SizeY = static_cast<const float>(CanvasContext.Canvas->SizeY);

	StartPos.X = SizeX * ScreenOffset.X / 100.f;
	StartPos.Y = SizeY * ScreenOffset.Y / 100.f;
	EndPos.X = SizeX - StartPos.X;
	EndPos.Y = SizeY - StartPos.Y;
}

FString FGDC_LockOnTarget::GetScreenOffsetName() const
{
	return LockOn->IsTargetLocked() ? GET_MEMBER_NAME_CHECKED(UDefaultTargetHandler, SwitchingScreenOffset).ToString() : GET_MEMBER_NAME_CHECKED(UDefaultTargetHandler, FindingScreenOffset).ToString();
}

void FGDC_LockOnTarget::DrawPlayerInput(UDefaultTargetHandler* TargetHandler, FGameplayDebuggerCanvasContext& CanvasContext)
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

			const float BoundaryAngle = LOT_Math::GetAngle(UpVector, BoundaryDir);
			const float DirDeltaAngle = LOT_Math::GetAngle(UpVector, AbsDirection);

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
	FString TrigAngleStr = FString::Printf(TEXT("%s %.1f"), *GET_MEMBER_NAME_CHECKED(UDefaultTargetHandler, AngleRange).ToString(), TargetHandler->AngleRange);
	float StrSizeX, StrSizeY;
	CanvasContext.MeasureString(TrigAngleStr, StrSizeX, StrSizeY);
	CanvasContext.PrintAt(CapturedLocation.X - StrSizeX / 2.f, CapturedLocation.Y - 5.f + StrSizeY, FColor::Yellow, TrigAngleStr);
}

void FGDC_LockOnTarget::OnModifierCalculated(const FFindTargetContext& TargetContext, float Modifier)
{
	//We need this as by default this check may be ignored if the modifier isn't small enough.
	if (TargetContext.ContextOwner->PostModifierCalculationCheck(TargetContext))
	{
		Modifiers.Push({ TargetContext.IteratorTarget.SocketWorldLocation, Modifier });
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
		Replicator->SetDebugActor(LockOn.IsValid() ? LockOn->GetTarget() : nullptr);
	}
}

void FGDC_LockOnTarget::OnKeyPressedSimulateTargetHandler()
{
	if (LockOn.IsValid() && Cast<UDefaultTargetHandler>(LockOn->TargetHandlerImplementation))
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

void FGDC_LockOnTarget::OnKeyPressedAdd()
{
	if (LockOn.IsValid() && LockOn->IsTargetLocked() && bSimulateTargetHandler)
	{
		SimulatedPlayerInput += 5.f;
	}
}

void FGDC_LockOnTarget::OnKeyPressedSubtract()
{
	if (LockOn.IsValid() && LockOn->IsTargetLocked() && bSimulateTargetHandler)
	{
		SimulatedPlayerInput -= 5.f;
	}
}

void FGDC_LockOnTarget::OnKeyPressedSimulateRotationModes()
{
	if (LockOn.IsValid() & LockOn->IsTargetLocked())
	{
		bSimulateRotationModes ^= true;
	}
}

#endif //WITH_GAMEPLAY_DEBUGGER

#undef BOOL_TO_TCHAR
