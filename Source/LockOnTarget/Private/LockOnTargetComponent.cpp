// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetComponent.h"
#include "TargetComponent.h"
#include "TargetHandlers/TargetHandlerBase.h"
#include "LockOnTargetDefines.h"
#include "LockOnTargetModuleBase.h"

#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ULockOnTargetComponent::ULockOnTargetComponent()
	: bCanCaptureTarget(true)
	, InputBufferThreshold(.15f)
	, BufferResetFrequency(.2f)
	, ClampInputVector(-2.f, 2.f)
	, InputProcessingDelay(0.25f)
	, bFreezeInputAfterSwitch(true)
	, UnfreezeThreshold(1e-2f)
	, CurrentTargetInternal(FTargetInfo::NULL_TARGET)
	, TargetingDuration(0.f)
	, bIsTargetLocked(false)
	, bInputFrozen(false)
	, InputBuffer(0.f)
	, InputVector(0.f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = true; //TargetingDuration initial sync.
	PrimaryComponentTick.TickInterval = 0.f;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;

	//Seems work since UE5.0
	//TargetHandlerImplementation = CreateDefaultSubobject<UThirdPersonTargetHandler>(TEXT("TargetHandler"));
}

void ULockOnTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeSubobject(GetTargetHandler());

	//Reverse loop to remove invalid modules.
	for (int32 i = Modules.Num() - 1; i >= 0; --i)
	{
		ULockOnTargetModuleBase* const Module = Modules[i];

		if (IsValid(Module))
		{
			InitializeSubobject(Module);
		}
		else
		{
			Modules.RemoveAtSwap(i);
		}
	}
}

void ULockOnTargetComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	bCanCaptureTarget = false;
	OnTargetReleased(CurrentTargetInternal);

	ClearTargetHandler();
	RemoveAllModules();

	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
}

void ULockOnTargetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//To read more about network: PushModel.h, NetSerialization.h, ActorChannel.h, NetDriver.h,
	//NetConnection.h, ReplicationGraph.h, DataReplication.h, NetworkGuid.h, FastArraySerializer.h,
	//CoreNet.h, PackageMapClient.h, RepLayout.h.

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true; //Can be activated in DefaultEngine.ini
	Params.Condition = COND_SkipOwner; //Local authority.
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, CurrentTargetInternal, Params);

	//We need to initially synchronize the timer for unmapped simulated proxies. It's not accurate, but does it make sense?
	DOREPLIFETIME_CONDITION(ThisClass, TargetingDuration, COND_InitialOnly);
}

/*******************************************************************************************/
/************************************  Polls  **********************************************/
/*******************************************************************************************/

AActor* ULockOnTargetComponent::GetTargetActor() const
{
	return IsTargetLocked() ? GetTargetComponent()->GetOwner() : nullptr;
}

FVector ULockOnTargetComponent::GetCapturedSocketLocation() const
{
	return IsTargetLocked() ? GetTargetComponent()->GetSocketLocation(GetCapturedSocket()) : FVector(0.f);
}

FVector ULockOnTargetComponent::GetCapturedFocusLocation() const
{
	return IsTargetLocked() ? GetTargetComponent()->GetFocusLocation(this) : FVector(0.f);
}

/*******************************************************************************************/
/********************************  Target Validation  **************************************/
/*******************************************************************************************/

bool ULockOnTargetComponent::CanTargetBeCaptured(const FTargetInfo& TargetInfo) const
{
	return IsTargetValid(TargetInfo.TargetComponent) && (!IsTargetLocked() || TargetInfo != CurrentTargetInternal);
}

bool ULockOnTargetComponent::IsTargetValid(const UTargetComponent* Target) const
{
	return IsValid(Target) && Target->CanBeCaptured() && Target->GetOwner() != GetOwner();
}

/*******************************************************************************************/
/*******************************  Target Handling  *****************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::EnableTargeting()
{
	if (CanCaptureTarget() && !IsInputDelayActive())
	{
		//Prevents reliable buffer overflow.
		ActivateInputDelay();

		if (IsTargetLocked())
		{
			ClearTargetManual();
		}
		else
		{
			TryFindTarget();
		}
	}
}

void ULockOnTargetComponent::TryFindTarget(FVector2D OptionalInput)
{
	LOT_BOOKMARK("PerformFindTarget");
	LOT_SCOPED_EVENT(TryFindTarget, Green);

	checkf(HasAuthorityOverTarget(), TEXT("Only the locally controlled owners are able to find a Target."));

	if (IsValid(GetTargetHandler()))
	{
		const FTargetInfo NewTargetInfo = GetTargetHandler()->FindTarget(OptionalInput);
		ProcessTargetHandlerResult(NewTargetInfo);
	}
	else
	{
		LOG_WARNING("Attemtp to access the invalid TargetHandler by %s", *GetFullNameSafe(GetOwner()));
	}
}

void ULockOnTargetComponent::ProcessTargetHandlerResult(const FTargetInfo& TargetInfo)
{
	if (CanTargetBeCaptured(TargetInfo))
	{
		UpdateTargetInfo(TargetInfo);
	}
	else
	{
		LOT_BOOKMARK("TargetNotFound");

		ForEachSubobject([this](ULockOnTargetModuleProxy* Module)
			{
				Module->OnTargetNotFound(IsTargetLocked());
			});

		OnTargetNotFound.Broadcast(IsTargetLocked());
	}
}

void ULockOnTargetComponent::CheckTargetState(float DeltaTime)
{
	check(IsTargetLocked() && HasAuthorityOverTarget());

	if (IsValid(GetTargetHandler()))
	{
		GetTargetHandler()->CheckTargetState(CurrentTargetInternal, DeltaTime);
	}
}

bool ULockOnTargetComponent::HasAuthorityOverTarget() const
{
	const ENetMode NetMode = GetNetMode();

	if (NetMode == NM_Standalone)
	{
		// Not networked.
		return true;
	}

	ENetRole LocalRole = GetOwnerRole();

	if (NetMode == NM_Client && LocalRole == ROLE_AutonomousProxy)
	{
		// Networked client in control.
		return true;
	}

	if (GetOwner()->GetRemoteRole() != ROLE_AutonomousProxy && LocalRole == ROLE_Authority)
	{
		// Local authority in control.
		return true;
	}

	return false;
}

bool ULockOnTargetComponent::CanCaptureTarget() const
{
	return bCanCaptureTarget && HasBegunPlay() && HasAuthorityOverTarget();
}

void ULockOnTargetComponent::SetCanCaptureTarget(bool bInCanCaptureTarget)
{
	if (bInCanCaptureTarget != bCanCaptureTarget)
	{
		bCanCaptureTarget = bInCanCaptureTarget;

		if (!bCanCaptureTarget)
		{
			ClearTargetManual();
		}
	}
}

/*******************************************************************************************/
/*******************************  Manual Handling  *****************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::SetLockOnTargetManual(AActor* NewTarget, FName Socket)
{
	if (IsValid(NewTarget))
	{
		SetLockOnTargetManualByInfo({ NewTarget->FindComponentByClass<UTargetComponent>(), Socket });
	}
}

void ULockOnTargetComponent::SetLockOnTargetManualByInfo(const FTargetInfo& TargetInfo)
{
	if (CanCaptureTarget() && CanTargetBeCaptured(TargetInfo))
	{
		UpdateTargetInfo(TargetInfo);
	}
}

void ULockOnTargetComponent::SwitchTargetManual(FVector2D PlayerInput)
{
	if (IsTargetLocked() && HasAuthorityOverTarget())
	{
		TryFindTarget(PlayerInput);
	}
}

void ULockOnTargetComponent::ClearTargetManual()
{
	if (IsTargetLocked() && HasAuthorityOverTarget())
	{
		UpdateTargetInfo(FTargetInfo::NULL_TARGET);
	}
}

/*******************************************************************************************/
/*******************************  Target Synchronization  **********************************/
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
	if (TargetInfo != CurrentTargetInternal)
	{
		const FTargetInfo OldTarget = CurrentTargetInternal;
		CurrentTargetInternal = TargetInfo;
		OnTargetInfoUpdated(OldTarget);
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, CurrentTargetInternal, this);
	}
}

bool ULockOnTargetComponent::Server_UpdateTargetInfo_Validate(const FTargetInfo& TargetInfo)
{
	return !TargetInfo.TargetComponent || CanTargetBeCaptured(TargetInfo);
}

void ULockOnTargetComponent::OnTargetInfoUpdated(const FTargetInfo& OldTarget)
{
	if (IsValid(CurrentTargetInternal.TargetComponent))
	{
		if (OldTarget.TargetComponent == CurrentTargetInternal.TargetComponent)
		{
			//Same Target with a new socket.
			OnTargetSocketChanged(OldTarget.Socket);
		}
		else
		{
			//Another or a new Target.
			OnTargetReleased(OldTarget);
			OnTargetCaptured(CurrentTargetInternal);
		}
	}
	else
	{
		//Null Target.
		OnTargetReleased(OldTarget);
	}
}

void ULockOnTargetComponent::OnTargetCaptured(const FTargetInfo& Target)
{
	check(Target.TargetComponent);

	LOT_BOOKMARK("Target captured %s", *GetNameSafe(Target.TargetComponent->GetOwner()));

	bIsTargetLocked = true;
	Target.TargetComponent->CaptureTarget(this);

	ForEachSubobject([&Target](ULockOnTargetModuleProxy* Module)
		{
			Module->OnTargetLocked(Target.TargetComponent, Target.Socket);
		});

	OnTargetLocked.Broadcast(Target.TargetComponent, Target.Socket);
}

void ULockOnTargetComponent::OnTargetReleased(const FTargetInfo& Target)
{
	if (IsTargetLocked())
	{
		check(Target.TargetComponent);

		LOT_BOOKMARK("Target released %s", *GetNameSafe(Target.TargetComponent->GetOwner()));

		bIsTargetLocked = false;
		Target.TargetComponent->ReleaseTarget(this);

		ForEachSubobject([&Target](ULockOnTargetModuleProxy* Module)
			{
				Module->OnTargetUnlocked(Target.TargetComponent, Target.Socket);
			});

		OnTargetUnlocked.Broadcast(Target.TargetComponent, Target.Socket);

		//Clear the timer after notifying all listeners.
		TargetingDuration = 0.f;
	}
}

void ULockOnTargetComponent::OnTargetSocketChanged(FName OldSocket)
{
	LOT_BOOKMARK("SocketChanged %s->%s", *OldSocket.ToString(), *GetCapturedSocket().ToString());

	ForEachSubobject([this, OldSocket](ULockOnTargetModuleProxy* Module)
		{
			Module->OnSocketChanged(GetTargetComponent(), GetCapturedSocket(), OldSocket);
		});

	OnSocketChanged.Broadcast(GetTargetComponent(), GetCapturedSocket(), OldSocket);
}

void ULockOnTargetComponent::ReceiveTargetException(ETargetExceptionType Exception)
{
	const FTargetInfo Target = CurrentTargetInternal;

	//Clear Target locally.
	Server_UpdateTargetInfo_Implementation(FTargetInfo::NULL_TARGET);

	if (HasAuthorityOverTarget())
	{
		//@TODO: There is one extra RPC if the Target was destroyed as it replicates.
		//But in case of relevancy, we need to send it.

		if (IsValid(GetTargetHandler()))
		{
			GetTargetHandler()->HandleTargetException(Target, Exception);
		}

		//If Target is still null, then sync with the server if needed.
		if (!IsTargetLocked() && GetOwnerRole() == ROLE_AutonomousProxy)
		{
			Server_UpdateTargetInfo(FTargetInfo::NULL_TARGET);
		}
	}
}

/*******************************************************************************************/
/***************************************  Tick  ********************************************/
/*******************************************************************************************/

void ULockOnTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	LOT_SCOPED_EVENT(Tick, Red);

	if (IsTargetLocked())
	{
		TargetingDuration += DeltaTime;

		if (HasAuthorityOverTarget())
		{
			ProcessAnalogInput(DeltaTime);
			CheckTargetState(DeltaTime);
		}
	}

	ForEachSubobject([DeltaTime](ULockOnTargetModuleProxy* Module)
		{
			Module->Update(DeltaTime);
		});
}

/*******************************************************************************************/
/*******************************  LockOnTarget Subobjects  *********************************/
/*******************************************************************************************/

void ULockOnTargetComponent::InitializeSubobject(ULockOnTargetModuleProxy* Subobject)
{
	if (IsValid(Subobject) && HasBegunPlay())
	{
		Subobject->Initialize(this);

		if (IsTargetLocked())
		{
			Subobject->OnTargetLocked(GetTargetComponent(), GetCapturedSocket());
		}
	}
}

void ULockOnTargetComponent::DestroySubobject(ULockOnTargetModuleProxy* Subobject)
{
	if (IsValid(Subobject))
	{
		if (Subobject->IsInitialized())
		{
			if (IsTargetLocked())
			{
				//Some resources might be captured in OnTargetLocked, so we need to release them.
				Subobject->OnTargetUnlocked(GetTargetComponent(), GetCapturedSocket());
			}

			Subobject->Deinitialize(this);
		}

		Subobject->MarkAsGarbage();
	}
}

TArray<ULockOnTargetModuleProxy*, TInlineAllocator<8>> ULockOnTargetComponent::GetAllSubobjects() const
{
	TArray<ULockOnTargetModuleProxy*, TInlineAllocator<8>> Subobjects;

	if (IsValid(GetTargetHandler()))
	{
		Subobjects.Add(GetTargetHandler());
	}

	for (auto& Module : Modules)
	{
		if (IsValid(Module))
		{
			Subobjects.Add(Module);
		}
	}

	return Subobjects;
}

UTargetHandlerBase* ULockOnTargetComponent::SetTargetHandlerByClass(TSubclassOf<UTargetHandlerBase> TargetHandlerClass)
{
	UTargetHandlerBase* const TargetHandler = NewObject<UTargetHandlerBase>(this, TargetHandlerClass);

	if (TargetHandler)
	{
		ClearTargetHandler();
		InitializeSubobject(TargetHandler);
		TargetHandlerImplementation = TargetHandler;
	}

	return TargetHandler;
}

void ULockOnTargetComponent::ClearTargetHandler()
{
	if (IsValid(GetTargetHandler()))
	{
		DestroySubobject(GetTargetHandler());
		TargetHandlerImplementation = nullptr;
	}
}

ULockOnTargetModuleBase* ULockOnTargetComponent::FindModuleByClass(TSubclassOf<ULockOnTargetModuleBase> ModuleClass) const
{
	auto* const Module = Modules.FindByPredicate([ModuleClass](const ULockOnTargetModuleBase* const Module)
		{
			return IsValid(Module) && Module->IsA(ModuleClass);
		});

	return Module ? *Module : nullptr;
}

ULockOnTargetModuleBase* ULockOnTargetComponent::AddModuleByClass(TSubclassOf<ULockOnTargetModuleBase> ModuleClass)
{
	ULockOnTargetModuleBase* const NewModule = NewObject<ULockOnTargetModuleBase>(this, ModuleClass);

	if (NewModule)
	{
		InitializeSubobject(NewModule);
		Modules.Add(NewModule);
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

			if (ensure(IsValid(Module)) && Module->IsA(ModuleClass))
			{
				DestroySubobject(Module);
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
		DestroySubobject(Module);
	}

	Modules.Empty();
}

/*******************************************************************************************/
/*******************************  Player Input  ********************************************/
/*******************************************************************************************/

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
	LOT_SCOPED_EVENT(ProcessInput, Blue);

	check(IsTargetLocked() && HasAuthorityOverTarget());

	//@TODO: If the Target is unlocked while the Input is frozen, then it'll be frozen until the new Target is captured.

	const FVector2D ConsumedInput = ConsumeInput();

	if (IsInputDelayActive() || !CanInputBeProcessed(ConsumedInput))
	{
		return;
	}

	InputBuffer += ConsumedInput.ClampAxes(ClampInputVector.X, ClampInputVector.Y) * DeltaTime;

	if (InputBuffer.SizeSquared() > FMath::Square(InputBufferThreshold))
	{
		bInputFrozen = bFreezeInputAfterSwitch;
		ActivateInputDelay(); //Prevent reliable buffer overflow.
		TryFindTarget(InputBuffer);
		ClearInputBuffer();
	}

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();

	if (!TimerManager.IsTimerActive(BufferResetHandler))
	{
		TimerManager.SetTimer(BufferResetHandler, FTimerDelegate::CreateUObject(this, &ULockOnTargetComponent::ClearInputBuffer), BufferResetFrequency, false);
	}
}

FVector2D ULockOnTargetComponent::ConsumeInput()
{
	const FVector2D Consumed{ InputVector };
	InputVector = FVector2D::ZeroVector;
	return Consumed;
}

bool ULockOnTargetComponent::CanInputBeProcessed(FVector2D Input)
{
	const float InputSq = Input.SizeSquared();

	if (bInputFrozen)
	{
		bInputFrozen = InputSq > FMath::Square(UnfreezeThreshold);
	}

	return InputSq > 0.f && !bInputFrozen;
}

void ULockOnTargetComponent::ClearInputBuffer()
{
	InputBuffer = { 0.f, 0.f };
}
