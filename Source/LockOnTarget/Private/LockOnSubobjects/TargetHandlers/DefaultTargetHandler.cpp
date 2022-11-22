// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/TargetHandlers/DefaultTargetHandler.h"
#include "LockOnTargetComponent.h"
#include "TargetingHelperComponent.h"
#include "LOT_Math.h"
#include "LockOnTargetDefines.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include <type_traits>

//----------------------------------------------------------------//
//  FTargetModifier
//----------------------------------------------------------------//

FTargetModifier::FTargetModifier(const FTargetContext& TargetContext)
	: TargetInfo(TargetContext.HelperComponent, TargetContext.SocketName)
{
	//Do something.
}

//----------------------------------------------------------------//
//  FFindTargetContext
//----------------------------------------------------------------//

FFindTargetContext::FFindTargetContext(UDefaultTargetHandler* Owner, ULockOnTargetComponent* LockOn, FVector2D PlayerInput)
	: ContextOwner(Owner)
	, LockOnTargetComponent(LockOn)
	, PlayerRawInput(PlayerInput)
{
	check(ContextOwner);
	check(LockOnTargetComponent);

	bIsSwitchingTarget = LockOnTargetComponent->IsTargetLocked();

	if (bIsSwitchingTarget)
	{
		CurrentTarget.HelperComponent = LockOnTargetComponent->GetHelperComponent();
		CurrentTarget.SocketName = LockOnTargetComponent->GetCapturedSocket();
		CurrentTarget.SocketWorldLocation = LockOnTargetComponent->GetCapturedSocketLocation();
		CurrentTarget.bIsScreenPositionValid = VectorToScreenPosition(CurrentTarget.SocketWorldLocation, CurrentTarget.SocketScreenPosition);
	}
}

void FFindTargetContext::PrepareTargetSocket(FName Socket)
{
	IteratorTarget.SocketName = Socket;
	IteratorTarget.SocketWorldLocation = IteratorTarget.HelperComponent->GetSocketLocation(IteratorTarget.SocketName);

	if (bIsSwitchingTarget || ContextOwner->bScreenCapture)
	{
		IteratorTarget.bIsScreenPositionValid = VectorToScreenPosition(IteratorTarget.SocketWorldLocation, IteratorTarget.SocketScreenPosition);
	}
}

bool FFindTargetContext::VectorToScreenPosition(const FVector& Location, FVector2D& OutScreenPosition) const
{
	const APlayerController* const PC = LockOnTargetComponent->GetPlayerController();
	return PC && PC->ProjectWorldLocationToScreen(Location, OutScreenPosition);
}

//----------------------------------------------------------------//
//  UDefaultTargetHandler
//----------------------------------------------------------------//

UDefaultTargetHandler::UDefaultTargetHandler()
	: AutoFindTargetFlags(0b00011111)
	, bDistanceCheck(true)
	, bScreenCapture(true)
	, FindingScreenOffset(15.f, 10.f)
	, SwitchingScreenOffset(7.5f, 2.5f)
	, CaptureAngle(35.f)
	, DistanceWeight(0.735)
	, AngleWeightWhileFinding(0.6f)
	, AngleWeightWhileSwitching(0.535f)
	, PlayerInputWeight(0.097f)
	, ViewRotationOffset(0.f)
	, AngleRange(60.f)
	, bLineOfSightCheck(true)
	, LostTargetDelay(3.f)
	, CheckInterval(0.1f)
	, TargetCaptureRadiusModifier(1.f)
	, PureDefaultModifier(1000.f)
	, WeightPrecision(1e-2f)
	, DistanceMaxFactor(2750.f)
	, AngleMaxFactor(90.f)
	, MinimumThreshold(0.035f)
	, CheckTimer(0.f)
{
	TraceObjectChannels.Emplace(ECollisionChannel::ECC_WorldStatic);
	bWantsUpdate = false;
	bUpdateOnlyWhileLocked = false;
}

/*******************************************************************************************/
/******************************* Target Handler Interface **********************************/
/*******************************************************************************************/

void UDefaultTargetHandler::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);
	CheckConfigInvariants();
}

void UDefaultTargetHandler::OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);
	StopLineOfSightTimer();
	CheckTimer = 0.f;
}

FTargetInfo UDefaultTargetHandler::FindTarget_Implementation(FVector2D PlayerInput)
{
	DTH_EVENT(FindTargetImpl, Red);

	//Context initialization. Automatically determines if any Target is locked.
	FFindTargetContext TargetContext(this, GetLockOnTargetComponent(), PlayerInput);

	return FindTargetInternal(TargetContext);
}

bool UDefaultTargetHandler::CanContinueTargeting_Implementation()
{
	const ULockOnTargetComponent* const LockOn = GetLockOnTargetComponent();
	const UTargetingHelperComponent* const CurrentHC = LockOn->GetHelperComponent();
	const AActor* const CurrentTarget = LockOn->GetTarget();

	check(LockOn && CurrentHC && CurrentTarget);

	//Check the TargetingHelperComponent state.
	if (!CurrentHC->CanBeCaptured(LockOn))
	{
		return HandleTargetClearing(EUnlockReasonBitmask::HelperComponentDiscard);
	}

	//Lost Distance check.
	if(bDistanceCheck)
	{
		//Here we can use SquaredDistance, cause user won't see it in the GameplayDebugger.
		const float DistanceToTarget = (CurrentTarget->GetActorLocation() - LockOn->GetCameraLocation()).SizeSquared();
		const float LostRadius = CurrentHC->CaptureRadius * TargetCaptureRadiusModifier + CurrentHC->LostOffsetRadius;
		
		if (DistanceToTarget > FMath::Square(LostRadius))
		{
			return HandleTargetClearing(EUnlockReasonBitmask::OutOfLostDistance);
		}
	}

	//Line of Sight check.
	if (ShouldTargetBeTraced())
	{
		LineOfSightTrace(CurrentTarget, LockOn->GetCapturedSocketLocation()) ? StopLineOfSightTimer() : StartLineOfSightTimer();
	}

	return true;
}

void UDefaultTargetHandler::HandleTargetEndPlay_Implementation(UTargetingHelperComponent* HelperComponent)
{
	HandleTargetClearing(EUnlockReasonBitmask::TargetInvalidation);
}

void UDefaultTargetHandler::HandleSocketRemoval_Implementation(FName RemovedSocket)
{
	HandleTargetClearing(EUnlockReasonBitmask::CapturedSocketInvalidation);
}

bool UDefaultTargetHandler::HandleTargetClearing(EUnlockReasonBitmask UnlockReason)
{
	ULockOnTargetComponent* const LockOn = GetLockOnTargetComponent();
	LockOn->ClearTargetManual(AutoFindTargetFlags & static_cast<std::underlying_type_t<EUnlockReasonBitmask>>(UnlockReason));
	return LockOn->IsTargetLocked();
}

/*******************************************************************************************/
/******************************* Target Finding ********************************************/
/*******************************************************************************************/

FTargetInfo UDefaultTargetHandler::FindTargetInternal(FFindTargetContext& TargetContext)
{
	FTargetModifier BestTarget;

	for (TObjectIterator<UTargetingHelperComponent> It; It; ++It)
	{
		DTH_EVENT(TargetCalc, White);

		if (It->GetWorld() == GetWorld() && IsTargetable(*It))
		{
			TargetContext.IteratorTarget.HelperComponent = *It;
			FindBestSocket(BestTarget, TargetContext);
		}
	}

	return BestTarget.TargetInfo;
}

bool UDefaultTargetHandler::IsTargetable(UTargetingHelperComponent* HelperComponent) const
{
	DTH_EVENT(IsTargetable, Yellow);

	const AActor* const HelperOwner = HelperComponent->GetOwner();

	/** Target should have a valid owner. */
	if (!IsValid(HelperOwner))
	{
		return false;
	}

	const ULockOnTargetComponent* const LockOn = GetLockOnTargetComponent();

	/** Capturing yourself isn't allowed. */
	if (HelperOwner == LockOn->GetOwner())
	{
		return false;
	}

	/** Can the Target actually be captured. */
	if (!HelperComponent->CanBeCaptured(LockOn))
	{
		return false;
	}

	/** Check the distance to the Target. */
	if (bDistanceCheck)
	{
		const float DistanceToTarget = (LockOn->GetCameraLocation() - HelperOwner->GetActorLocation()).SizeSquared();
		const float CaptureRadius = FMath::Square(HelperComponent->CaptureRadius * TargetCaptureRadiusModifier);
		const float MinimumCaptureRadius = FMath::Square(HelperComponent->MinimumCaptureRadius);

		if (DistanceToTarget > CaptureRadius || DistanceToTarget < MinimumCaptureRadius)
		{
			return false;
		}
	}

	return IsTargetableCustom(HelperComponent);
}

bool UDefaultTargetHandler::IsTargetableCustom_Implementation(const UTargetingHelperComponent* HelperComponent) const
{
	//Unimplemented.
	return true;
}

void UDefaultTargetHandler::FindBestSocket(FTargetModifier& TargetModifier, FFindTargetContext& TargetContext)
{
	for (FName TargetSocket : TargetContext.IteratorTarget.HelperComponent->GetSockets())
	{
		DTH_EVENT(SocketCalc, Blue);

		TargetContext.PrepareTargetSocket(TargetSocket);

		if (PreModifierCalculationCheck(TargetContext))
		{
			const float CurrentModifier = CalculateTargetModifier(TargetContext);

			//Basically used by FGDC_LockOnTarget.
			OnModifierCalculated.Broadcast(TargetContext, CurrentModifier);

			if (CurrentModifier < TargetModifier.Modifier && PostModifierCalculationCheck(TargetContext))
			{
				TargetModifier.Modifier = CurrentModifier;
				TargetModifier.TargetInfo.SocketForCapturing = TargetContext.IteratorTarget.SocketName;
				TargetModifier.TargetInfo.HelperComponent = TargetContext.IteratorTarget.HelperComponent;
			}
		}
	}
}

bool UDefaultTargetHandler::PreModifierCalculationCheck(const FFindTargetContext& TargetContext) const
{
	DTH_EVENT(PreModifierCalcCheck, Green);

	//If we're trying to find another socket within the current Target then skip the same socket.
	if (TargetContext.CurrentTarget.HelperComponent == TargetContext.IteratorTarget.HelperComponent
		&& TargetContext.CurrentTarget.SocketName == TargetContext.IteratorTarget.SocketName)
	{
		return false;
	}

	//Check the range for the Player input.
	if (TargetContext.bIsSwitchingTarget && TargetContext.CurrentTarget.bIsScreenPositionValid && TargetContext.IteratorTarget.bIsScreenPositionValid)
	{
		const float Angle = LOT_Math::GetAngle(TargetContext.PlayerRawInput, TargetContext.IteratorTarget.SocketScreenPosition - TargetContext.CurrentTarget.SocketScreenPosition);;

		if (Angle > AngleRange)
		{
			return false;
		}
	}

	//Visibility check. 
	if (bScreenCapture)
	{
		if (!TargetContext.IteratorTarget.bIsScreenPositionValid || !IsTargetOnScreen(TargetContext.IteratorTarget.SocketScreenPosition))
		{
			return false;
		}
	}
	else
	{
		const ULockOnTargetComponent* const LockOn = TargetContext.LockOnTargetComponent;
		const FVector DirectionToSocket = TargetContext.IteratorTarget.SocketWorldLocation - LockOn->GetCameraLocation();
		const float Angle = LOT_Math::GetAngle(LockOn->GetCameraForwardVector(), DirectionToSocket);

		if (Angle > CaptureAngle)
		{
			return false;
		}
	}

	return true;
}

float UDefaultTargetHandler::CalculateTargetModifier_Implementation(const FFindTargetContext& TargetContext) const
{
	DTH_EVENT(ModifierCalc, Red);

	float FinalModifier = PureDefaultModifier;

	//Final modifier will be divided into 2 parts by a weight proportion.
	//1 part will be unmodified and another one will be modified by a factor.
	auto ApplyFactor = [&FinalModifier](float Weight, float Factor)
	{
		check(Weight >= 0.f && Weight <= 1.f);
		check(Factor >= 0.f);
		FinalModifier = FinalModifier * (1.f - Weight) + FinalModifier * Weight * Factor;
	};

	const FVector CameraLocation = TargetContext.LockOnTargetComponent->GetCameraLocation();
	const FVector DirectionToTargetSocket = TargetContext.IteratorTarget.SocketWorldLocation - CameraLocation;

	//Distance weight modifier adjustment.
	if (DistanceWeight > WeightPrecision)
	{
		const float DistanceRatio = DirectionToTargetSocket.SizeSquared() / FMath::Square(DistanceMaxFactor);
		ApplyFactor(DistanceWeight, FMath::Min(DistanceRatio, 1.f));
	}

	//Angle weight modifier adjustment.
	if (GetAngleWeight() > WeightPrecision)
	{
		FVector Direction;

		//If any Target is locked then use the direction to the captured socket, otherwise the camera rotation will be used with an offset.
		if(TargetContext.bIsSwitchingTarget)
		{
			Direction = TargetContext.CurrentTarget.SocketWorldLocation - CameraLocation;
		}
		else
		{
			Direction = (TargetContext.LockOnTargetComponent->GetCameraRotation().Quaternion() * ViewRotationOffset.Quaternion()).GetAxisX();
		}

		const float DeltaAngle = LOT_Math::GetAngle(DirectionToTargetSocket, Direction);
		ApplyFactor(GetAngleWeight(), FMath::Clamp(DeltaAngle / AngleMaxFactor, MinimumThreshold, 1.f));
	}

	//Player input weight modifier adjustment.
	if (TargetContext.bIsSwitchingTarget 
		&& PlayerInputWeight > WeightPrecision
		&& TargetContext.CurrentTarget.bIsScreenPositionValid 
		&& TargetContext.IteratorTarget.bIsScreenPositionValid)
	{
		const float InputDeltaAngle = LOT_Math::GetAngle(TargetContext.PlayerRawInput, TargetContext.IteratorTarget.SocketScreenPosition - TargetContext.CurrentTarget.SocketScreenPosition);
		ApplyFactor(PlayerInputWeight, FMath::Clamp(InputDeltaAngle / AngleRange, MinimumThreshold, 1.f));
	}

	return FinalModifier;
}

bool UDefaultTargetHandler::PostModifierCalculationCheck_Implementation(const FFindTargetContext& TargetContext) const
{
	DTH_EVENT(PostModifierCalcCheck, Yellow);

	//LineOfSight check
	if (bLineOfSightCheck && !LineOfSightTrace(TargetContext.IteratorTarget.HelperComponent->GetOwner(), TargetContext.IteratorTarget.SocketWorldLocation))
	{
		return false;
	}

	return true;
}

/*******************************************************************************************/
/*************************************** Misc **********************************************/
/*******************************************************************************************/

bool UDefaultTargetHandler::IsTargetOnScreen(FVector2D ScreenPosition) const
{
	const APlayerController* const PC = GetLockOnTargetComponent()->GetPlayerController();

	if (PC)
	{
		int32 VX, VY;
		PC->GetViewportSize(VX, VY);
		//Percents to ratio.
		const FVector2D ScreenOffset = GetScreenOffset() / 100.f;
		const int32 VXClamped = static_cast<int32>(VX * ScreenOffset.X);
		const int32 VYClamped = static_cast<int32>(VY * ScreenOffset.Y);

		return ScreenPosition.X > (0 + VXClamped) && ScreenPosition.X < (VX - VXClamped)
			&& ScreenPosition.Y >(0 + VYClamped) && ScreenPosition.Y < (VY - VYClamped);
	}

	//Skip check for non-player controlled owners.
	return true;
}

FVector2D UDefaultTargetHandler::GetScreenOffset() const
{
	return GetLockOnTargetComponent()->IsTargetLocked() ? SwitchingScreenOffset : FindingScreenOffset;
}

float UDefaultTargetHandler::GetAngleWeight() const
{
	return GetLockOnTargetComponent()->IsTargetLocked() ? AngleWeightWhileSwitching : AngleWeightWhileFinding;;
}

void UDefaultTargetHandler::CheckConfigInvariants() const
{
	check(PureDefaultModifier > 0.f);
	check(DistanceMaxFactor > 0.f);
	check(WeightPrecision > 0.f && WeightPrecision < 1.f);
	check(AngleMaxFactor > 0.f && AngleMaxFactor < 180.f);
	check(MinimumThreshold > 0.f && MinimumThreshold < 1.f);
}

/*******************************************************************************************/
/*******************************  Line Of Sight  *******************************************/
/*******************************************************************************************/

void UDefaultTargetHandler::StartLineOfSightTimer()
{
	if (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(LOSDelayHandler))
	{
		GetWorld()->GetTimerManager().SetTimer(LOSDelayHandler, FTimerDelegate::CreateUObject(this, &UDefaultTargetHandler::OnLineOfSightExpiration), LostTargetDelay, false);
	}
}

void UDefaultTargetHandler::StopLineOfSightTimer()
{
	if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(LOSDelayHandler))
	{
		GetWorld()->GetTimerManager().ClearTimer(LOSDelayHandler);
	}
}

void UDefaultTargetHandler::OnLineOfSightExpiration()
{
	HandleTargetClearing(EUnlockReasonBitmask::LineOfSightFail);
}

bool UDefaultTargetHandler::ShouldTargetBeTraced()
{
	//don't trace if delay <= 0.f.
	bool bShouldTrace = bLineOfSightCheck && LostTargetDelay > 0.f;

	if(const UWorld* const World = GetWorld())
	{
		CheckTimer += World->GetDeltaSeconds();
		bShouldTrace &= CheckTimer > CheckInterval;

		if(bShouldTrace)
		{
			CheckTimer = 0.f;
		}
	}

	return bShouldTrace;
}

bool UDefaultTargetHandler::LineOfSightTrace(const AActor* const Target, const FVector& Location) const
{
	DTH_EVENT(LineOfSight, Red);

	if (!GetWorld() || !IsValid(Target))
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
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(LockOnTrace));
	TraceParams.AddIgnoredActor(Target);
	TraceParams.AddIgnoredActor(GetLockOnTargetComponent()->GetOwner());

	return !GetWorld()->LineTraceSingleByObjectType(HitRes, GetLockOnTargetComponent()->GetCameraLocation(), Location, ObjParams, TraceParams);
}

/*******************************************************************************************/
/******************************* Debug and Editor Only  ************************************/
/*******************************************************************************************/
#if WITH_EDITOR

void UDefaultTargetHandler::PostEditChangeProperty(struct FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	const FName PropertyName = Event.GetPropertyName();
	const FName MemberPropertyName = Event.MemberProperty->GetFName();

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UDefaultTargetHandler, FindingScreenOffset))
	{
		FindingScreenOffset = FindingScreenOffset.ClampAxes(0.f, 50.f);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UDefaultTargetHandler, SwitchingScreenOffset))
	{
		SwitchingScreenOffset = SwitchingScreenOffset.ClampAxes(0.f, 50.f);
	}
}

#endif
