// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "LockOnSubobjects/RotationModes/RotationModeBase.h"
#include "LockOnSubobjects/TargetHandlers/TargetHandlerBase.h"
#include "TargetingHelperComponent.h"
#include "TimerManager.h"

ULockOnTargetComponent::ULockOnTargetComponent()
	: bCanCaptureTarget(true)
	, bDisableTickWhileUnlocked(true)
	, InputBufferThreshold(.1f)
	, BufferResetFrequency(0.15f)
	, ClampInputVector(-2.f, 2.f)
	, SwitchDelay(0.3f)
	, bFreezeInputAfterSwitch(true)
	, bIsTargetLocked(false)
	, TargetingDuration(0.f)
	, InputBuffer(0.f)
	, InputVector(0.f)
	, bInputFrozen(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	//This makes hard reference to Material, which shouldn't be if we don't need decal component at all.
	/*static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatInst(TEXT("MaterialInstanceConstant'/Attributes/MI_TargetFloor.MI_TargetFloor'"));
	MatInst.Succeeded() ? DecalMaterial = MatInst.Object : DecalMaterial = nullptr;*/
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

	ClearTarget();
}

/*******************************************************************************************/
/*******************************  Player Input  ********************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::EnableTargeting()
{
	if (IsTargetLocked())
	{
		ClearTarget();
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
	if (bCanCaptureTarget && IsTargetLocked() && (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(SwitchDelayHandler)))
	{
		InputVector.X = YawAxis;
	}
}

void ULockOnTargetComponent::SwitchTargetPitch(float PitchAxis)
{
	if (bCanCaptureTarget && IsTargetLocked() && (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(SwitchDelayHandler)))
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

	FVector2D AnalogInput = MoveTemp(InputVector);

	if (!CanInputBeProcessed(AnalogInput.Size()))
	{
		return;
	}

	AnalogInput = AnalogInput.ClampAxes(ClampInputVector.X, ClampInputVector.Y);
	AnalogInput *= GetWorld()->GetDeltaSeconds();
	InputBuffer += AnalogInput;

	if (InputBuffer.Size() > GetInputBufferThreshold())
	{
		if (SwitchDelay > 0.f)
		{
			//Timer without callback, because we only need the fact, that timer in progress.
			TimerManager.SetTimer(SwitchDelayHandler, SwitchDelay, false);
		}

		float TrigonometricInput = ULOTC_BPLibrary::GetTrigonometricAngle2D(InputBuffer);

#if WITH_EDITORONLY_DATA
		if (bShowPlayerInput)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Black, FString::Printf(TEXT("PlayerInput: %.2f deg."), TrigonometricInput), true, FVector2D(1.15f));
		}
#endif

		SwitchTarget(TrigonometricInput);
		bInputFrozen = true;
		InputBuffer = {0.f, 0.f};
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
		bool bInputIsEmpty = FMath::IsNearlyZero(PlayerInput, 0.01f);

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
		TRACE_BOOKMARK(TEXT("LOC_PerformFindTarget"));
		SCOPED_NAMED_EVENT(LOC_TargetFinding, FColor::Red);
#endif

		FTargetInfo NewTargetInfo = TargetHandlerImplementation->FindTarget();

		if (IsValid(NewTargetInfo.HelperComponent))
		{
			SetLockOnTargetNative(NewTargetInfo);
		}
		else
		{
			OnTargetNotFound.Broadcast();
		}
	}
}

bool ULockOnTargetComponent::SwitchTarget(float PlayerInput)
{
	if (IsValid(TargetHandlerImplementation))
	{
#if LOC_INSIGHTS
		TRACE_BOOKMARK(TEXT("LOC_PerformSwitchTarget"));
		SCOPED_NAMED_EVENT(LOC_SwitchingTarget, FColor::Red);
#endif
		
		FTargetInfo NewTargetInfo;

		if (TargetHandlerImplementation->SwitchTarget(NewTargetInfo, PlayerInput))
		{
			if (NewTargetInfo.HelperComponent == GetHelperComponent())
			{
				UpdateTargetSocket(NewTargetInfo.SocketForCapturing);
			}
			else
			{
				ClearTarget();
				SetLockOnTargetNative(NewTargetInfo);
			}

			return true;
		}
		else
		{
			OnTargetNotFound.Broadcast();
		}
	}

	return false;
}

bool ULockOnTargetComponent::SwitchTargetManual(float TrigonometricInput)
{
	return bCanCaptureTarget && IsTargetLocked() && SwitchTarget(TrigonometricInput);
}

/*******************************************************************************************/
/*******************************  Directly Lock On System  *********************************/
/*******************************************************************************************/

void ULockOnTargetComponent::SetLockOnTarget(AActor* NewTarget, const FName& Socket)
{
	if (!bCanCaptureTarget)
	{
		return;
	}

	if (IsTargetLocked())
	{
		ClearTarget();
	}

	if (IsValid(NewTarget))
	{
		UTargetingHelperComponent* NewTargetHelperComponent = NewTarget->FindComponentByClass<UTargetingHelperComponent>();

		if (IsValid(NewTargetHelperComponent) && NewTargetHelperComponent->CanBeTargeted(this))
		{
			SetLockOnTargetNative({NewTargetHelperComponent, Socket});
		}
	}
}

void ULockOnTargetComponent::SetLockOnTargetNative(const FTargetInfo& TargetInfo)
{
#if LOC_INSIGHTS
	SCOPED_NAMED_EVENT(LOC_SetLock, FColor::Emerald);
#endif

	UTargetingHelperComponent* TargetHelpComp = TargetInfo.HelperComponent;

	if (IsValid(TargetHelpComp) && TargetHelpComp != GetHelperComponent() && TargetHelpComp->GetOwner() != GetOwner())
	{
		PrivateTargetInfo = TargetInfo;

		SetupHelperComponent();
		bIsTargetLocked = true;
		OnTargetLocked.Broadcast(GetTarget());
		TargetHandlerImplementation->OnTargetLockedNative(); //Maybe bind to OnTargetLocked in HandlerBase.

		if (bDisableTickWhileUnlocked)
		{
			SetComponentTickEnabled(true);
		}

#if LOC_INSIGHTS
		TRACE_BOOKMARK(TEXT("Target Locked: %s"), *GetNameSafe(GetTarget()));
#endif
	}
}

void ULockOnTargetComponent::SetupHelperComponent()
{
	GetHelperComponent()->CaptureTarget(this, GetCapturedSocket());

	TWeakObjectPtr<ULockOnTargetComponent> ThisComp = this;
	TargetUnlockDelegateHandle = GetHelperComponent()->OnInvadersUnlock.AddLambda(
		[ThisComp](bool bUnlockAll, ULockOnTargetComponent* Invader)
		{
			if (ThisComp.IsValid())
			{
				if (bUnlockAll || Invader == ThisComp)
				{
					ThisComp->ClearTarget();
				}
			}
		});
}

void ULockOnTargetComponent::ClearTarget()
{
	if (IsTargetLocked())
	{
#if LOC_INSIGHTS
		TRACE_BOOKMARK(TEXT("Target unlocked: %s"), *GetNameSafe(GetTarget()));
		SCOPED_NAMED_EVENT(LOC_ClearTarget, FColor::Silver);
#endif

		if (bDisableTickWhileUnlocked)
		{
			SetComponentTickEnabled(false);
		}

		TargetingDuration = 0.f;
		OnTargetUnlocked.Broadcast(GetTarget());
		TargetHandlerImplementation->OnTargetUnlockedNative();

		if (IsValid(GetHelperComponent()))
		{
			GetHelperComponent()->ReleaseTarget(this);
			GetHelperComponent()->OnInvadersUnlock.Remove(TargetUnlockDelegateHandle);
		}

		bIsTargetLocked = false;
		PrivateTargetInfo.Reset();
	}
}

void ULockOnTargetComponent::UpdateTargetSocket(const FName& NewSocket)
{
#if LOC_INSIGHTS
	TRACE_BOOKMARK(TEXT("LOC_SocketChanged %s"), *NewSocket.ToString());
	SCOPED_NAMED_EVENT(LOC_SocketChanged, FColor::Yellow);
#endif

	if (NewSocket != GetCapturedSocket())
	{
		PrivateTargetInfo.SocketForCapturing = NewSocket;
		GetHelperComponent()->UpdateWidget(NewSocket);
	}
}

/*******************************************************************************************/
/*******************************  Tick and During Target Lock features  ********************/
/*******************************************************************************************/

void ULockOnTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if LOC_INSIGHTS
	SCOPED_NAMED_EVENT(LOC_Tick, FColor::Orange);
#endif

	if (IsTargetLocked() && IsValid(TargetHandlerImplementation) && TargetHandlerImplementation->CanContinueTargeting())
	{
		TargetingDuration += DeltaTime;

		ProcessAnalogInput();

		const FVector FocusLocation = GetCapturedLocation(true);
		TickControlRotationCalc(DeltaTime, FocusLocation);
		TickOwnerRotationCalc(DeltaTime, FocusLocation);

#if WITH_EDITORONLY_DATA
		DebugOnTick();
#endif
	}
}

void ULockOnTargetComponent::TickControlRotationCalc(float DeltaTime, const FVector& TargetLocation)
{
	if (IsValid(ControlRotationModeConfig)&& ControlRotationModeConfig->IsEnabled() && GetController())
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
	if (IsValid(OwnerRotationModeConfig) && OwnerRotationModeConfig->IsEnabled() && GetOwner())
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
		TargetedLocation = GetHelperComponent()->GetSocketLocation(GetCapturedSocket(), bWithOffset, const_cast<ULockOnTargetComponent*>(this));
	}

	return TargetedLocation;
}

AController* ULockOnTargetComponent::GetController() const
{
	return GetOwner() ? GetOwner()->GetInstigatorController() : nullptr;
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

/*******************************************************************************************/
/*******************************  Debug  ***************************************************/
/*******************************************************************************************/

#if WITH_EDITORONLY_DATA

void ULockOnTargetComponent::PostEditChangeProperty(struct FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	//const FName PropertyName = Event.GetPropertyName();
	//const FName MemberPropertyName = Event.MemberProperty->GetFName();
}

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
				InvadersName.Append(FString::Printf(TEXT("\n %s"), *GetNameSafe(Invader->GetOwner())));
			}
		}

		GEngine->AddOnScreenDebugMessage(-1, 0.f, DebugInfoColor, FString::Printf(TEXT("Target Invaders amount: %i \nInvaders: %s\nSockets Amount: %i"), GetHelperComponent()->GetInvaders().Num(), *InvadersName, GetHelperComponent()->Sockets.Num()), true, FVector2D(1.1f));
		GEngine->AddOnScreenDebugMessage(-1, 0.f, DebugInfoColor, FString::Printf(TEXT("ControlRotationMode: %s and OwnerRotationMode: %s"), *GetNameSafe(ControlRotationModeConfig), *GetNameSafe(OwnerRotationModeConfig)), true, FVector2D(1.1f));

		if (bFreezeInputAfterSwitch)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.f, DebugInfoColor, FString::Printf(TEXT("Is Input Frozen: %s "), bInputFrozen ? TEXT("true") : TEXT("false")), true, FVector2D(1.1f));
		}

		GEngine->AddOnScreenDebugMessage(-1, 0.f, DebugInfoColor, FString::Printf(TEXT("Input Buffer ratio = %.3f / %.3f InputVector2D: X %.3f, Y %.3f "), InputBuffer.Size(), GetInputBufferThreshold(), InputBuffer.X, InputBuffer.Y), true, FVector2D(1.1f));
		GEngine->AddOnScreenDebugMessage(-1, 0.f, DebugInfoColor, FString::Printf(TEXT("\n\n\nTarget: %s \nDuration: %.2f \nCapturedSocket: %s"), *GetNameSafe(GetTarget()), GetTargetingDuration(), *PrivateTargetInfo.SocketForCapturing.ToString()), true, FVector2D(1.15f));
	}
}

#endif
