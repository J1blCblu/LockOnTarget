// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetExtensions/ControllerRotationExtension.h"
#include "LockOnTargetComponent.h"
#include "TargetComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

static FVector VInterpCriticallyDamped(const FVector& Current, const FVector& Target, FVector& InOutVelocity, float DeltaTime, float InterpSpeed)
{
	/** https://mathproofs.blogspot.com/2013/07/critically-damped-spring-smoothing.html */
	const FVector Delta = InOutVelocity - (Current - Target) * (FMath::Square(InterpSpeed) * DeltaTime);
	InOutVelocity = Delta / FMath::Square(1.f + InterpSpeed * DeltaTime);
	return Current + InOutVelocity * DeltaTime;
}

UControllerRotationExtension::UControllerRotationExtension()
	: bBlockLookInput(true)
	, bUseLocationPrediction(true)
	, PredictionTime(0.083f)
	, MaxAngularDeviation(10.f)
	, bUseOscillationSmoothing(true)
	, OscillationDampingFactor(2.9f)
	, DeadZonePitchTolerance(12.f)
	, YawOffset(0.f)
	, YawClampRange(35.f)
	, PitchOffset(-10.f)
	, PitchClamp(-50.f, 30.f)
	, InterpolationSpeed(12.5f)
	, AngularSleepTolerance(4.75f)
	, InterpEasingRange(10.f)
	, InterpEasingExponent(1.25f)
	, MinInterpSpeed(0.65f)
	, SpringVelocity(0.f)
{
	ExtensionTick.TickGroup = TG_PostPhysics;
	ExtensionTick.bCanEverTick = true;
	ExtensionTick.bStartWithTickEnabled = false;
	ExtensionTick.bAllowTickOnDedicatedServer = false;
}

void UControllerRotationExtension::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);

	//We need to tick before the spring arm component so it can process our result without a 1 frame delay.
	if (auto* const SpringArmComponent = Instigator->GetOwner()->FindComponentByClass<USpringArmComponent>())
	{
		SpringArmComponent->PrimaryComponentTick.AddPrerequisite(this, ExtensionTick);
	}
}

void UControllerRotationExtension::Deinitialize(ULockOnTargetComponent* Instigator)
{
	Super::Deinitialize(Instigator);

	if (auto* const SpringArmComponent = Instigator->GetOwner()->FindComponentByClass<USpringArmComponent>())
	{
		SpringArmComponent->PrimaryComponentTick.RemovePrerequisite(this, ExtensionTick);
	}
}

void UControllerRotationExtension::OnTargetLocked(UTargetComponent* Target, FName Socket)
{
	Super::OnTargetLocked(Target, Socket);
	SetTickEnabled(true);

	if (bBlockLookInput)
	{
		if (AController* const Controller = GetInstigatorController())
		{
			Controller->SetIgnoreLookInput(true);
		}
	}
}

void UControllerRotationExtension::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);
	SetTickEnabled(false);
	ResetSpringInterpData();

	if (bBlockLookInput)
	{
		if (AController* const Controller = GetInstigatorController())
		{
			Controller->SetIgnoreLookInput(false);
		}
	}
}

void UControllerRotationExtension::OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket)
{
	Super::OnSocketChanged(CurrentTarget, NewSocket, OldSocket);
	ResetSpringInterpData();
}

void UControllerRotationExtension::ResetSpringInterpData()
{
	SpringLocation.Reset();
	SpringVelocity = FVector::ZeroVector;
}

void UControllerRotationExtension::Update(float DeltaTime)
{
	if (GetLockOnTargetComponent()->IsTargetLocked())
	{
		AController* const Controller = GetInstigatorController();
		if (Controller && Controller->IsLocalController())
		{
			const FRotator Rotation = CalcRotation(Controller, DeltaTime);
			Controller->SetControlRotation(Rotation);
		}
	}
}

FRotator UControllerRotationExtension::CalcRotation_Implementation(const AController* Controller, float DeltaTime)
{
	const FRotator CurrentRotation = Controller->GetControlRotation();
	const FVector InitialTargetLocation = GetTargetFocusLocation();
	FVector TargetLocation = InitialTargetLocation;

	//TargetLocation Adjustment
	{
		const AActor* const OwnerActor = GetLockOnTargetComponent()->GetOwner();
		const float Distance2D = (OwnerActor->GetActorLocation() - InitialTargetLocation).Size2D();
		const float CollisionRadius	= OwnerActor->GetSimpleCollisionRadius();

		//Correction
		{
			TargetLocation = GetCorrectedTargetLocation(InitialTargetLocation, Distance2D, DeltaTime);
			
			//Don't overshoot the owner's pivot.
			const float MaxOffsetLength = FMath::Max(Distance2D - CollisionRadius, 0.f);
			const FVector FinalOffset = TargetLocation - InitialTargetLocation;
			TargetLocation = InitialTargetLocation + FinalOffset.GetClampedToMaxSize2D(MaxOffsetLength);
		}

		//DeadZone
		{
			const FVector ToTarget = TargetLocation - OwnerActor->GetActorLocation();
			const float ToTargetPitch = FMath::Atan2(ToTarget.Z, ToTarget.Size2D());
			const float DeadZoneMaxPitch = FMath::DegreesToRadians(90.f - DeadZonePitchTolerance);
			
			if (Distance2D < CollisionRadius || FMath::Abs(ToTargetPitch) > DeadZoneMaxPitch)
			{
				//It's possible to use other features later on without skipping an update.
				return CurrentRotation;
			}
		}
	}

	const FVector ViewLocation = GetViewLocation(Controller);
	FRotator TargetRotation = GetTargetRotation(ViewLocation, TargetLocation, CurrentRotation);
	TargetRotation = InterpTargetRotation(TargetRotation, CurrentRotation, DeltaTime);

	return TargetRotation;
}

FVector UControllerRotationExtension::GetCorrectedTargetLocation(const FVector& TargetLocation, float Distance2D, float DeltaTime)
{
	const auto* const LockOnComponent = GetLockOnTargetComponent();
	const AActor* const TargetActor = LockOnComponent->GetTargetActor();

	FVector OutLocation = TargetLocation;

	if (bUseLocationPrediction && PredictionTime > 0.f)
	{
		//A very simple approximation, which is sufficient for the camera.
		const AActor* OwnerActor = LockOnComponent->GetOwner();
		FVector Velocity = TargetActor->GetVelocity() - OwnerActor->GetVelocity();
		Velocity *= FMath::Min(PredictionTime, 0.5f);
		const float MaxLength = Distance2D * FMath::Tan(FMath::DegreesToRadians(MaxAngularDeviation));
		OutLocation += Velocity.GetClampedToMaxSize(MaxLength);
	}

	if (bUseOscillationSmoothing)
	{
		//Smooth position in Actor's relative location to avoid 'jelly' movement in global space.
		const FVector TargetActorLocation = TargetActor->GetActorLocation();

		if (!SpringLocation.IsSet())
		{
			SpringLocation = TargetLocation - TargetActorLocation;
		}

		FVector RelativeLocation = OutLocation - TargetActorLocation;

		if (!RelativeLocation.Equals(SpringLocation.GetValue(), 1e-2))
		{
			RelativeLocation = VInterpCriticallyDamped(SpringLocation.GetValue(), RelativeLocation, SpringVelocity, DeltaTime, OscillationDampingFactor);
			OutLocation = TargetActorLocation + RelativeLocation;
			SpringLocation = RelativeLocation;
		}
	}

	return OutLocation;
}

FRotator UControllerRotationExtension::GetTargetRotation(const FVector& ViewLocation, const FVector& TargetLocation, const FRotator& CurrentRotation)
{
	FRotator TargetRotation = (TargetLocation - ViewLocation).ToOrientationRotator();

	//Apply offset
	{
		TargetRotation.Pitch += PitchOffset;
		TargetRotation.Yaw += YawOffset;
	}

	//Clamp axes
	{
		//Clamp the yaw in the owner's pivot.
		const FVector Pivot = GetLockOnTargetComponent()->GetOwner()->GetActorLocation();
		const FVector ToTarget = TargetLocation - Pivot;
		const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
		TargetRotation.Yaw = FMath::ClampAngle(TargetRotation.Yaw, TargetYaw - YawClampRange, TargetYaw + YawClampRange);

		float PitchMinClamped = PitchClamp.X;
		float PitchMaxClamped = PitchClamp.Y;
		if (CurrentRotation.Pitch > PitchClamp.Y)
		{
			PitchMaxClamped -= AngularSleepTolerance;
		}
		else if (CurrentRotation.Pitch < PitchClamp.X)
		{
			PitchMinClamped += AngularSleepTolerance;
		}
		TargetRotation.Pitch = FMath::ClampAngle(TargetRotation.Pitch, PitchMinClamped, PitchMaxClamped);
	}

	return TargetRotation;
}

FRotator UControllerRotationExtension::InterpTargetRotation(const FRotator& TargetRotation, const FRotator& CurrentRotation, float DeltaTime)
{
	FRotator OutRotation = CurrentRotation;
	const float Delta = FMath::RadiansToDegrees(FMath::Acos(TargetRotation.Vector() | CurrentRotation.Vector()));

	if (Delta > AngularSleepTolerance)
	{
		const float InterpEasingRangeSafe = FMath::Max(InterpEasingRange, 1.f);
		const float Alpha = FMath::Clamp((Delta - AngularSleepTolerance) / InterpEasingRangeSafe, 0.f, 1.f);
		const float ScaledInterpSpeed = FMath::InterpEaseIn(MinInterpSpeed, InterpolationSpeed, Alpha, InterpEasingExponent);
		OutRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, ScaledInterpSpeed);
		OutRotation.Roll = 0.f;
	}

	return OutRotation;
}

FVector UControllerRotationExtension::GetTargetFocusLocation_Implementation() const
{
	return GetLockOnTargetComponent()->GetCapturedFocusPointLocation();
}

FVector UControllerRotationExtension::GetViewLocation_Implementation(const AController* Controller) const
{
	FVector ViewLocation; FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	return ViewLocation;
}
