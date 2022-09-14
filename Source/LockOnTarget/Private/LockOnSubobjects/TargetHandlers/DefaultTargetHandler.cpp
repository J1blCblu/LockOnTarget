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

//@TODO: Maybe move to the config properties or project settings.
namespace DTH_SolverConfig
{
	//Modifier default value before weights are applied.
	constexpr float PureDefaultModifier = 1000.f;
	//Precision below which the weight won't be taken into account.
	constexpr float WeightPrecision = 1e-2f;
	//Distance max factor above which the distance won't be taken into account (in uu).
	constexpr float DistanceMaxFactor = 2750.f;
	//Angle max factor above which the angle won't be taken into account (in deg).
	constexpr float AngleMaxFactor = 90.f;
	//Due to the best Target should have the least modifier we want to avoid an exact match with the 0.f modifier.
	//e.g. don't capture the most centered Target on the screen at 10'000m while we have a Target in front of us.
	//So, we have something like a free zone (threshold), where several Target will have the same least modifier.
	constexpr float MinimumThreshold = 0.035f;
}

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

void FFindTargetContext::PrepareIteratorTargetContext(FName Socket)
{
	check(IteratorTarget.HelperComponent);

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
	, TargetCaptureRadiusModifier(1.f)
{
	TraceObjectChannels.Emplace(ECollisionChannel::ECC_WorldStatic);
	bWantsUpdate = false;
	bUpdateOnlyWhileLocked = false;
}

/*******************************************************************************************/
/******************************* Target Handler Interface **********************************/
/*******************************************************************************************/

void UDefaultTargetHandler::OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);
	StopLineOfSightTimer();
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

	//Check if the TargetingHelperComponent or the TargetActor are pending to kill.
	if (!IsValid(CurrentTarget))
	{
		return HandleTargetClearing(EUnlockReasonBitmask::TargetInvalidation);
	}

	//Check the TargetingHelperComponent state.
	if (!CurrentHC->CanBeCaptured(LockOn))
	{
		return HandleTargetClearing(EUnlockReasonBitmask::HelperComponentDiscard);
	}

	const float DistanceToTarget = (CurrentTarget->GetActorLocation() - LockOn->GetCameraLocation()).Size();

	//Lost Distance check.
	if (DistanceToTarget > (CurrentHC->CaptureRadius * TargetCaptureRadiusModifier + CurrentHC->LostOffsetRadius))
	{
		return HandleTargetClearing(EUnlockReasonBitmask::OutOfLostDistance);
	}

	//@TODO: Use listener pattern instead with the delegate in the HelperComponent.
	//Check the validity of the captured socket.
	if (!CurrentHC->GetSockets().Contains(LockOn->GetCapturedSocket()))
	{
		return HandleTargetClearing(EUnlockReasonBitmask::CapturedSocketInvalidation);
	}

	//Line of Sight check.
	if (bLineOfSightCheck && LostTargetDelay > 0.f) //don't trace if timer <= 0.f.
	{
		LineOfSightTrace(CurrentTarget, LockOn->GetCapturedSocketLocation()) ? StopLineOfSightTimer() : StartLineOfSightTimer();
	}

	return true;
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

bool UDefaultTargetHandler::IsTargetable(UTargetingHelperComponent* HelpComp) const
{
	DTH_EVENT(IsTargetable, Yellow);

	//Necessary checks are here, auxiliary checks are in IsTargetableCustom().
	const ULockOnTargetComponent* const LockOn = GetLockOnTargetComponent();
	const AActor* const HelperOwner = HelpComp->GetOwner();

	/** Check validity of the TargetingHelperComponent's Owner. */
	if (!IsValid(HelperOwner))
	{
		return false;
	}

	/** Check that a Candidate is not the Owner (to not capture yourself). */
	if (HelperOwner == LockOn->GetOwner())
	{
		return false;
	}

	/** Check TargetingHelperComponent's condition. */
	if (!HelpComp->CanBeCaptured(LockOn))
	{
		return false;
	}

	return IsTargetableCustom(HelpComp);
}

bool UDefaultTargetHandler::IsTargetableCustom_Implementation(UTargetingHelperComponent* HelperComponent) const
{
	//@TODO: Maybe calculate size squared to avoid the root computation.
	const float DistanceToTarget = (HelperComponent->GetOwner()->GetActorLocation() - GetLockOnTargetComponent()->GetCameraLocation()).Size();

	/** Capture the Target radius check. */
	if (DistanceToTarget > (HelperComponent->CaptureRadius * TargetCaptureRadiusModifier))
	{
		return false;
	}

	/** MinimumCaptureRadius to the Camera check. */
	if (DistanceToTarget < HelperComponent->MinimumCaptureRadius)
	{
		return false;
	}

	return true;
}

void UDefaultTargetHandler::FindBestSocket(FTargetModifier& TargetModifier, FFindTargetContext& TargetContext)
{
	for (FName TargetSocket : TargetContext.IteratorTarget.HelperComponent->GetSockets())
	{
		DTH_EVENT(SocketCalc, Blue);

		TargetContext.PrepareIteratorTargetContext(TargetSocket);

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
		const ULockOnTargetComponent* const LockOn = GetLockOnTargetComponent();
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

	const FVector CameraLocation = GetLockOnTargetComponent()->GetCameraLocation();
	const FVector DirectionToTargetSocket = TargetContext.IteratorTarget.SocketWorldLocation - CameraLocation;

	float FinalModifier = DTH_SolverConfig::PureDefaultModifier;

	//Final modifier will be divided into 2 parts by a weight proportion.
	//1 part will be unmodified and another one will be modified by a factor.
	auto ApplyFactor = [&FinalModifier](float Weight, float Factor)
	{
		check(Weight >= 0.f && Weight <= 1.f);
		check(Factor >= 0.f);
		FinalModifier = FinalModifier * (1.f - Weight) + FinalModifier * Weight * Factor;
	};

	//Distance weight modifier adjustment.
	if (DistanceWeight > DTH_SolverConfig::WeightPrecision)
	{
		ApplyFactor(DistanceWeight, FMath::Min(DirectionToTargetSocket.Size() / DTH_SolverConfig::DistanceMaxFactor, 1.f));
	}

	//Angle weight modifier adjustment.
	if (GetAngleWeight() > DTH_SolverConfig::WeightPrecision)
	{
		//If any Target is locked then use the direction to the captured socket, otherwise the camera rotation will be used with an offset.
		const FVector Direction = TargetContext.bIsSwitchingTarget ? TargetContext.CurrentTarget.SocketWorldLocation - CameraLocation 
			: (TargetContext.LockOnTargetComponent->GetCameraRotation().Quaternion() * ViewRotationOffset.Quaternion()).GetAxisX();

		const float DeltaAngle = LOT_Math::GetAngle(DirectionToTargetSocket, Direction);
		ApplyFactor(GetAngleWeight(), FMath::Clamp(DeltaAngle / DTH_SolverConfig::AngleMaxFactor, DTH_SolverConfig::MinimumThreshold, 1.f));
	}

	//Player input weight modifier adjustment.
	if (TargetContext.bIsSwitchingTarget && PlayerInputWeight > DTH_SolverConfig::WeightPrecision
		&& TargetContext.CurrentTarget.bIsScreenPositionValid && TargetContext.IteratorTarget.bIsScreenPositionValid)
	{
		const float InputDeltaAngle = LOT_Math::GetAngle(TargetContext.PlayerRawInput, TargetContext.IteratorTarget.SocketScreenPosition - TargetContext.CurrentTarget.SocketScreenPosition);
		ApplyFactor(PlayerInputWeight, FMath::Clamp(InputDeltaAngle / AngleRange, DTH_SolverConfig::MinimumThreshold, 1.f));
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

bool UDefaultTargetHandler::HandleTargetClearing(EUnlockReasonBitmask UnlockReason)
{
	ULockOnTargetComponent* const LockOn = GetLockOnTargetComponent();
	LockOn->ClearTargetManual(AutoFindTargetFlags & static_cast<std::underlying_type_t<EUnlockReasonBitmask>>(UnlockReason));
	return LockOn->IsTargetLocked();
}

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
