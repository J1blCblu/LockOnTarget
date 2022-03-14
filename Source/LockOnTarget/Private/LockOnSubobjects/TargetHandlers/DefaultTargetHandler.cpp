// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/TargetHandlers/DefaultTargetHandler.h"
#include "TargetingHelperComponent.h"
#include "LockOnTargetComponent.h"
#include "Utilities/Structs.h"
#include "Utilities/LOTC_BPLibrary.h"
#include "UObject/UObjectIterator.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

#if WITH_EDITORONLY_DATA
#include "DrawDebugHelpers.h"
#endif

UDefaultTargetHandler::UDefaultTargetHandler()
	: AutoFindTargetFlags(0b1111)
	, bScreenCapture(true)
	, FindingScreenOffset(15.f, 10.f)
	, SwitchingScreenOffset(5.f, 0.f)
	, CaptureAngle(35.f)
	, bCalculateDistance(true)
	, AngleWeightWhileFinding(0.85f)
	, AngleWeightWhileSwitching(0.85f)
	, TrigonometricInputWeight(0.3f)
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
	//Check if TargetActor or HelperComponent pending to kill.
	if (!IsValid(GetLockOn()->GetTarget()) || !IsValid(GetLockOn()->GetHelperComponent()))
	{
		return HandleTargetClearing(EUnlockReasonBitmask::E_TargetInvalidation);
	}

	//Check helper component state.
	if (!GetLockOn()->GetHelperComponent()->CanBeTargeted(GetLockOn()))
	{
		return HandleTargetClearing(EUnlockReasonBitmask::E_HelperComponentDiscard);
	}

	float DistanceToTarget = (GetLockOn()->GetTarget()->GetActorLocation() - GetLockOn()->GetCameraLocation()).Size();

	//Lost Distance check.
	if (DistanceToTarget > GetLockOn()->GetHelperComponent()->LostRadius)
	{
		return HandleTargetClearing(EUnlockReasonBitmask::E_OutOfLostDistance);
	}

	//Line of Sight check.
	if (bLineOfSightCheck && LostTargetDelay >= 0.f) //do not trace if timer < 0.f.
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

			const TArray<FName>& TargetSockets = It->Sockets;

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
						DrawDebugString(GetWorld(), SocketLocation, FString::Printf(TEXT("Modifier %.1f\n"), CurrentModifier), nullptr, ModifierColor, ModifierDuration, false, 1.2f);
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

	/** Check HelperComponent condition. */
	if(!HelpComp->CanBeTargeted(GetLockOn()))
	{
		return false;
	}

	AActor* HelperOwner = HelpComp->GetOwner();

	/** Check validity of HelperComponent's Owner. */
	if (!IsValid(HelperOwner))
	{
		return false;
	}

	/** Check that Candidate is not CurrentTarget(if exist) or Owner. */
	if (HelperOwner == GetLockOn()->GetTarget() || HelperOwner == GetLockOn()->GetOwner())
	{
		return false;
	}

	const FVector DirectionToActor = HelperOwner->GetActorLocation() - GetLockOn()->GetCameraLocation();
	const float Distance = DirectionToActor.Size();

	/** MinDistance to Camera check. */
	if (Distance < HelpComp->MinDistance)
	{
		return false;
	}

	/** Capture Target radius check. */
	if (Distance > HelpComp->CaptureRadius)
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

	//Check range for Player input.
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
		const float Angle = ULOTC_BPLibrary::GetAngle(GetLockOn()->GetCameraForwardVector(), DirectionToSocket);

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
	const FVector ForwardVector = GetLockOn()->IsTargetLocked() ? GetLockOn()->GetCapturedLocation() - CameraLocation : (GetLockOn()->GetCameraRotation().Quaternion() * FRotator(ScreenCenterOffsetWhileFinding.Y, ScreenCenterOffsetWhileFinding.X, 0.f).Quaternion()).Rotator().Vector();
	const FVector Direction = Location - CameraLocation;

	float FinalModifier = 1000.f;

	//Final modifier = distance between Owner and Target.
	//In below sections Final modifier will be divided into 2 parts as a percentage.
	//1 part will be unmodified and another will be modified by weights.
	//At the end 2 parts will be summed.

	if (bCalculateDistance)
	{
		//Modifier uses Distance to Target.ca
		FinalModifier = Direction.Size();
	}

	//Ange weight modifier calculation
	if (!FMath::IsNearlyZero(AngleWeightWhileFinding, 0.001f) || !FMath::IsNearlyZero(AngleWeightWhileSwitching, 0.001f))
	{
		const float Angle = ULOTC_BPLibrary::GetAngle(Direction, ForwardVector);
		const float AngleRatio = (Angle / 180.f);

		float FM1 = FinalModifier * (PlayerInput < 0.f ? AngleWeightWhileFinding : AngleWeightWhileSwitching);
		const float FM2 = FinalModifier - FM1;

		FM1 *= AngleRatio;
		FinalModifier = FM1 + FM2;
	}

	//Player trigonometric input modifier calculation.
	if (GetLockOn()->IsTargetLocked() && !FMath::IsNearlyZero(TrigonometricInputWeight, 0.001f))
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
			//If we find NewTarget and Socket for switching in current Target.
			float NewTargetModifier = CalculateTargetModifier(NewTargetInfo.HelperComponent->GetSocketLocation(NewTargetInfo.SocketForCapturing), NewTargetInfo.HelperComponent, PlayerInput);

			TargetInfo = NewSocket.Value < NewTargetModifier ? FTargetInfo(GetLockOn()->GetHelperComponent(), NewSocket.Key) : NewTargetInfo;
		}
		else
		{
			//if we find New Target, but not New Socket in Current Target.
			TargetInfo = NewTargetInfo;
		}

		return true;
	}
	else
	{
		//if we doesn't find New Target and find New Socket in Current Target.
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

	TPair<FName, float> BestSocket {CapturedSocket, FLT_MAX};

	const TArray<FName>& TargetSockets = CapturedHC->Sockets;

	for (const FName& Socket : TargetSockets)
	{
		//Skip already captured socket.
		if (Socket == CapturedSocket)
		{
			continue;
		}

		const FVector SocketLocation = CapturedHC->GetSocketLocation(Socket);

		//check socket for validity.
		if (!IsSocketValid(Socket, CapturedHC, PlayerInput, SocketLocation))
		{
			continue;
		}

		//Calculate Iterated socket modifier.
		float CurrentModifier = CalculateTargetModifier(SocketLocation, CapturedHC, PlayerInput);

		if (CurrentModifier < BestSocket.Value)
		{
			//Assign iterated socket to new Best Socket.
			BestSocket.Key = Socket;
			BestSocket.Value = CurrentModifier;
		}

#if WITH_EDITORONLY_DATA
		if (bDisplayModifier)
		{
			DrawDebugString(GetWorld(), CapturedHC->GetSocketLocation(Socket), FString::Printf(TEXT("Modifier %.1f\n"), CurrentModifier), nullptr, ModifierColor, ModifierDuration, false, 1.2f);
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
	if (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(LOSDelayHandler) && LostTargetDelay >= 0.f)
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

	//SCENE QUERY PARAMS for debug. Tag = WeaponTrace. In console print TraceTag<WeaponTrace> or TraceTagAll
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
	GetLockOn()->ClearTarget();

	if (AutoFindTargetFlags & static_cast<uint8>(UnlockReason))
	{
		GetLockOn()->EnableTargeting();
	}

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
