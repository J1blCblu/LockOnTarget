// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "TargetHandlers/WeightedTargetHandler.h"
#include "LockOnTargetComponent.h"
#include "TargetComponent.h"
#include "TargetManager.h"
#include "LockOnTargetDefines.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"

UWeightedTargetHandler::UWeightedTargetHandler()
	: AutoFindTargetFlags(0b00011111)
	, DistanceWeight(0.725f)
	, DeltaAngleWeight(0.275f)
	, PlayerInputWeight(0.1f)
	, TargetPriorityWeight(0.25f)
	, PureDefaultWeight(1000.f)
	, DistanceMaxFactor(2420.f)
	, DeltaAngleMaxFactor(45.f)
	, MinimumFactorThreshold(0.035f)
	, bDistanceCheck(true)
	, DefaultCaptureRadius(2200.f)
	, LostRadiusScale(1.1f)
	, NearClipRadius(150.f)
	, CaptureRadiusScale(1.f)
	, ViewConeAngle(42.f)
	, ViewPitchOffset(10.f)
	, ViewYawOffset(0.f)
	, bScreenCapture(false)
	, ScreenOffset(5.f, 2.5f)
	, bRecentRenderCheck(true)
	, RecentTolerance(0.1f)
	, PlayerInputAngularRange(60.f)
	, bLineOfSightCheck(true)
	, TraceCollisionChannel(ECollisionChannel::ECC_Visibility)
	, LostTargetDelay(3.f)
	, CheckInterval(0.2f)
	, LineOfSightCheckTimer(0.f)
{
	ExtensionTick.bCanEverTick = false;
}

FFindTargetRequestResponse UWeightedTargetHandler::FindTarget_Implementation(const FFindTargetRequestParams& RequestParams)
{
	const EFindTargetContextMode ContextMode = GetLockOnTargetComponent()->IsTargetLocked() ? EFindTargetContextMode::Switch : EFindTargetContextMode::Find;
	FFindTargetContext Context = CreateFindTargetContext(ContextMode, RequestParams);
	return FindTargetBatched(Context);
}

void UWeightedTargetHandler::CheckTargetState_Implementation(const FTargetInfo& Target, float DeltaTime)
{
	LOT_SCOPED_EVENT(WTH_CheckTargetState);

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPointOfView(ViewLocation, ViewRotation);

	const AActor* const TargetActor = Target->GetOwner();

	if (bDistanceCheck)
	{
		const float DistanceSq = (TargetActor->GetActorLocation() - ViewLocation).SizeSquared();
		const float TargetLostRadius = GetTargetCaptureRadius(Target.TargetComponent) * LostRadiusScale;

		if (DistanceSq > FMath::Square(TargetLostRadius))
		{
			HandleTargetUnlock(ETargetUnlockReason::DistanceFailure);
			return;
		}
	}

	if (bLineOfSightCheck && LostTargetDelay > 0.f)
	{
		LineOfSightCheckTimer += DeltaTime;

		if (LineOfSightCheckTimer > CheckInterval)
		{
			LineOfSightCheckTimer = 0.f;

			//@TODO: Maybe use AsyncLineTrace.
			if (LineOfSightTrace(ViewLocation, Target->GetSocketLocation(Target.Socket), TargetActor))
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

void UWeightedTargetHandler::HandleTargetException_Implementation(const FTargetInfo& Target, ETargetExceptionType Exception)
{
	HandleTargetUnlock(ConvertTargetExceptionToUnlockReason(Exception));
}

void UWeightedTargetHandler::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);
	StopLineOfSightTimer();
	LineOfSightCheckTimer = 0.f;
}

/*******************************************************************************************/
/******************************* Batched Finding *******************************************/
/*******************************************************************************************/

FFindTargetRequestResponse UWeightedTargetHandler::FindTargetBatched(FFindTargetContext& Context)
{
	LOT_SCOPED_EVENT(WTH_BatchedFinding);

	FFindTargetRequestResponse OutResponse;
	TArray<FTargetContext> TargetsData;

	{
		LOT_SCOPED_EVENT(WTH_Pass_PrimarySampling);
		PerformPrimarySamplingPass(Context, /*out*/TargetsData);
	}

	if (TargetsData.Num() > 0)
	{
		{
			LOT_SCOPED_EVENT(WTH_Pass_Solver);
			PerformSolverPass(Context, /*inout*/TargetsData);
		}

		{
			LOT_SCOPED_EVENT(WTH_Pass_Sort);
			TargetsData.Sort([](const auto& lhs, const auto& rhs)
				{
					return lhs.Weight < rhs.Weight;
				});
		}

		{
			LOT_SCOPED_EVENT(WTH_Pass_SecondarySampling);
			OutResponse = PerformSecondarySamplingPass(Context, /*in*/TargetsData);
		}
	}

	return OutResponse;
}

void UWeightedTargetHandler::PerformPrimarySamplingPass(FFindTargetContext& Context, TArray<FTargetContext>& OutTargetsData)
{
	const TSet<UTargetComponent*>& RegisteredTargets = UTargetManager::Get(*GetWorld()).GetRegisteredTargets();

	OutTargetsData.Empty();
	OutTargetsData.Reserve(RegisteredTargets.Num());

	for (UTargetComponent* const Target : RegisteredTargets)
	{
		if (ShouldSkipTargetPrimaryPass(Context, Target))
		{
			continue;
		}

		for (const FName TargetSocket : Target->GetSockets())
		{
			const FTargetInfo CurrentTarget = { Target, TargetSocket };

			//Skip already captured Target and Socket.
			if (Context.CapturedTarget.Target == CurrentTarget)
			{
				continue;
			}

			FTargetContext TargetContext = CreateTargetContext(Context, CurrentTarget);

			//Check if in view cone.
			const float DeltaConeAngle = FMath::RadiansToDegrees(FMath::Acos(Context.ViewRotationMatrix.GetScaledAxis(EAxis::X) | TargetContext.Direction));

			if (DeltaConeAngle > ViewConeAngle)
			{
				continue;
			}

			//Check if in input range.
			if (Context.Mode == EFindTargetContextMode::Switch)
			{
				CalcDeltaAngle2D(Context, TargetContext);

				if (TargetContext.DeltaAngle2D > PlayerInputAngularRange)
				{
					continue;
				}
			}

			OutTargetsData.Add(TargetContext);
		}
	}
}

bool UWeightedTargetHandler::ShouldSkipTargetPrimaryPass(const FFindTargetContext& Context, const UTargetComponent* Target) const
{
	if (!IsTargetValid(Target))
	{
		return true;
	}

	const AActor* const TargetActor = Target->GetOwner();

	if (bRecentRenderCheck && !TargetActor->WasRecentlyRendered(RecentTolerance))
	{
		return true;
	}

	if (bDistanceCheck)
	{
		//It'd be more correct to check the distance per socket, but this is faster.
		const float DistanceSq = (Context.ViewLocation - TargetActor->GetActorLocation()).SizeSquared();
		const float TargetCaptureRadius = GetTargetCaptureRadius(Target);

		if (DistanceSq > FMath::Square(TargetCaptureRadius) || DistanceSq < FMath::Square(NearClipRadius))
		{
			return true;
		}
	}

	return false;
}

void UWeightedTargetHandler::PerformSolverPass(FFindTargetContext& Context, TArray<FTargetContext>& InOutTargetsData)
{
	for (FTargetContext& TargetContext : InOutTargetsData)
	{
		TargetContext.Weight = CalculateTargetWeight(Context, TargetContext);
	}
}

float UWeightedTargetHandler::CalculateTargetWeight_Implementation(const FFindTargetContext& Context, const FTargetContext& TargetContext) const
{
	float OutWeight = 0.f;

	const float WeightSum = DistanceWeight + DeltaAngleWeight + PlayerInputWeight + TargetPriorityWeight;

	if (!FMath::IsNearlyZero(WeightSum))
	{
		const float WeightNormalizer = 1.f / WeightSum;

		auto ApplyFactor = [&OutWeight, WeightNormalizer, this](float Weight, float Ratio)
			{
				const float Factor = FMath::Clamp(Ratio, MinimumFactorThreshold, 1.f);
				OutWeight += PureDefaultWeight * Weight * WeightNormalizer * Factor;
			};

		if (DistanceWeight > UE_KINDA_SMALL_NUMBER)
		{
			const float Ratio = TargetContext.DistanceSq / FMath::Square(DistanceMaxFactor);
			ApplyFactor(DistanceWeight, Ratio);
		}

		if (DeltaAngleWeight > UE_KINDA_SMALL_NUMBER)
		{
			const float Ratio = FMath::RadiansToDegrees(FMath::Acos(TargetContext.Direction | Context.SolverViewDirection)) / DeltaAngleMaxFactor;
			ApplyFactor(DeltaAngleWeight, Ratio);
		}

		if (Context.Mode == EFindTargetContextMode::Switch && PlayerInputWeight > UE_KINDA_SMALL_NUMBER)
		{
			const float Ratio = TargetContext.DeltaAngle2D / PlayerInputAngularRange;
			ApplyFactor(PlayerInputWeight, Ratio);
		}

		if (TargetPriorityWeight > UE_KINDA_SMALL_NUMBER)
		{
			ApplyFactor(TargetPriorityWeight, TargetContext.Target->Priority);
		}
	}

	return OutWeight;
}

FFindTargetRequestResponse UWeightedTargetHandler::PerformSecondarySamplingPass(FFindTargetContext& Context, TArray<FTargetContext>& InTargetsData)
{
	FFindTargetRequestResponse OutResponse;

	if (Context.RequestParams.bGenerateDetailedResponse)
	{
		InTargetsData.RemoveAll([this, &Context](const FTargetContext& TargetContext)
			{
				return ShouldSkipTargetSecondaryPass(Context, TargetContext);
			});

		if (InTargetsData.Num() > 0)
		{
			OutResponse.Target = InTargetsData[0].Target;
		}

		OutResponse.Payload = GenerateDetailedResponse(Context, InTargetsData);
	}
	else
	{
		const auto* const FoundTargetContext = InTargetsData.FindByPredicate([this, &Context](const FTargetContext& TargetContext)
			{
				return !ShouldSkipTargetSecondaryPass(Context, TargetContext);
			});

		if (FoundTargetContext)
		{
			OutResponse.Target = FoundTargetContext->Target;
		}
	}

	return OutResponse;
}

bool UWeightedTargetHandler::ShouldSkipTargetSecondaryPass(const FFindTargetContext& Context, const FTargetContext& TargetContext) const
{
	if (ShouldSkipTargetCustom(Context, TargetContext))
	{
		return true;
	}

	if (bScreenCapture && !IsTargetOnScreen(Context, TargetContext))
	{
		return true;
	}

	if (bLineOfSightCheck && !LineOfSightTrace(Context.ViewLocation, TargetContext.Location, TargetContext.Target->GetOwner()))
	{
		return true;
	}

	return false;
}

UWeightedTargetHandlerDetailedResponse* UWeightedTargetHandler::GenerateDetailedResponse(const FFindTargetContext& Context, TArray<FTargetContext>& InTargetsData)
{
	auto* const Response = NewObject<UWeightedTargetHandlerDetailedResponse>(this, FName(TEXT("WeightedTargetHandler_DetailedResponse")), RF_StrongRefOnFrame | RF_Transient);

	if (Response)
	{
		Response->Context = Context;
		Response->TargetsData = MoveTemp(InTargetsData);
	}

	return Response;
}

/*******************************************************************************************/
/*********************************** Helpers ***********************************************/
/*******************************************************************************************/

void UWeightedTargetHandler::HandleTargetUnlock(ETargetUnlockReason UnlockReason)
{
	LOT_SCOPED_EVENT(WTH_HandleTargetUnlock);

	if (IsAnyUnlockReasonSet(AutoFindTargetFlags, UnlockReason))
	{
		TryFindTarget(true);
	}
	else
	{
		GetLockOnTargetComponent()->ClearTargetManual();
	}
}

void UWeightedTargetHandler::TryFindTarget(bool bClearTargetIfFailed)
{
	if (GetLockOnTargetComponent()->CanCaptureTarget())
	{
		const FFindTargetRequestParams RequestParams;
		FFindTargetContext Context = CreateFindTargetContext(EFindTargetContextMode::Find, RequestParams);
		const FFindTargetRequestResponse Response = FindTargetBatched(Context);

		if (Context.Instigator->CanTargetBeCaptured(Response.Target))
		{
			Context.Instigator->SetLockOnTargetManualByInfo(Response.Target);
		}
		else if (bClearTargetIfFailed)
		{
			Context.Instigator->ClearTargetManual();
		}
	}
}

FFindTargetContext UWeightedTargetHandler::CreateFindTargetContext(EFindTargetContextMode Mode, const FFindTargetRequestParams& RequestParams)
{
	LOT_SCOPED_EVENT(WTH_CreateFindTargetContext);

	FFindTargetContext Context;
	Context.Mode = Mode;
	Context.RequestParams = RequestParams;
	Context.PlayerInputDirection = RequestParams.PlayerInput.GetSafeNormal();
	Context.TargetHandler = this;
	Context.Instigator = GetLockOnTargetComponent();
	Context.InstigatorPawn = GetInstigatorPawn();
	Context.PlayerController = GetPlayerController();

	GetPointOfView(Context.ViewLocation, Context.ViewRotation);
	Context.ViewRotationMatrix = FRotationMatrix::Make(Context.ViewRotation);

	if (Context.Instigator->IsTargetLocked())
	{
		Context.CapturedTarget = CreateTargetContext(Context, { Context.Instigator->GetTargetComponent(), Context.Instigator->GetCapturedSocket() });
	}

	if (Mode == EFindTargetContextMode::Find)
	{
		Context.SolverViewDirection = (Context.ViewRotation.Quaternion() * FRotator(ViewPitchOffset, ViewYawOffset, 0.f).Quaternion()).GetAxisX();
	}
	else
	{
		Context.SolverViewDirection = Context.CapturedTarget.Direction;
	}

	return Context;
}

FTargetContext UWeightedTargetHandler::CreateTargetContext(const FFindTargetContext& Context, const FTargetInfo& InTarget)
{
	check(InTarget.TargetComponent);

	FTargetContext TargetContext{ InTarget };
	TargetContext.Location = InTarget->GetSocketLocation(InTarget.Socket);
	const FVector Delta = TargetContext.Location - Context.ViewLocation;
	TargetContext.DistanceSq = Delta.SizeSquared();

	if (TargetContext.DistanceSq > UE_KINDA_SMALL_NUMBER)
	{
		TargetContext.Direction = Delta * FMath::InvSqrt(TargetContext.DistanceSq);
	}

	return TargetContext;
}

void UWeightedTargetHandler::CalcDeltaAngle2D(const FFindTargetContext& Context, FTargetContext& OutTargetContext) const
{
	const FVector Point = FMath::LinePlaneIntersection(Context.ViewLocation, OutTargetContext.Location, Context.CapturedTarget.Location, Context.ViewRotationMatrix.GetScaledAxis(EAxis::X));
	const FVector Delta = Point - Context.CapturedTarget.Location;
	const float DeltaX = Context.ViewRotationMatrix.GetScaledAxis(EAxis::Y) | Delta;
	const float DeltaY = Context.ViewRotationMatrix.GetScaledAxis(EAxis::Z) | Delta;
	OutTargetContext.DeltaDirection2D = FVector2D(DeltaX, -DeltaY).GetSafeNormal();
	OutTargetContext.DeltaAngle2D = FMath::RadiansToDegrees(FMath::Acos(OutTargetContext.DeltaDirection2D | Context.PlayerInputDirection));
}

float UWeightedTargetHandler::GetTargetCaptureRadius(const UTargetComponent* InTarget) const
{
	check(InTarget);
	return CaptureRadiusScale * (InTarget->bForceCustomCaptureRadius ? InTarget->CustomCaptureRadius : DefaultCaptureRadius);
}

bool UWeightedTargetHandler::IsTargetOnScreen(const FFindTargetContext& Context, const FTargetContext& TargetContext) const
{
	//True for non-player controlled owners.
	bool bResult = true;

	if (Context.PlayerController)
	{
		FVector2D ScreenPosition;

		if (Context.PlayerController->ProjectWorldLocationToScreen(TargetContext.Location, ScreenPosition, true))
		{
			const FVector2D ScreenSize = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(Context.PlayerController).GetAbsoluteSize();

			//Percent to ratio.
			const FVector2D OffsetRatio = ScreenOffset / 100.f;
			const float XOffset = ScreenSize.X * OffsetRatio.X;
			const float YOffset = ScreenSize.Y * OffsetRatio.Y;

			bResult = ScreenPosition.X > XOffset				//Left
				&& ScreenPosition.X < (ScreenSize.X - XOffset)	//Right
				&& ScreenPosition.Y > YOffset					//Top
				&& ScreenPosition.Y < (ScreenSize.Y - YOffset);	//Bottom
		}
	}

	return bResult;
}

void UWeightedTargetHandler::GetPointOfView_Implementation(FVector& OutLocation, FRotator& OutRotation) const
{
	if (const AActor* const OwnerActor = GetLockOnTargetComponent()->GetOwner())
	{
		OwnerActor->GetActorEyesViewPoint(OutLocation, OutRotation);

		if (const AController* const Controller = GetInstigatorController())
		{
			Controller->GetPlayerViewPoint(OutLocation, OutRotation);
		}
	}
}

/*******************************************************************************************/
/*******************************  Line Of Sight  *******************************************/
/*******************************************************************************************/

void UWeightedTargetHandler::StartLineOfSightTimer()
{
	const UWorld* const World = GetWorld();
	if (World && !World->GetTimerManager().IsTimerActive(LineOfSightExpirationHandle))
	{
		World->GetTimerManager().SetTimer(LineOfSightExpirationHandle, FTimerDelegate::CreateUObject(this, &UWeightedTargetHandler::OnLineOfSightTimerExpired), LostTargetDelay, false);
	}
}

void UWeightedTargetHandler::StopLineOfSightTimer()
{
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LineOfSightExpirationHandle);
	}
}

void UWeightedTargetHandler::OnLineOfSightTimerExpired()
{
	HandleTargetUnlock(ETargetUnlockReason::LineOfSightFailure);
}

bool UWeightedTargetHandler::LineOfSightTrace(const FVector& From, const FVector& To, const AActor* const TargetToIgnore) const
{
	bool bOutSuccess = false;

	if (UWorld* const World = GetWorld())
	{
		FHitResult HitRes;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT_NAME_ONLY(LockOnTargetTrace));
		QueryParams.AddIgnoredActor(TargetToIgnore);
		QueryParams.AddIgnoredActor(GetLockOnTargetComponent()->GetOwner());
		bOutSuccess = !World->LineTraceSingleByChannel(HitRes, From, To, TraceCollisionChannel, QueryParams);
	}

	return bOutSuccess;
}
