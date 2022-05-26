// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetComponent.h"
#include "TargetingHelperComponent.h"
#include "LockOnSubobjects/RotationModes/RotationModeBase.h"
#include "LockOnSubobjects/TargetHandlers/TargetHandlerBase.h"

#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ULockOnTargetComponent::ULockOnTargetComponent()
	: bCanCaptureTarget(true)
	, bDisableTickWhileUnlocked(true)
	, InputBufferThreshold(.1f)
	, BufferResetFrequency(0.15f)
	, ClampInputVector(-2.f, 2.f)
	, SwitchDelay(0.3f)
	, bFreezeInputAfterSwitch(true)
	, UnfreezeThreshold(1e-2f)
	, bIsTargetLocked(false)
	, TargetingDuration(0.f)
	, InputBuffer(0.f)
	, InputVector(0.f)
	, bInputFrozen(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	SetIsReplicatedByDefault(true);
}

void ULockOnTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bDisableTickWhileUnlocked)
	{
		SetComponentTickEnabled(true);
	}
}

void ULockOnTargetComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);

	ClearTargetNative();

	if(GetWorld())
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	}
}

void ULockOnTargetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, Rep_TargetInfo, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ThisClass, TargetingDuration, COND_SkipOwner);
}

/*******************************************************************************************/
/*******************************  Player Input  ********************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::EnableTargeting()
{
	if (IsTargetLocked())
	{
		ClearTargetManual();
	}
	else
	{
		if (bCanCaptureTarget)
		{
			FindTarget();
		}
	}
}

void ULockOnTargetComponent::SwitchTargetYaw(float YawAxis)
{
	if (bCanCaptureTarget && IsTargetLocked() && GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(SwitchDelayHandler))
	{
		InputVector.X = YawAxis;
	}
}

void ULockOnTargetComponent::SwitchTargetPitch(float PitchAxis)
{
	if (bCanCaptureTarget && IsTargetLocked() && GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(SwitchDelayHandler))
	{
		InputVector.Y = PitchAxis;
	}
}

void ULockOnTargetComponent::ProcessAnalogInput()
{
	if (!GetWorld())
	{
		return;
	}

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();

	if (TimerManager.IsTimerActive(SwitchDelayHandler))
	{
		return;
	}

	if (!CanInputBeProcessed(InputVector.Size()))
	{
		return;
	}

	InputBuffer += InputVector.ClampAxes(ClampInputVector.X, ClampInputVector.Y) * GetWorld()->GetDeltaSeconds();

	if (InputBuffer.Size() > GetInputBufferThreshold())
	{
		SwitchTarget(InputBuffer);

		bInputFrozen = true;
		ClearInputBuffer();

		if (SwitchDelay > 0.f)
		{
			//Timer without a callback, 'cause we only need the fact that the timer is in progress.
			TimerManager.SetTimer(SwitchDelayHandler, SwitchDelay, false);
		}
	}
	else
	{
		if (!TimerManager.IsTimerActive(BufferResetHandler))
		{
			FTimerDelegate ClearInputBufferCallback;
			ClearInputBufferCallback.BindUObject(this, &ULockOnTargetComponent::ClearInputBuffer);
			TimerManager.SetTimer(BufferResetHandler, ClearInputBufferCallback, BufferResetFrequency, false);
		}
	}
}

bool ULockOnTargetComponent::CanInputBeProcessed(float PlayerInput) const
{
	if (bFreezeInputAfterSwitch)
	{
		bool bInputIsEmpty = FMath::IsNearlyZero(PlayerInput, UnfreezeThreshold);

		if (bInputFrozen)
		{
			bInputFrozen = !bInputIsEmpty;
		}

		return !bInputFrozen && !bInputIsEmpty;
	}

	return true;
}

float ULockOnTargetComponent::GetInputBufferThreshold_Implementation() const
{
	return InputBufferThreshold;
}

void ULockOnTargetComponent::ClearInputBuffer()
{
	InputBuffer = {0.f, 0.f};
}

/*******************************************************************************************/
/******************************** Target Handling ******************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::FindTarget()
{
	if (IsValid(TargetHandlerImplementation))
	{
#if LOC_INSIGHTS
		TRACE_BOOKMARK(TEXT("LOT_PerformFindTarget"));
		SCOPED_NAMED_EVENT(LOT_TargetFinding, FColor::Red);
#endif
		
		FTargetInfo NewTargetInfo = TargetHandlerImplementation->FindTarget();

		if (IsValid(NewTargetInfo.HelperComponent))
		{
			UpdateTargetInfo(NewTargetInfo);
		}
		else
		{
			OnTargetNotFound.Broadcast();
		}
	}
}

bool ULockOnTargetComponent::SwitchTarget(FVector2D PlayerInput)
{
	if (IsValid(TargetHandlerImplementation))
	{
#if LOC_INSIGHTS
		TRACE_BOOKMARK(TEXT("LOT_PerformSwitchTarget"));
		SCOPED_NAMED_EVENT(LOT_SwitchingTarget, FColor::Red);
#endif

		FTargetInfo NewTargetInfo;

		if (TargetHandlerImplementation->SwitchTarget(NewTargetInfo, PlayerInput) && IsValid(NewTargetInfo.HelperComponent))
		{
			UpdateTargetInfo(NewTargetInfo);
			return true;
		}
		else
		{
			OnTargetNotFound.Broadcast();
		}
	}

	return false;
}

bool ULockOnTargetComponent::CanContinueTargeting() const
{
	if (GetOwner())
	{
		if(IsOwnerLocallyControlled())
		{
			//Only the locally controlled component checks all conditions.
			return IsValid(TargetHandlerImplementation) && TargetHandlerImplementation->CanContinueTargeting();
		}
		else
		{
			//Server and SimulatedProxies just check the pointer's validity to avoid crashes.
			return IsValid(GetTarget());
		}
	}

	return false;
}

/*******************************************************************************************/
/*******************************  Manual Handling  *****************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::SetLockOnTargetManual(AActor* NewTarget, const FName& Socket)
{
	if (bCanCaptureTarget)
	{
		if (IsValid(NewTarget) && NewTarget != GetOwner())
		{
			UTargetingHelperComponent* NewTargetHelperComponent = NewTarget->FindComponentByClass<UTargetingHelperComponent>();

			if (IsValid(NewTargetHelperComponent) && NewTargetHelperComponent->CanBeTargeted(this))
			{
				UpdateTargetInfo({NewTargetHelperComponent, Socket});
			}
		}
	}
}

bool ULockOnTargetComponent::SwitchTargetManual(FVector2D PlayerInput)
{
	return bCanCaptureTarget && IsTargetLocked() && SwitchTarget(PlayerInput);
}

void ULockOnTargetComponent::ClearTargetManual(bool bAutoFindNewTarget)
{
	if (IsTargetLocked())
	{
		UpdateTargetInfo({nullptr, NAME_None});

		if(bAutoFindNewTarget)
		{
			EnableTargeting();
		}
	}
}

/*******************************************************************************************/
/*******************************  Directly Lock On System  *********************************/
/*******************************************************************************************/

void ULockOnTargetComponent::UpdateTargetInfo(const FTargetInfo& TargetInfo)
{
	//Update the Target locally.
	Rep_TargetInfo = TargetInfo;
	OnTargetInfoUpdated();

	//Update the Target on the server.
	if(GetOwner()->GetNetMode() != NM_Standalone)
	{
		Server_UpdateTargetInfo(TargetInfo);
	}
}

void ULockOnTargetComponent::Server_UpdateTargetInfo_Implementation(const FTargetInfo& TargetInfo)
{
	if (TargetInfo.GetActor() != GetOwner())
	{
		Rep_TargetInfo = TargetInfo;
		OnTargetInfoUpdated();
	}
}

void ULockOnTargetComponent::OnTargetInfoUpdated()
{
	if (IsValid(Rep_TargetInfo.HelperComponent))
	{
		if (IsTargetLocked())
		{
			if (Rep_TargetInfo.HelperComponent == GetHelperComponent())
			{
				//If locked and received the same HelperComponent then change the socket.
				if (Rep_TargetInfo.SocketForCapturing != GetCapturedSocket())
				{
					UpdateTargetSocket();
				}
			}
			else
			{
				//If locked and received another HelperComponent then switch to the new Target.
				ClearTargetNative();
				SetLockOnTargetNative();
			}
		}
		else
		{
			//If not locked then capture the new Target.
			SetLockOnTargetNative();
		}
	}
	else
	{
		//If HelperComponent is invalid clear the Target.
		ClearTargetNative();
	}
}

void ULockOnTargetComponent::SetLockOnTargetNative()
{
#if LOC_INSIGHTS
	SCOPED_NAMED_EVENT(LOT_SetLock, FColor::Emerald);
	TRACE_BOOKMARK(TEXT("Target Locked: %s"), *GetNameSafe(Rep_TargetInfo.GetActor()));
#endif

	PrivateTargetInfo = Rep_TargetInfo;
	bIsTargetLocked = true;
	
	//Notify TargetingHelperComponent.
	GetHelperComponent()->CaptureTarget(this, GetCapturedSocket());
	
	//Notify TargetHadler that the Target is captured.
	TargetHandlerImplementation->OnTargetLockedNative();
	OnTargetLocked.Broadcast(GetTarget());

	if (bDisableTickWhileUnlocked)
	{
		SetComponentTickEnabled(true);
	}
}

void ULockOnTargetComponent::ClearTargetNative()
{
	if (IsTargetLocked())
	{
#if LOC_INSIGHTS
		TRACE_BOOKMARK(TEXT("Target unlocked: %s"), *GetNameSafe(GetTarget()));
		SCOPED_NAMED_EVENT(LOT_ClearTarget, FColor::Silver);
#endif

		if (bDisableTickWhileUnlocked)
		{
			SetComponentTickEnabled(false);
		}

		//Notify TargetingHelperComponent.
		if (IsValid(GetHelperComponent()))
		{
			GetHelperComponent()->ReleaseTarget(this);
		}

		TargetingDuration = 0.f;
		bIsTargetLocked = false;
		PrivateTargetInfo.Reset();

		//Notify TargetHandler that the Target is released.
		TargetHandlerImplementation->OnTargetUnlockedNative();
		OnTargetUnlocked.Broadcast(GetTarget());
	}
}

void ULockOnTargetComponent::UpdateTargetSocket()
{
#if LOC_INSIGHTS
	TRACE_BOOKMARK(TEXT("LOT_SocketChanged %s"), *Rep_TargetInfo.SocketForCapturing.ToString());
	SCOPED_NAMED_EVENT(LOT_SocketChanged, FColor::Yellow);
#endif

	PrivateTargetInfo.SocketForCapturing = Rep_TargetInfo.SocketForCapturing;
	GetHelperComponent()->CaptureTarget(this, GetCapturedSocket());
}

/*******************************************************************************************/
/*******************************  Tick and During Target Lock features  ********************/
/*******************************************************************************************/

void ULockOnTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if LOC_INSIGHTS
	SCOPED_NAMED_EVENT(LOT_Tick, FColor::Orange);
#endif

	if (IsTargetLocked() && CanContinueTargeting())
	{
		if (GetOwner()->GetLocalRole() > ROLE_SimulatedProxy /*GetOwner()->HasAuthority() || IsOwnerLocallyControlled()*/)
		{
			TargetingDuration += DeltaTime;
		}

		const FVector FocusLocation = GetCapturedLocation(true);

		if (IsOwnerLocallyControlled())
		{
			//Process the input only for the locally controlled Owner.
			ProcessAnalogInput();

			//Update the ControlRotation locally and it'll be replicated to the server automatically.
			TickControlRotationCalc(DeltaTime, FocusLocation);

#if WITH_EDITORONLY_DATA
			DebugOnTick();
#endif
		}

		//Update on the Client and the Server simultaneously.
		//TODO: server check as in CMC.
		TickOwnerRotationCalc(DeltaTime, FocusLocation);
	}
}

void ULockOnTargetComponent::TickControlRotationCalc(float DeltaTime, const FVector& TargetLocation)
{
	if (IsValid(ControlRotationModeConfig) && ControlRotationModeConfig->bIsEnabled)
	{
		AController* const Controller = GetController();
		const FRotator& ControlRotation = Controller->GetControlRotation();
		const FVector LocationFrom = GetCameraLocation();
		const FRotator NewRotation = ControlRotationModeConfig->GetRotation(ControlRotation, LocationFrom, TargetLocation, DeltaTime);
		Controller->SetControlRotation(NewRotation);
	}
}

void ULockOnTargetComponent::TickOwnerRotationCalc(float DeltaTime, const FVector& TargetLocation)
{
	if (IsValid(OwnerRotationModeConfig) && OwnerRotationModeConfig->bIsEnabled && GetOwner())
	{
		const FVector LocationFrom = GetOwnerLocation();
		const FRotator& CurrentRotation = GetOwnerRotation();
		const FRotator NewRotation = OwnerRotationModeConfig->GetRotation(CurrentRotation, LocationFrom, TargetLocation, DeltaTime);
		GetOwner()->SetActorRotation(NewRotation);
	}
}

/*******************************************************************************************/
/*******************************  Helpers Methods  *****************************************/
/*******************************************************************************************/

AActor* ULockOnTargetComponent::GetTarget() const
{
	return GetHelperComponent() ? GetHelperComponent()->GetOwner() : nullptr;
}

FVector ULockOnTargetComponent::GetCapturedLocation(bool bWithOffset) const
{
	FVector TargetedLocation(0.f);

	if (IsTargetLocked())
	{
		TargetedLocation = GetHelperComponent()->GetSocketLocation(GetCapturedSocket(), bWithOffset, this);
	}

	return TargetedLocation;
}

AController* ULockOnTargetComponent::GetController() const
{
	return GetOwner() ? GetOwner()->GetInstigatorController() : nullptr;
}

bool ULockOnTargetComponent::IsOwnerLocallyControlled() const
{
	AController* Controller = GetController();

	return IsValid(Controller) && Controller->IsLocalController();
}

APlayerController* ULockOnTargetComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetController());
}

FRotator ULockOnTargetComponent::GetRotationToTarget() const
{
	return FRotationMatrix::MakeFromX(GetCapturedLocation() - GetCameraLocation()).Rotator();
}

FRotator ULockOnTargetComponent::GetOwnerRotation() const
{
	return GetOwner() ? GetOwner()->GetActorRotation() : FRotator();
}

FVector ULockOnTargetComponent::GetOwnerLocation() const
{
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector();
}

FRotator ULockOnTargetComponent::GetCameraRotation() const
{
	return GetPlayerController() ? GetPlayerController()->PlayerCameraManager->GetCameraRotation() : GetOwnerRotation();
}

FVector ULockOnTargetComponent::GetCameraLocation() const
{
	return GetPlayerController() ? GetPlayerController()->PlayerCameraManager->GetCameraLocation() : GetOwnerLocation();
}

FVector ULockOnTargetComponent::GetCameraUpVector() const
{
	return FRotationMatrix(GetCameraRotation()).GetUnitAxis(EAxis::Z);
}

FVector ULockOnTargetComponent::GetCameraRightVector() const
{
	return FRotationMatrix(GetCameraRotation()).GetUnitAxis(EAxis::Y);
}

FVector ULockOnTargetComponent::GetCameraForwardVector() const
{
	return GetCameraRotation().Vector();
}

void ULockOnTargetComponent::SetTargetHandler(UTargetHandlerBase* NewTargetHandler)
{
	if(IsValid(NewTargetHandler))
	{
		TargetHandlerImplementation = NewTargetHandler;

		if(IsTargetLocked())
		{
			TargetHandlerImplementation->OnTargetLockedNative();
		}
	}
}

void ULockOnTargetComponent::SetTickWhileUnlockedEnabled(bool bTickWhileUnlocked)
{
	bDisableTickWhileUnlocked = bTickWhileUnlocked;

	if (bDisableTickWhileUnlocked && !IsTargetLocked())
	{
		SetComponentTickEnabled(false);
	}
}

/*******************************************************************************************/
/*******************************  Debug  ***************************************************/
/*******************************************************************************************/

#if WITH_EDITORONLY_DATA

void ULockOnTargetComponent::DebugOnTick() const
{
	if (bShowTargetInfo)
	{
		TSet<ULockOnTargetComponent*> Invaders = GetHelperComponent()->GetInvaders();
		FString InvadersName;

		for (ULockOnTargetComponent* Invader : Invaders)
		{
			if (Invader)
			{
				InvadersName.Append(FString::Printf(TEXT("%s, "), *GetNameSafe(Invader->GetOwner())));
			}
		}

		GEngine->AddOnScreenDebugMessage(-1, 0.f, DebugInfoColor, FString::Printf(TEXT("\nTarget Invaders amount: %i \nInvaders: [%s]"), GetHelperComponent()->GetInvaders().Num(), *InvadersName), true, FVector2D(1.1f));
		GEngine->AddOnScreenDebugMessage(-1, 0.f, DebugInfoColor, FString::Printf(TEXT("\nControlRotationMode: %s \nOwnerRotationMode: %s"), *GetNameSafe(ControlRotationModeConfig), *GetNameSafe(OwnerRotationModeConfig)), true, FVector2D(1.1f));

		if (bFreezeInputAfterSwitch)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.f, DebugInfoColor, FString::Printf(TEXT("Is Input Frozen: %s "), bInputFrozen ? TEXT("true") : TEXT("false")), true, FVector2D(1.1f));
		}

		GEngine->AddOnScreenDebugMessage(-1, 0.f, DebugInfoColor, FString::Printf(TEXT("Input Buffer ratio = %.3f / %.3f InputVector2D: X %.3f, Y %.3f "), InputBuffer.Size(), GetInputBufferThreshold(), InputBuffer.X, InputBuffer.Y), true, FVector2D(1.1f));
		GEngine->AddOnScreenDebugMessage(-1, 0.f, DebugInfoColor, FString::Printf(TEXT("\n\nTarget: %s \nCapturedSocket: %s \nSockets Amount: %i\nDuration: %.2f "), *GetNameSafe(GetTarget()), *PrivateTargetInfo.SocketForCapturing.ToString(), GetHelperComponent()->GetSockets().Num(), GetTargetingDuration()), true, FVector2D(1.15f));
	}
}

#endif
