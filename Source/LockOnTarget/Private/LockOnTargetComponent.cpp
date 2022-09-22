// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetComponent.h"
#include "TargetingHelperComponent.h"
#include "LockOnSubobjects/TargetHandlers/TargetHandlerBase.h"
#include "LockOnTargetDefines.h"
#include "LockOnSubobjects/LockOnTargetModuleBase.h"

#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ULockOnTargetComponent::ULockOnTargetComponent()
	: bCanCaptureTarget(true)
	, InputBufferThreshold(.15f)
	, BufferResetFrequency(.2f)
	, ClampInputVector(-2.f, 2.f)
	, InputProcessingDelay(0.25f)
	, bFreezeInputAfterSwitch(true)
	, UnfreezeThreshold(1e-2f)
	, TargetingDuration(0.f)
	, bIsTargetLocked(false)
	, InputBuffer(0.f)
	, InputVector(0.f)
	, bInputFrozen(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(true);

	//Seems work since UE5.0
	//TargetHandlerImplementation = CreateDefaultSubobject<UDefaultTargetHandler>(TEXT("TargetHandler"));
}

void ULockOnTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(TargetHandlerImplementation))
	{
		TargetHandlerImplementation->InitializeModule(this);
	}

	for (ULockOnTargetModuleBase* const Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->InitializeModule(this);
		}
	}
}

void ULockOnTargetComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	ReleaseTarget(CurrentTargetInternal);
	RemoveAllModules();

	if (IsValid(TargetHandlerImplementation))
	{
		TargetHandlerImplementation->DeinitializeModule(this);
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ULockOnTargetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//To read more about network: PushModel.h, NetSerialization.h, ActorChannel.h, NetDriver.h,
	//NetConnection.h, ReplicationGraph.h, DataReplication.h, NetworkGuid.h, FastArraySerializer.h.
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true; //Can be activated in DefaultEngine.ini
	Params.Condition = COND_SkipOwner;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, CurrentTargetInternal, Params);

	//Basically need to initially replicate the TargetingDuration cause a SimulatedProxy may not be relevant while the Target was locked. Doesn't make sense for the AutonomousProxy.
	DOREPLIFETIME_CONDITION(ThisClass, TargetingDuration, COND_InitialOnly); //@TODO: Maybe sync in the OnRep or not, cause it's accurate enough for this type of timer.
}

/*******************************************************************************************/
/*******************************  Player Input  ********************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::EnableTargeting()
{
	if (bCanCaptureTarget && !IsInputDelayActive())
	{
		ActivateInputDelay(); //Prevent reliable buffer overflow.
		IsTargetLocked() ? ClearTargetManual() : FindTarget();
	}
}

void ULockOnTargetComponent::SwitchTargetYaw(float YawAxis)
{
	InputVector.X = YawAxis;
}

void ULockOnTargetComponent::SwitchTargetPitch(float PitchAxis)
{
	InputVector.Y = PitchAxis;
}

bool ULockOnTargetComponent::IsInputDelayActive() const
{
	return InputProcessingDelay > 0.f && GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(InputProcessingDelayHandler);
}

void ULockOnTargetComponent::ActivateInputDelay()
{
	if (InputProcessingDelay > 0.f && GetWorld())
	{
		//Timer without a callback, 'cause we only need the fact that the timer is in progress.
		GetWorld()->GetTimerManager().SetTimer(InputProcessingDelayHandler, InputProcessingDelay, false);
	}
}

void ULockOnTargetComponent::ProcessAnalogInput(float DeltaTime)
{
	LOT_EVENT(ProcessInput, Blue);

	//@TODO: If the Target is unlocked while the Input is frozen, then it'll be frozen until the new Target is captured.

	//If you want to have an ability to unfreeze input while delay is active then change the order in the if statement below.
	if (!bCanCaptureTarget || !GetWorld() || IsInputDelayActive() || !CanInputBeProcessed(InputVector.Size()))
	{
		return;
	}

	InputBuffer += InputVector.ClampAxes(ClampInputVector.X, ClampInputVector.Y) * DeltaTime;

	if (InputBuffer.Size() > InputBufferThreshold)
	{
		if (bFreezeInputAfterSwitch)
		{
			bInputFrozen = true;
		}

		ActivateInputDelay(); //Prevent reliable buffer overflow.
		FindTarget(InputBuffer);
		ClearInputBuffer();
	}

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();

	if (!TimerManager.IsTimerActive(BufferResetHandler))
	{
		TimerManager.SetTimer(BufferResetHandler, FTimerDelegate::CreateUObject(this, &ULockOnTargetComponent::ClearInputBuffer), BufferResetFrequency, false);
	}
}

bool ULockOnTargetComponent::CanInputBeProcessed(float PlayerInput) const
{
	if (bFreezeInputAfterSwitch && bInputFrozen)
	{
		bInputFrozen = PlayerInput > UnfreezeThreshold;
	}

	return PlayerInput > 0.f && !bInputFrozen;
}

void ULockOnTargetComponent::ClearInputBuffer()
{
	InputBuffer = { 0.f, 0.f };
}

/*******************************************************************************************/
/******************************** Target Handling ******************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::FindTarget(FVector2D OptionalInput)
{
	LOT_BOOKMARK("PerformFindTarget");
	LOT_EVENT(FindTarget, Green);

	if (IsValid(TargetHandlerImplementation))
	{
		const FTargetInfo NewTargetInfo = TargetHandlerImplementation->FindTarget(OptionalInput);
		ProcessTargetHandlerResult(NewTargetInfo);
	}
	else
	{
		LOG_WARNING("Attemtp to access an invalid TargetHandler by %s", *GetNameSafe(GetOwner()));
	}
}

void ULockOnTargetComponent::ProcessTargetHandlerResult(const FTargetInfo& TargetInfo)
{
	checkf(TargetInfo.GetActor() != GetOwner(), TEXT("Capturing yourself is not allowed."));

	//Validate the Target.
	const bool bCanTargetBeCaptured = IsValid(TargetInfo.HelperComponent) && TargetInfo.HelperComponent->CanBeCaptured(this);
	//If Target is locked and we've got the same TargetInfo then don't process it.
	const bool bIsTargetNotSame = !IsTargetLocked() || TargetInfo != CurrentTargetInternal;

	if (bCanTargetBeCaptured && bIsTargetNotSame)
	{
		UpdateTargetInfo(TargetInfo);
	}
	else
	{
		LOT_BOOKMARK("TargetNotFound");

		OnTargetNotFound.Broadcast();
	}
}

bool ULockOnTargetComponent::CanContinueTargeting()
{
	//Corner case. Shouldn't actually be happened.
	if (!GetOwner())
	{
		ClearTargetManual();
		return false;
	}

	/**
	 * A few words about Target's network synchronization in the LockOnTargetComponent:
	 *
	 * It's not safe to capture a non-replicated Target that only exists in the owning player's world or isn't stably named (e.g. spawned at runtime).
	 * Cause otherwise the Target will only be captured on that machine and the server won't know anything.
	 * https://docs.unrealengine.com/5.0/en-US/replicating-object-references-in-unreal-engine/
	 *
	 * Captured Target's validity check is performed on the owning client (via TargetHandler or automatically) and on the non-owning client (automatically).
	 * There is a problem that we can't know whether the Target Actor was replicated after its destruction (as far as I know, at least without extra memory).
	 * So if the Target isn't replicated and destroyed locally on the server or simulated proxy, then the Target may become out of sync!
	 * The owning client will still be capturing the Target, while the server won't be.
	 *
	 * General advice is to not capture non-replicated Target in case of unstably named actors and don't destroy them at all while they're locked.
	 * Instead mark them as 'can't be captured' in the TargetingHelperComponent on the owning client and then destroy after a while.
	 **/

	 //@TODO: Actors without Controller should also use the TargetHadler.
	if (IsLocallyControlled())
	{
		//Only the locally controlled component refers to the TargetHandler.
		if (IsValid(TargetHandlerImplementation))
		{
			//If TargetHandler exists then give it a chance to decide what to do with the nulled Target, e.g. find a new Target.
			if (TargetHandlerImplementation->CanContinueTargeting())
			{
				//Just verifying that TargetHandler 'handled' everything correctly or skipped these checks, cause otherwise the Target will be out of sync.
				if (!IsValid(GetTarget()) || !GetHelperComponent()->CanBeCaptured(this))
				{
					ClearTargetManual();
					return false;
				}
			}
			else
			{
				//If TargetHandler decided that the Target should be released and didn't perform Target clearing then clear it manual.
				if (IsTargetLocked())
				{
					ClearTargetManual();
				}

				return false;
			}
		}
		else
		{
			//If TargetHandlerImplementation is invalid then provide checks manually.
			if (!IsValid(GetTarget()) || !GetHelperComponent()->CanBeCaptured(this))
			{
				ClearTargetManual();
				return false;
			}
		}

	}
	else
	{
		//Non-locally controlled Server and SimulatedProxies just clear the Target locally.
		if (!IsValid(GetTarget()))
		{
			ClearTargetManual();
			return false;
		}
	}

	return true;
}

/*******************************************************************************************/
/*******************************  Manual Handling  *****************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::SetLockOnTargetManual(AActor* NewTarget, FName Socket)
{
	checkf(NewTarget != GetOwner(), TEXT("Capturing yourself is not allowed."));

	if (bCanCaptureTarget && IsValid(NewTarget))
	{
		const FTargetInfo NewTargetInfo{ NewTarget->FindComponentByClass<UTargetingHelperComponent>(), Socket };

		if (IsValid(NewTargetInfo.HelperComponent) && NewTargetInfo.HelperComponent->CanBeCaptured(this) && CurrentTargetInternal != NewTargetInfo)
		{
			UpdateTargetInfo(NewTargetInfo);
		}
	}
}

void ULockOnTargetComponent::SwitchTargetManual(FVector2D PlayerInput)
{
	if (bCanCaptureTarget && IsTargetLocked())
	{
		FindTarget(PlayerInput);
	}
}

void ULockOnTargetComponent::ClearTargetManual(bool bAutoFindNewTarget)
{
	//Nice to have: Don't send the RPC if the CurrentTargetInternal is invalid, cause it'll automatically be cleared on the server.
	//But this will only work in case of replicated actors. You can read more in CanContinueTargeting() implementation.
	if (IsTargetLocked())
	{
		//Implementation uses 1 reliable server call.
		if (bAutoFindNewTarget)
		{
			//Clear the current Target locally.
			Server_UpdateTargetInfo_Implementation({ nullptr, NAME_None });

			//Find a new Target and if found automatically update on the server.
			//Note that the released Target can be captured again.
			FindTarget();

			//If a new Target isn't found then clear the Target on the server.
			if (!IsTargetLocked() && GetOwnerRole() == ROLE_AutonomousProxy)
			{
				Server_UpdateTargetInfo({ nullptr, NAME_None });
			}
		}
		else
		{
			UpdateTargetInfo({ nullptr, NAME_None });
		}
	}
}

/*******************************************************************************************/
/*******************************  Directly Lock On System  *********************************/
/*******************************************************************************************/

void ULockOnTargetComponent::UpdateTargetInfo(const FTargetInfo& TargetInfo)
{
	//Update the Target locally.
	Server_UpdateTargetInfo_Implementation(TargetInfo);

	//Update the Target on the server.
	if (GetOwnerRole() == ROLE_AutonomousProxy)
	{
		Server_UpdateTargetInfo(TargetInfo);
	}
}

void ULockOnTargetComponent::Server_UpdateTargetInfo_Implementation(const FTargetInfo& TargetInfo)
{
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, CurrentTargetInternal, this);
	const FTargetInfo OldTarget = CurrentTargetInternal;
	CurrentTargetInternal = TargetInfo;
	OnTargetInfoUpdated(OldTarget);
}

void ULockOnTargetComponent::OnTargetInfoUpdated(const FTargetInfo& OldTarget)
{
	if (IsValid(CurrentTargetInternal.HelperComponent))
	{
		if (OldTarget.HelperComponent == CurrentTargetInternal.HelperComponent)
		{
			UpdateTargetSocket(OldTarget.SocketForCapturing);
		}
		else
		{
			ReleaseTarget(OldTarget);
			CaptureTarget(CurrentTargetInternal);
		}
	}
	else
	{
		ReleaseTarget(OldTarget);
	}
}

void ULockOnTargetComponent::CaptureTarget(const FTargetInfo& Target)
{
	LOT_BOOKMARK("Target captured %s", *GetNameSafe(Target.GetActor()));

	bIsTargetLocked = true;

	//Notify TargetingHelperComponent.
	Target.HelperComponent->CaptureTarget(this, Target.SocketForCapturing);

	//Notify all listeners including the TargetHandler.
	OnTargetLocked.Broadcast(Target.HelperComponent, Target.SocketForCapturing);
}

void ULockOnTargetComponent::ReleaseTarget(const FTargetInfo& Target)
{
	if (IsTargetLocked())
	{
		LOT_BOOKMARK("Target released %s", *GetNameSafe(Target.GetActor()));

		bIsTargetLocked = false;

		//Notify TargetingHelperComponent.
		if (IsValid(Target.HelperComponent))
		{
			Target.HelperComponent->ReleaseTarget(this);
		}

		//Notify all listeners including the TargetHandler.
		OnTargetUnlocked.Broadcast(Target.HelperComponent, Target.SocketForCapturing);

		//Clear the timer after notifying all listeners.
		TargetingDuration = 0.f;
	}
}

void ULockOnTargetComponent::UpdateTargetSocket(FName OldSocket)
{
	LOT_BOOKMARK("SocketChanged %s->%s", *OldSocket.ToString(), *GetCapturedSocket().ToString());

	GetHelperComponent()->CaptureTarget(this, GetCapturedSocket());
	OnSocketChanged.Broadcast(GetHelperComponent(), GetCapturedSocket(), OldSocket);
}

/*******************************************************************************************/
/***************************************  Tick  ********************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	LOT_EVENT(Tick, Red);

	if (IsTargetLocked() && CanContinueTargeting())
	{
		TargetingDuration += DeltaTime;

		if (IsLocallyControlled())
		{
			//Process the input only for the locally controlled Owner.
			ProcessAnalogInput(DeltaTime);
		}
	}

	if (IsValid(TargetHandlerImplementation))
	{
		TargetHandlerImplementation->Update(InputVector, DeltaTime);
	}

	UpdateModules(DeltaTime);
}

/*******************************************************************************************/
/*******************************  LockOnTarget Modules  ************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::UpdateModules(float DeltaTime)
{
	for (ULockOnTargetModuleBase* const Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->Update(InputVector, DeltaTime);
		}
	}
}

const TArray<ULockOnTargetModuleBase*>& ULockOnTargetComponent::GetAllModules() const
{
	return Modules;
}

ULockOnTargetModuleBase* ULockOnTargetComponent::FindModuleByClass(TSubclassOf<ULockOnTargetModuleBase> ModuleClass) const
{
	auto Module = Modules.FindByPredicate([ModuleClass](const ULockOnTargetModuleBase* const Module)
		{
			return IsValid(Module) && Module->IsA(ModuleClass);
		});

	return Module ? *Module : nullptr;
}

ULockOnTargetModuleBase* ULockOnTargetComponent::AddModuleByClass(TSubclassOf<ULockOnTargetModuleBase> ModuleClass)
{
	ULockOnTargetModuleBase* NewModule = NewObject<ULockOnTargetModuleBase>(this, ModuleClass);

	if (NewModule)
	{
		NewModule->InitializeModule(this);
		Modules.Add(NewModule);
	}
	else
	{
		LOG_ERROR("Failed to create the module named %s", *ModuleClass->GetName());
	}

	return NewModule;
}

bool ULockOnTargetComponent::RemoveModuleByClass(TSubclassOf<ULockOnTargetModuleBase> ModuleClass)
{
	if (ModuleClass.Get())
	{
		for (int32 i = 0; i < Modules.Num(); ++i)
		{
			ULockOnTargetModuleBase* const Module = Modules[i];

			if (IsValid(Module) && Module->IsA(ModuleClass))
			{
				Module->DeinitializeModule(this);
				Modules.RemoveAtSwap(i);
				return true;
			}
		}
	}

	return false;
}

void ULockOnTargetComponent::RemoveAllModules()
{
	for (ULockOnTargetModuleBase* const Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->DeinitializeModule(this);
		}
	}

	Modules.Empty();
}

/*******************************************************************************************/
/************************************  Misc  ***********************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::SetCanCaptureTarget(bool bInCanCaptureTarget)
{
	bCanCaptureTarget = bInCanCaptureTarget;

	//To prevent the situation where the owning client is still capturing the Target while the server is not.
	if (!bCanCaptureTarget && IsLocallyControlled())
	{
		ClearTargetManual();
	}
}

void ULockOnTargetComponent::SetTargetHandler(UTargetHandlerBase* NewTargetHandler)
{
	if (IsValid(NewTargetHandler) && NewTargetHandler != TargetHandlerImplementation)
	{
		if (IsValid(TargetHandlerImplementation))
		{
			TargetHandlerImplementation->DeinitializeModule(this);
		}

		TargetHandlerImplementation = NewTargetHandler;
		TargetHandlerImplementation->InitializeModule(this);
	}
}

UTargetHandlerBase* ULockOnTargetComponent::SetTargetHandlerByClass(TSubclassOf<UTargetHandlerBase> TargetHandlerClass)
{
	SetTargetHandler(NewObject<UTargetHandlerBase>(this, TargetHandlerClass));
	return TargetHandlerImplementation;
}

AActor* ULockOnTargetComponent::GetTarget() const
{
	return GetHelperComponent() ? GetHelperComponent()->GetOwner() : nullptr;
}

FVector ULockOnTargetComponent::GetCapturedSocketLocation() const
{
	return IsTargetLocked() && GetHelperComponent() ? GetHelperComponent()->GetSocketLocation(GetCapturedSocket()) : FVector(0.f);
}

FVector ULockOnTargetComponent::GetCapturedFocusLocation() const
{
	return IsTargetLocked() && GetHelperComponent() ? GetHelperComponent()->GetFocusLocation(this) : FVector(0.f);
}

AController* ULockOnTargetComponent::GetController() const
{
	return GetOwner() ? GetOwner()->GetInstigatorController() : nullptr;
}

bool ULockOnTargetComponent::IsLocallyControlled() const
{
	const AController* const Controller = GetController();
	return IsValid(Controller) && Controller->IsLocalController();
}

APlayerController* ULockOnTargetComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetController());
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
	const APlayerController* const Controller = GetPlayerController();
	return IsValid(Controller) ? Controller->PlayerCameraManager->GetCameraRotation() : GetOwnerRotation();
}

FVector ULockOnTargetComponent::GetCameraLocation() const
{
	const APlayerController* const Controller = GetPlayerController();
	return IsValid(Controller) ? Controller->PlayerCameraManager->GetCameraLocation() : GetOwnerLocation();
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
