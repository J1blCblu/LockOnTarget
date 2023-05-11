// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "TargetHandlers/ThirdPersonTargetHandler.h"
#include "LockOnTargetComponent.h"
#include "TargetComponent.h"
#include "TargetManager.h"
#include "LockOnTargetDefines.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Camera/CameraTypes.h"

UThirdPersonTargetHandler::UThirdPersonTargetHandler()
	: AutoFindTargetFlags(0b00011111)
	, DistanceWeight(0.735f)
	, AngleWeight(0.6f)
	, PlayerInputWeight(0.097f)
	, PureDefaultModifier(1000.f)
	, WeightPrecision(0.01f)
	, DistanceMaxFactor(2750.f)
	, AngleMaxFactor(90.f)
	, MinimumThreshold(0.035f)
	, bDistanceCheck(true)
	, MinimumRadius(0.f)
	, TargetCaptureRadiusModifier(1.f)
	, bUseCameraPointOfView(true)
	, bScreenCapture(true)
	, ScreenOffset(5.f, 2.5f)
	, ViewAngle(42.f)
	, ViewRotationOffset(10.f, 0.f, 0.f)
	, bRecentRenderCheck(true)
	, RecentTolerance(0.1f)
	, AngleRange(60.f)
	, bLineOfSightCheck(true)
	, LostTargetDelay(3.f)
	, CheckInterval(0.1f)
	, LineOfSightCheckTimer(0.f)
{
	TraceObjectChannels.Emplace(ECollisionChannel::ECC_WorldStatic);
}

/*******************************************************************************************/
/******************************* Target Handler Interface **********************************/
/*******************************************************************************************/

FTargetInfo UThirdPersonTargetHandler::FindTarget_Implementation(FVector2D PlayerInput)
{
	const EContextMode ContextMode = GetLockOnTargetComponent()->IsTargetLocked() ? EContextMode::Switch : EContextMode::Find;
	FFindTargetContext Context = CreateFindTargetContext(ContextMode, PlayerInput);
	return FindTargetInternal(Context);
}

void UThirdPersonTargetHandler::CheckTargetState_Implementation(const FTargetInfo& Target, float DeltaTime)
{
	LOT_SCOPED_EVENT(TargetHandlerCheckState, Red);

	FVector ViewLocation, ViewDirection;
	GetPointOfView(ViewLocation, ViewDirection);

	const AActor* const TargetActor = Target.TargetComponent->GetOwner();

	if (bDistanceCheck)
	{
		const float DistanceSq = (TargetActor->GetActorLocation() - ViewLocation).SizeSquared();
		const float LostRadius = Target.TargetComponent->CaptureRadius * TargetCaptureRadiusModifier + Target.TargetComponent->LostOffsetRadius;

		if (DistanceSq > FMath::Square(LostRadius))
		{
			if (IsAnyUnlockReasonSet(AutoFindTargetFlags, EUnlockReason::DistanceFail))
			{
				TryFindAndSetNewTarget(true);
			}
			else
			{
				GetLockOnTargetComponent()->ClearTargetManual();
			}

			return;
		}
	}

	//Line of Sight check.
	if (bLineOfSightCheck && LostTargetDelay > 0.f)
	{
		LineOfSightCheckTimer += DeltaTime;

		if (LineOfSightCheckTimer > CheckInterval)
		{
			LineOfSightCheckTimer -= CheckInterval;

			if (LineOfSightTrace(ViewLocation, Target.TargetComponent->GetSocketLocation(Target.Socket), TargetActor))
			{
				StopLineOfSightTimer();
			}
			else
			{
				StartLineOfSightTimer();
			}
		}
	}
}

void UThirdPersonTargetHandler::HandleTargetException_Implementation(const FTargetInfo& Target, ETargetExceptionType Exception)
{
	LOT_SCOPED_EVENT(TargetHandlerHandleException, Red);

	if (IsAnyUnlockReasonSet(AutoFindTargetFlags, ConvertTargetExceptionToUnlockReason(Exception)))
	{
		TryFindAndSetNewTarget(false);
	}
}

void UThirdPersonTargetHandler::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);

	StopLineOfSightTimer();
	LineOfSightCheckTimer = 0.f;
}

/*******************************************************************************************/
/*********************************** Context ***********************************************/
/*******************************************************************************************/

FFindTargetContext UThirdPersonTargetHandler::CreateFindTargetContext(EContextMode Mode, FVector2D Input)
{
	LOT_SCOPED_EVENT(TargetHandlerCreateContext, Blue);

	FFindTargetContext Context;
	Context.Mode = Mode;
	Context.PlayerInput = Input;
	Context.PlayerInputDirection = Input.GetSafeNormal();
	Context.TargetHandler = this;
	Context.Instigator = GetLockOnTargetComponent();
	Context.InstigatorPawn = Cast<APawn>(Context.Instigator->GetOwner());
	Context.PlayerController = GetPlayerController();
	GetPointOfView(Context.ViewLocation, Context.ViewDirection);
	Context.ViewDirectionWithOffset = (Context.ViewDirection.ToOrientationQuat() * ViewRotationOffset.Quaternion()).GetAxisX();

	if (Mode == EContextMode::Switch)
	{
		//Populate data for CapturedTarget.
		checkf(Context.Instigator->IsTargetLocked(), TEXT("Target must be locked by LockOnTargetComponent for the Switch context mode."));
		Context.CapturedTarget.Target = Context.Instigator->GetTargetComponent();
		PrepareTargetContext(Context, Context.CapturedTarget, Context.Instigator->GetCapturedSocket());
	}

	return Context;
}

void UThirdPersonTargetHandler::PrepareTargetContext(const FFindTargetContext& FindTargetContext, FTargetContext& OutTargetContext, FName InSocket)
{
	LOT_SCOPED_EVENT(TargetHandlerTargetContext, Blue);

	if (ensure(OutTargetContext.Target))
	{
		OutTargetContext.Socket = InSocket;
		OutTargetContext.Location = OutTargetContext.Target->GetSocketLocation(InSocket);
		OutTargetContext.VectorToSocket = OutTargetContext.Location - FindTargetContext.ViewLocation;
		OutTargetContext.Direction = OutTargetContext.VectorToSocket.GetSafeNormal();
	}
}

void UThirdPersonTargetHandler::UpdateContext(FFindTargetContext& Context)
{
	LOT_SCOPED_EVENT(TargetHandlerUpdateContext, Blue);

	if (Context.Mode == EContextMode::Switch)
	{
		//Converts DeltaRotator between Targets to DeltaDirection2D in PlayerInput space (screen by default).
		//Faster but slightly less accurate than projecting socket locations onto the screen and subtracting them afterwards.
		//It also doesn't need the participation of the PlayerController.

		//@TODO: Still need to find a better algorithm. Maybe without projecting into PlayerInput space.

		const FRotator CapturedTargetRot = Context.CapturedTarget.Direction.ToOrientationRotator();
		const FRotator ItTargetRot = Context.IteratorTarget.Direction.ToOrientationRotator();

		FRotator Delta = { CapturedTargetRot.Pitch - ItTargetRot.Pitch, ItTargetRot.Yaw - CapturedTargetRot.Yaw, 0.f };
		Delta.Normalize();

		const FVector2D Direction2D = { Delta.Yaw, Delta.Pitch };

		Context.DeltaAngle2D = FMath::RadiansToDegrees(FMath::Acos(Context.PlayerInputDirection | Direction2D.GetSafeNormal()));
	}
}

/*******************************************************************************************/
/******************************* Target Finding ********************************************/
/*******************************************************************************************/

void UThirdPersonTargetHandler::TryFindAndSetNewTarget(bool bClearTargetIfFailed)
{
	FFindTargetContext Context = CreateFindTargetContext(EContextMode::Find);

	const FTargetInfo Target = FindTargetInternal(Context);

	if (Context.Instigator->CanTargetBeCaptured(Target))
	{
		Context.Instigator->SetLockOnTargetManualByInfo(Target);
	}
	else if (bClearTargetIfFailed)
	{
		Context.Instigator->ClearTargetManual();
	}
}

FTargetInfo UThirdPersonTargetHandler::FindTargetInternal(FFindTargetContext& TargetContext)
{
	//@TODO: Try batching, don't process each Target individually. Store all Target in an array and run ~4 passes: IsTargetable, PreCheck, ModCalc, PostCheck.

	LOT_SCOPED_EVENT(TargetHandlerFindTarget, Red);

	FTargetModifier BestTarget{ FTargetInfo::NULL_TARGET, FLT_MAX };

	for (UTargetComponent* const Target : UTargetManager::Get(*GetWorld()).GetAllTargets())
	{
		LOT_SCOPED_EVENT(TargetHandlerTargetCalculation, Orange);

		TargetContext.IteratorTarget.Target = Target;

		if (IsTargetable(TargetContext))
		{
			FindBestSocket(BestTarget, TargetContext);
		}
	}

	return BestTarget.Key;
}

bool UThirdPersonTargetHandler::IsTargetable(const FFindTargetContext& TargetContext) const
{
	LOT_SCOPED_EVENT(TargetHandlerIsTargetable, Green);

	const UTargetComponent* const Target = TargetContext.IteratorTarget.Target;

	/** Core checks. */
	if (!IsTargetValid(Target))
	{
		return false;
	}

	const AActor* const TargetActor = Target->GetOwner();

	/** Render check. */
	if (bRecentRenderCheck)
	{
		if (!TargetActor->WasRecentlyRendered(RecentTolerance))
		{
			return false;
		}
	}

	/** Distance check. */
	if (bDistanceCheck)
	{
		const float DistanceSq = (TargetContext.ViewLocation - TargetActor->GetActorLocation()).SizeSquared();
		const float MaxRadius = Target->CaptureRadius * TargetCaptureRadiusModifier;

		if (DistanceSq > FMath::Square(MaxRadius) || DistanceSq < FMath::Square(MinimumRadius))
		{
			return false;
		}
	}

	return IsTargetableCustom(Target);
}

bool UThirdPersonTargetHandler::IsTargetableCustom_Implementation(const UTargetComponent* TargetComponent) const
{
	//Unimplemented.
	return true;
}

void UThirdPersonTargetHandler::FindBestSocket(FTargetModifier& BestTarget, FFindTargetContext& TargetContext)
{
	for (const FName TargetSocket : TargetContext.IteratorTarget.Target->GetSockets())
	{
		LOT_SCOPED_EVENT(TargetHandlerProcessSocket, Red);

		//Skip already captured Target and Socket.
		if (TargetContext.CapturedTarget.Target == TargetContext.IteratorTarget.Target && TargetContext.CapturedTarget.Socket == TargetSocket)
		{
			continue;
		}

		PrepareTargetContext(TargetContext, TargetContext.IteratorTarget, TargetSocket);
		UpdateContext(TargetContext);

		if (PreModifierCalculationCheck(TargetContext))
		{
			const float CurrentModifier = CalculateTargetModifier(TargetContext);

			//Basically used by FGDC_LockOnTarget to visualize all modifiers.
			OnModifierCalculated.Broadcast(TargetContext, CurrentModifier);

			if (CurrentModifier < BestTarget.Value && PostModifierCalculationCheck(TargetContext))
			{
				BestTarget.Key = TargetContext.IteratorTarget;
				BestTarget.Value = CurrentModifier;
			}
		}
	}
}

bool UThirdPersonTargetHandler::PreModifierCalculationCheck(const FFindTargetContext& TargetContext) const
{
	LOT_SCOPED_EVENT(TargetHandlerPreCheck, Green);

	if (TargetContext.Mode == EContextMode::Switch)
	{
		//Check the range for the Player input.
		if (TargetContext.DeltaAngle2D > AngleRange)
		{
			return false;
		}
	}

	//Cone view check.
	if (!bScreenCapture)
	{
		//@TODO: Look at AIHelpers.h CheckIsTargetInSightCone().
		const float Product = TargetContext.ViewDirection | TargetContext.IteratorTarget.Direction;

		if (FMath::RadiansToDegrees(FMath::Acos(Product)) > ViewAngle)
		{
			return false;
		}
	}

	return true;
}

float UThirdPersonTargetHandler::CalculateTargetModifier_Implementation(const FFindTargetContext& TargetContext) const
{
	LOT_SCOPED_EVENT(TargetHandlerModifierCalculation, Red);

	float FinalModifier = PureDefaultModifier;

	//Final modifier will be divided into 2 parts by a weight proportion.
	//1 part will be unmodified and another one will be modified by a factor.
	auto ApplyFactor = [&FinalModifier, this](float Weight, float Ratio)
	{
		check(Weight >= 0.f && Weight <= 1.f);
		const float Factor = FMath::Clamp(Ratio, MinimumThreshold, 1.f);
		FinalModifier = FinalModifier * (1.f - Weight) + FinalModifier * Weight * Factor;
	};

	if (DistanceWeight > WeightPrecision)
	{
		const float Ratio = TargetContext.IteratorTarget.VectorToSocket.SizeSquared() / FMath::Square(DistanceMaxFactor);
		ApplyFactor(DistanceWeight, Ratio);
	}

	if (AngleWeight > WeightPrecision)
	{
		const FVector ContextDirection = TargetContext.Mode == EContextMode::Find ? TargetContext.ViewDirectionWithOffset : TargetContext.CapturedTarget.Direction;
		const float Ratio = FMath::RadiansToDegrees(FMath::Acos(TargetContext.IteratorTarget.Direction | ContextDirection)) / AngleMaxFactor;
		ApplyFactor(AngleWeight, Ratio);
	}

	if (TargetContext.Mode == EContextMode::Switch && PlayerInputWeight > WeightPrecision)
	{
		const float Ratio = TargetContext.DeltaAngle2D / AngleRange;
		ApplyFactor(PlayerInputWeight, Ratio);
	}

	return FinalModifier;
}

bool UThirdPersonTargetHandler::PostModifierCalculationCheck_Implementation(const FFindTargetContext& TargetContext) const
{
	LOT_SCOPED_EVENT(TargetHandlerPostCheck, Yellow);

	//Visibility check.
	if (bScreenCapture && IsValid(TargetContext.PlayerController))
	{
		FVector2D ScreenPosition;

		if (!TargetContext.PlayerController->ProjectWorldLocationToScreen(TargetContext.IteratorTarget.Location, ScreenPosition, true)
			|| !IsTargetOnScreen(TargetContext.PlayerController, ScreenPosition))
		{
			return false;
		}
	}

	//LineOfSight check
	if (bLineOfSightCheck)
	{
		if (!LineOfSightTrace(TargetContext.ViewLocation, TargetContext.IteratorTarget.Location, TargetContext.IteratorTarget.Target->GetOwner()))
		{
			return false;
		}
	}

	return true;
}

/*******************************************************************************************/
/*******************************  Line Of Sight  *******************************************/
/*******************************************************************************************/

void UThirdPersonTargetHandler::StartLineOfSightTimer()
{
	if (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(LineOfSightExpirationHandle))
	{
		GetWorld()->GetTimerManager().SetTimer(LineOfSightExpirationHandle, FTimerDelegate::CreateUObject(this, &UThirdPersonTargetHandler::OnLineOfSightExpiration), LostTargetDelay, false);
	}
}

void UThirdPersonTargetHandler::StopLineOfSightTimer()
{
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LineOfSightExpirationHandle);
	}
}

void UThirdPersonTargetHandler::OnLineOfSightExpiration()
{
	if (IsAnyUnlockReasonSet(AutoFindTargetFlags, EUnlockReason::LineOfSightFail))
	{
		TryFindAndSetNewTarget(true);
	}
	else
	{
		GetLockOnTargetComponent()->ClearTargetManual();
	}
}

bool UThirdPersonTargetHandler::LineOfSightTrace(const FVector& From, const FVector& To, const AActor* const TargetToIgnore) const
{
	LOT_SCOPED_EVENT(TargerHandlerLineOfSight, Yellow);

	if (!GetWorld() || !IsValid(TargetToIgnore))
	{
		return false;
	}

	FHitResult HitRes;
	FCollisionObjectQueryParams ObjParams;

	for (const auto& TraceChannel : TraceObjectChannels)
	{
		ObjParams.AddObjectTypesToQuery(TraceChannel);
	}

	//SCENE QUERY PARAMS for the debug. Tag = LockOnTrace. In the console print TraceTag<LockOnTrace> or TraceTagAll
	FCollisionQueryParams CollisionParams(SCENE_QUERY_STAT(LockOnTrace));
	CollisionParams.AddIgnoredActor(TargetToIgnore);
	CollisionParams.AddIgnoredActor(GetLockOnTargetComponent()->GetOwner());

	GetWorld()->LineTraceSingleByObjectType(HitRes, From, To, ObjParams, CollisionParams);

	return !HitRes.bBlockingHit;
}

/*******************************************************************************************/
/*********************************** Helpers ***********************************************/
/*******************************************************************************************/

bool UThirdPersonTargetHandler::IsTargetOnScreen(const APlayerController* const PlayerController, FVector2D ScreenPosition) const
{
	bool bResult = false;

	if (PlayerController)
	{
		//@TODO: Find the actual screen size in local split screen.
		int32 VX, VY;
		PlayerController->GetViewportSize(VX, VY);

		//Percents to ratio.
		const FVector2D OffsetRatio = ScreenOffset / 100.f;

		const int32 XOffset = static_cast<int32>(VX * OffsetRatio.X);
		const int32 YOffset = static_cast<int32>(VY * OffsetRatio.Y);

		bResult = ScreenPosition.X > XOffset		//Left
			&& ScreenPosition.X < (VX - XOffset)	//Right
			&& ScreenPosition.Y > YOffset			//Top
			&& ScreenPosition.Y < (VY - YOffset);	//Bottom
	}

	return bResult;
}

void UThirdPersonTargetHandler::GetPointOfView_Implementation(FVector& OutLocation, FVector& OutDirection) const
{
	if (const AActor* const OwnerActor = GetLockOnTargetComponent()->GetOwner())
	{
		OutLocation = OwnerActor->GetActorLocation();
		OutDirection = OwnerActor->GetActorForwardVector();

		if (bUseCameraPointOfView && GetPlayerController())
		{
			if (const APlayerCameraManager* const Manager = GetPlayerController()->PlayerCameraManager)
			{
				const FMinimalViewInfo& View = Manager->GetCameraCacheView();

				OutLocation = View.Location;
				OutDirection = View.Rotation.Vector();
			}
		}
	}
}
