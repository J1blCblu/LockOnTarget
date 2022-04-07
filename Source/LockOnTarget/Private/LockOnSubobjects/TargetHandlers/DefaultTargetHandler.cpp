// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/TargetHandlers/DefaultTargetHandler.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "LockOnTargetComponent.h"
#include "TargetingHelperComponent.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "Utilities/LOTC_BPLibrary.h"
#include "Utilities/Structs.h"

#if WITH_EDITORONLY_DATA
#include "DrawDebugHelpers.h"
#endif

UDefaultTargetHandler::UDefaultTargetHandler()
	: AutoFindTargetFlags(0b11111)
	, bScreenCapture(true)
	, FindingScreenOffset(15.f, 10.f)
	, SwitchingScreenOffset(5.f, 0.f)
	, CaptureAngle(35.f)
	, bCalculateDistance(true)
	, AngleWeightWhileFinding(0.85f)
	, AngleWeightWhileSwitching(0.85f)
	, TrigonometricInputWeight(0.3f)
	, CameraRotationOffsetForCalculations(0.f)
	, TrigonometricAngleRange(60.f)
	, bLineOfSightCheck(true)
	, LostTargetDelay(1.5f)
{
	TraceObjectChannels.Push(ECollisionChannel::ECC_WorldStatic);
}

/*******************************************************************************************/
/******************************* Target Handler Interface **********************************/
/*******************************************************************************************/

void UDefaultTargetHandler::OnTargetUnlockedNative()
{
	Super::OnTargetUnlocked();
	StopLineOfSightTimer();
}

FTargetInfo UDefaultTargetHandler::FindTarget_Implementation()
{
	return FindTargetNative();
}

bool UDefaultTargetHandler::SwitchTarget_Implementation(FTargetInfo& TargetInfo, float PlayerInput)
{
	return SwitchTargetNative(TargetInfo, PlayerInput);
}

bool UDefaultTargetHandler::CanContinueTargeting_Implementation()
{
	//Check if the TargetingHelperComponent or the TargetActor are pending to kill.
	if (!IsValid(GetLockOn()->GetTarget()))
	{
		return HandleTargetClearing(EUnlockReasonBitmask::E_TargetInvalidation);
	}

	//Check the TargetingHelperComponent state.
	if (!GetLockOn()->GetHelperComponent()->CanBeTargeted(GetLockOn()))
	{
		return HandleTargetClearing(EUnlockReasonBitmask::E_HelperComponentDiscard);
	}

	//Check the validity of the captured socket.
	if (!GetLockOn()->GetHelperComponent()->Sockets.Contains(GetLockOn()->GetCapturedSocket()))
	{
		return HandleTargetClearing(EUnlockReasonBitmask::E_CapturedSocketInvalidation);
	}

	float DistanceToTarget = (GetLockOn()->GetTarget()->GetActorLocation() - GetLockOn()->GetCameraLocation()).Size();

	//Lost Distance check.
	if (DistanceToTarget > GetLockOn()->GetHelperComponent()->LostRadius)
	{
		return HandleTargetClearing(EUnlockReasonBitmask::E_OutOfLostDistance);
	}

	//Line of Sight check.
	if (bLineOfSightCheck && LostTargetDelay > 0.f) //don't trace if timer <= 0.f.
	{
		LineOfSightTrace(GetLockOn()->GetTarget(), GetLockOn()->GetCapturedLocation()) ? StopLineOfSightTimer() : StartLineOfSightTimer();
	}

	return true;
}

/*******************************************************************************************/
/******************************* Target Finding ********************************************/
/*******************************************************************************************/

FTargetInfo UDefaultTargetHandler::FindTargetNative(float PlayerInput /*= -1.f*/) const
{
#if LOC_INSIGHTS
	SCOPED_NAMED_EVENT(LOC_TargetFinding, FColor::Purple);
#endif

	float BestModifier = FLT_MAX;
	UTargetingHelperComponent* BestTarget = nullptr;
	FName BestSocket;

	for (TObjectIterator<UTargetingHelperComponent> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && IsTargetable(*It))
		{
#if LOC_INSIGHTS
			SCOPED_NAMED_EVENT(LOC_TatgetCalculations, FColor::Green);
#endif

			const TSet<FName>& TargetSockets = It->Sockets;

			for (const FName& TargetSocket : TargetSockets)
			{
				const FVector SocketLocation = It->GetSocketLocation(TargetSocket);

				if (IsSocketValid(TargetSocket, *It, PlayerInput, SocketLocation))
				{
					const float CurrentModifier = CalculateTargetModifier(SocketLocation, *It, PlayerInput);

					if (CurrentModifier < BestModifier)
					{
						BestModifier = CurrentModifier;
						BestSocket = TargetSocket;
						BestTarget = *It;
					}

#if WITH_EDITORONLY_DATA
					if (bDisplayModifier)
					{
						DrawDebugString(GetWorld(), SocketLocation, FString::Printf(TEXT("%.1f\n"), CurrentModifier), nullptr, ModifierColor, ModifierDuration, false, 1.2f);
					}
#endif
				}
			}
		}
	}

	return {BestTarget, BestSocket};
}

bool UDefaultTargetHandler::IsTargetable(UTargetingHelperComponent* HelpComp) const
{
#if LOC_INSIGHTS
	SCOPED_NAMED_EVENT(LOC_IsTargetable, FColor::Red);
#endif

	/** Check TargetingHelperComponent's condition. */
	if (!HelpComp->CanBeTargeted(GetLockOn()))
	{
		return false;
	}

	AActor* HelperOwner = HelpComp->GetOwner();

	/** Check validity of TargetingHelperComponent's Owner. */
	if (!IsValid(HelperOwner))
	{
		return false;
	}

	/** Check that Candidate is not the current Target (if exist) or the Owner. */
	if (HelperOwner == GetLockOn()->GetTarget() || HelperOwner == GetLockOn()->GetOwner())
	{
		return false;
	}

	const FVector DirectionToActor = HelperOwner->GetActorLocation() - GetLockOn()->GetCameraLocation();
	const float Distance = DirectionToActor.Size();

	/** Capture the Target radius check. */
	if (Distance > HelpComp->CaptureRadius)
	{
		return false;
	}

	/** MinDistance to the Camera check. */
	if (Distance < HelpComp->MinDistance)
	{
		return false;
	}

	return true;
}

bool UDefaultTargetHandler::IsSocketValid(const FName& Socket, UTargetingHelperComponent* HelperComponent, float PlayerInput, const FVector& SocketLocation) const
{
	//LineOfSight check
	if (bLineOfSightCheck && !LineOfSightTrace(HelperComponent->GetOwner(), SocketLocation))
	{
		return false;
	}

	//Check the range for the Player input.
	if (GetLockOn()->IsTargetLocked())
	{
		const FVector OriginPoint = GetLockOn()->GetCapturedLocation();
		const float Angle = ULOTC_BPLibrary::GetTrigonometricAngle3D(SocketLocation, OriginPoint, GetLockOn()->GetCameraRotation());

		if (!ULOTC_BPLibrary::IsAngleInRange(Angle, PlayerInput, TrigonometricAngleRange))
		{
			return false;
		}
	}

	//Visibility check.
	if (bScreenCapture)
	{
		const FVector2D ScreenBorder = GetLockOn()->IsTargetLocked() ? SwitchingScreenOffset : FindingScreenOffset;

		if (!ULOTC_BPLibrary::IsVectorOnScreen(GetLockOn()->GetPlayerController(), SocketLocation, ScreenBorder))
		{
			return false;
		}
	}
	else
	{
		const FVector DirectionToSocket = SocketLocation - GetLockOn()->GetCameraLocation();
		const float Angle = ULOTC_BPLibrary::GetAngleDeg(GetLockOn()->GetCameraForwardVector(), DirectionToSocket);

		if (Angle > CaptureAngle)
		{
			return false;
		}
	}

	return true;
}

float UDefaultTargetHandler::CalculateTargetModifier_Implementation(const FVector& Location, UTargetingHelperComponent* TargetHelperComponent, float PlayerInput) const
{
	const FVector CameraLocation = GetLockOn()->GetCameraLocation();
	const FVector VectorForAngleCalculation = GetLockOn()->IsTargetLocked() ? GetLockOn()->GetCapturedLocation() - CameraLocation : (GetLockOn()->GetCameraRotation().Quaternion() * CameraRotationOffsetForCalculations.Quaternion()).Rotator().Vector();
	const FVector Direction = Location - CameraLocation;

	float FinalModifier = 1000.f;

	//Final modifier = distance between the Owner and the Target.
	//In below sections Final modifier will be divided into 2 parts as a percentage.
	//1 part will be unmodified and another will be modified by the weights.
	//At the end 2 parts will be summed.

	if (bCalculateDistance)
	{
		//Modifier uses Distance to Target.ca
		FinalModifier = Direction.Size();
	}

	//Ange weight modifier calculation
	if (!FMath::IsNearlyZero(AngleWeightWhileFinding, 1e-3f) || !FMath::IsNearlyZero(AngleWeightWhileSwitching, 1e-3f))
	{
		const float Angle = ULOTC_BPLibrary::GetAngleDeg(Direction, VectorForAngleCalculation);
		const float AngleRatio = (Angle / 180.f);

		float FM1 = FinalModifier * (GetLockOn()->IsTargetLocked() ? AngleWeightWhileSwitching : AngleWeightWhileFinding);
		const float FM2 = FinalModifier - FM1;

		FM1 *= AngleRatio;
		FinalModifier = FM1 + FM2;
	}

	//Player trigonometric input modifier calculation.
	if (GetLockOn()->IsTargetLocked() && !FMath::IsNearlyZero(TrigonometricInputWeight, 1e-3f))
	{
		const float TrigAngleToTarget = ULOTC_BPLibrary::GetTrigonometricAngle3D(Location, GetLockOn()->GetCapturedLocation(), GetLockOn()->GetCameraRotation());
		const float DeltaTrigAngle = FMath::FindDeltaAngleDegrees(PlayerInput, TrigAngleToTarget);
		const float TrigRatio = FMath::Abs(DeltaTrigAngle) / TrigonometricAngleRange;

		float FM1 = FinalModifier * TrigonometricInputWeight;
		const float FM2 = FinalModifier - FM1;

		FM1 *= TrigRatio;
		FinalModifier = FM1 + FM2;
	}

	return FinalModifier;
}

/*******************************************************************************************/
/******************************* Switching Socket ******************************************/
/*******************************************************************************************/

bool UDefaultTargetHandler::SwitchTargetNative(FTargetInfo& TargetInfo, float PlayerInput) const
{
	FTargetInfo NewTargetInfo = FindTargetNative(PlayerInput);
	TPair<FName, float> NewSocket = TrySwitchSocket(PlayerInput);

	if (IsValid(NewTargetInfo.HelperComponent))
	{
		if (NewSocket.Key != GetLockOn()->GetCapturedSocket())
		{
			//If we find the NewTarget with the socket or the Socket for the current Target.
			float NewTargetModifier = CalculateTargetModifier(NewTargetInfo.HelperComponent->GetSocketLocation(NewTargetInfo.SocketForCapturing), NewTargetInfo.HelperComponent, PlayerInput);

			TargetInfo = NewSocket.Value < NewTargetModifier ? FTargetInfo(GetLockOn()->GetHelperComponent(), NewSocket.Key) : NewTargetInfo;
		}
		else
		{
			//if we find the new Target, but not the new Socket in the current Target.
			TargetInfo = NewTargetInfo;
		}

		return true;
	}
	else
	{
		//if we doesn't find the new Target but find the new Socket in the Current Target.
		if (NewSocket.Key != GetLockOn()->GetCapturedSocket())
		{
			TargetInfo = FTargetInfo(GetLockOn()->GetHelperComponent(), NewSocket.Key);
			return true;
		}
	}

	return false;
}

TPair<FName, float> UDefaultTargetHandler::TrySwitchSocket(float PlayerInput) const
{
	const FName& CapturedSocket = GetLockOn()->GetCapturedSocket();
	UTargetingHelperComponent* const CapturedHC = GetLockOn()->GetHelperComponent();

	TPair<FName, float> BestSocket{CapturedSocket, FLT_MAX};

	const TSet<FName>& TargetSockets = CapturedHC->Sockets;

	for (const FName& Socket : TargetSockets)
	{
		//Skip the already captured socket.
		if (Socket == CapturedSocket)
		{
			continue;
		}

		const FVector SocketLocation = CapturedHC->GetSocketLocation(Socket);

		//Check the socket validity.
		if (!IsSocketValid(Socket, CapturedHC, PlayerInput, SocketLocation))
		{
			continue;
		}

		//Calculate the socket modifier.
		float CurrentModifier = CalculateTargetModifier(SocketLocation, CapturedHC, PlayerInput);

		if (CurrentModifier < BestSocket.Value)
		{
			//Assign the socket to the best Socket.
			BestSocket.Key = Socket;
			BestSocket.Value = CurrentModifier;
		}

#if WITH_EDITORONLY_DATA
		if (bDisplayModifier)
		{
			DrawDebugString(GetWorld(), CapturedHC->GetSocketLocation(Socket), FString::Printf(TEXT("%.1f\n"), CurrentModifier), nullptr, ModifierColor, ModifierDuration, false, 1.2f);
		}
#endif
	}

	return BestSocket;
}

/*******************************************************************************************/
/*******************************  Line Of Sight  *******************************************/
/*******************************************************************************************/

void UDefaultTargetHandler::StartLineOfSightTimer()
{
	if (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(LOSDelayHandler))
	{
		FTimerDelegate TimerCallback;
		TimerCallback.BindUObject(this, &UDefaultTargetHandler::OnLineOfSightFail);
		GetWorld()->GetTimerManager().SetTimer(LOSDelayHandler, TimerCallback, LostTargetDelay, false);
	}
}

void UDefaultTargetHandler::StopLineOfSightTimer()
{
	if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(LOSDelayHandler))
	{
		GetWorld()->GetTimerManager().ClearTimer(LOSDelayHandler);
	}
}

void UDefaultTargetHandler::OnLineOfSightFail()
{
	HandleTargetClearing(EUnlockReasonBitmask::E_LineOfSightFail);
}

bool UDefaultTargetHandler::LineOfSightTrace(const AActor* const Target, const FVector& Location) const
{
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

	//SCENE QUERY PARAMS for debug. Tag = LockOnTrace. In console print TraceTag<LockOnTrace> or TraceTagAll
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(LockOnTrace));
	TraceParams.AddIgnoredActor(Target);
	TraceParams.AddIgnoredActor(GetLockOn()->GetOwner());

	return !GetWorld()->LineTraceSingleByObjectType(HitRes, GetLockOn()->GetCameraLocation(), Location, ObjParams, TraceParams);
}

/*******************************************************************************************/
/******************************* Helpers Methods *******************************************/
/*******************************************************************************************/

bool UDefaultTargetHandler::HandleTargetClearing(EUnlockReasonBitmask UnlockReason)
{
	GetLockOn()->ClearTargetManual(AutoFindTargetFlags & static_cast<uint8>(UnlockReason));
	
	return GetLockOn()->IsTargetLocked();
}

/*******************************************************************************************/
/******************************* Debug and Editor Only  ************************************/
/*******************************************************************************************/
#if WITH_EDITORONLY_DATA

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
