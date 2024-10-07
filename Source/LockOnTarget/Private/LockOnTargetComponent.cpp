// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetComponent.h"
#include "TargetComponent.h"
#include "TargetHandlers/TargetHandlerBase.h"
#include "LockOnTargetDefines.h"
#include "LockOnTargetExtensions/LockOnTargetExtensionBase.h"

#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

ULockOnTargetComponent::ULockOnTargetComponent()
	: bCanCaptureTarget(true)
	, InputBufferThreshold(.08f)
	, BufferResetFrequency(.2f)
	, ClampInputVector(-1.f, 1.f)
	, InputProcessingDelay(0.2f)
	, bUseInputFreezing(true)
	, UnfreezeThreshold(1e-2f)
	, CurrentTargetInternal(FTargetInfo::NULL_TARGET)
	, TargetingDuration(0.f)
	, bIsTargetLocked(false)
	, bInputFrozen(false)
	, InputBuffer(0.f)
	, InputVector(0.f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = true; //TargetingDuration initial sync.
	PrimaryComponentTick.TickInterval = 0.f;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
}

void ULockOnTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	//@TODO: Subscribe on FWorldDelegates::OnWorldBeginTearDown to disable Targeting, instead of checking it manually.

	InitializeSubobject(GetTargetHandler());

	//Reverse loop to remove invalid extensions.
	for (int32 i = Extensions.Num() - 1; i >= 0; --i)
	{
		ULockOnTargetExtensionBase* const Extension = Extensions[i];

		if (IsValid(Extension))
		{
			InitializeSubobject(Extension);
		}
		else
		{
			Extensions.RemoveAtSwap(i, 1, false);
		}
	}

	Extensions.Shrink();
}

void ULockOnTargetComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	bCanCaptureTarget = false;

	if (IsTargetLocked())
	{
		NotifyTargetReleased(CurrentTargetInternal);
	}

	ClearTargetHandler();
	RemoveAllExtensions();

	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
}

void ULockOnTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsTargetLocked())
	{
		TargetingDuration += DeltaTime;

		if (HasAuthorityOverTarget())
		{
			ProcessAnalogInput(DeltaTime);
			CheckTargetState(DeltaTime);
		}
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

	//Need to initially synchronize the timer for unmapped simulated proxies.
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

FVector ULockOnTargetComponent::GetCapturedFocusPointLocation() const
{
	return IsTargetLocked() ? GetTargetComponent()->GetFocusPointLocation(this) : FVector(0.f);
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
			const FFindTargetRequestParams RequestParams;
			RequestFindTarget(RequestParams);
		}
	}
}

void ULockOnTargetComponent::RequestFindTarget(const FFindTargetRequestParams& RequestParams)
{
	LOT_BOOKMARK("RequestFindTarget");
	LOT_SCOPED_EVENT(RequestFindTarget);
	checkf(HasAuthorityOverTarget(), TEXT("Only the locally controlled owners are able to find a Target."));

	if (GetTargetHandler())
	{
		ProcessTargetHandlerResponse(GetTargetHandler()->FindTarget(RequestParams));
	}
}

void ULockOnTargetComponent::ProcessTargetHandlerResponse(const FFindTargetRequestResponse& Response)
{
	const FTargetInfo TargetInfo = Response.Target;

	if (CanTargetBeCaptured(TargetInfo))
	{
		UpdateTargetInfo(TargetInfo);
	}
	else
	{
		NotifyTargetNotFound();
	}
}

void ULockOnTargetComponent::CheckTargetState(float DeltaTime)
{
	check(IsTargetLocked() && HasAuthorityOverTarget());

	if (GetTargetHandler())
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

	//Don't know why, but the listen server controlled pawn has the AutonomousProxy remote role, unlike the Controller.
	//There's a hack as I don't want to have an AController/APawn dependency.
	//We can get the remote role from the owner's owner which is the Controller.
	if (const AActor* const Controller = GetOwner()->GetOwner())
	{
		if (Controller->GetRemoteRole() != ROLE_AutonomousProxy && LocalRole == ROLE_Authority)
		{
			// Local authority in control.
			return true;
		}
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
		FFindTargetRequestParams Params;
		Params.PlayerInput = PlayerInput;
		RequestFindTarget(Params);
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
			NotifyTargetSocketChanged(OldTarget.Socket);
		}
		else
		{
			//Another or a new Target.
			if (IsTargetLocked())
			{
				NotifyTargetReleased(OldTarget);
			}

			NotifyTargetCaptured(CurrentTargetInternal);
		}
	}
	else
	{
		//Null Target.
		NotifyTargetReleased(OldTarget);
	}
}

void ULockOnTargetComponent::NotifyTargetCaptured(const FTargetInfo& Target)
{
	LOT_BOOKMARK("Target captured %s", *GetNameSafe(Target->GetOwner()));
	LOT_SCOPED_EVENT(NotifyTargetCaptured);

	check(Target.TargetComponent);

	bIsTargetLocked = true;
	SetComponentTickEnabled(true);
	Target->NotifyTargetCaptured(this);

	if (HasBegunPlay())
	{
		ForEachSubobject([&Target](ULockOnTargetExtensionProxy* Extension)
			{
				Extension->OnTargetLocked(Target.TargetComponent, Target.Socket);
			});
	}

	OnTargetLocked.Broadcast(Target.TargetComponent, Target.Socket);
}

void ULockOnTargetComponent::NotifyTargetReleased(const FTargetInfo& Target)
{
	LOT_BOOKMARK("Target released %s", *GetNameSafe(Target->GetOwner()));
	LOT_SCOPED_EVENT(NotifyTargetReleased);

	check(Target.TargetComponent);

	bIsTargetLocked = false;
	SetComponentTickEnabled(false);
	Target->NotifyTargetReleased(this);

	ForEachSubobject([&Target](ULockOnTargetExtensionProxy* Extension)
		{
			Extension->OnTargetUnlocked(Target.TargetComponent, Target.Socket);
		});

	OnTargetUnlocked.Broadcast(Target.TargetComponent, Target.Socket);

	//Clear the timer after notifying all listeners.
	TargetingDuration = 0.f;
}

void ULockOnTargetComponent::NotifyTargetSocketChanged(FName OldSocket)
{
	LOT_BOOKMARK("SocketChanged %s->%s", *OldSocket.ToString(), *GetCapturedSocket().ToString());
	LOT_SCOPED_EVENT(ReceiveTargetSocketUpdate);

	ForEachSubobject([this, OldSocket](ULockOnTargetExtensionProxy* Extension)
		{
			Extension->OnSocketChanged(GetTargetComponent(), GetCapturedSocket(), OldSocket);
		});

	OnSocketChanged.Broadcast(GetTargetComponent(), GetCapturedSocket(), OldSocket);
}

void ULockOnTargetComponent::NotifyTargetNotFound()
{
	LOT_BOOKMARK("TargetNotFound");
	LOT_SCOPED_EVENT(NotifyTargetNotFound);

	ForEachSubobject([this](ULockOnTargetExtensionProxy* Extension)
		{
			Extension->OnTargetNotFound(IsTargetLocked());
		});

	OnTargetNotFound.Broadcast(IsTargetLocked());
}

void ULockOnTargetComponent::ReceiveTargetException(ETargetExceptionType Exception)
{
	LOT_BOOKMARK("ReceiveTargetException");
	LOT_SCOPED_EVENT(ReceiveTargetException);

	const FTargetInfo Target = CurrentTargetInternal;

	//Clear Target locally.
	Server_UpdateTargetInfo_Implementation(FTargetInfo::NULL_TARGET);

	if (!GetWorld()->bIsTearingDown && HasAuthorityOverTarget())
	{
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
/*******************************  LockOnTarget Extensions  *********************************/
/*******************************************************************************************/

void ULockOnTargetComponent::InitializeSubobject(ULockOnTargetExtensionProxy* Subobject)
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

void ULockOnTargetComponent::DestroySubobject(ULockOnTargetExtensionProxy* Subobject)
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

TArray<ULockOnTargetExtensionProxy*, TInlineAllocator<8>> ULockOnTargetComponent::GetAllSubobjects() const
{
	TArray<ULockOnTargetExtensionProxy*, TInlineAllocator<8>> Subobjects;

	if (IsValid(GetTargetHandler()))
	{
		Subobjects.Add(GetTargetHandler());
	}

	for (auto& Extension : Extensions)
	{
		if (IsValid(Extension))
		{
			Subobjects.Add(Extension);
		}
	}

	return Subobjects;
}

void ULockOnTargetComponent::SetDefaultTargetHandler(UTargetHandlerBase* InDefaultTargetHandler)
{
	TargetHandlerImplementation = InDefaultTargetHandler;
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

ULockOnTargetExtensionBase* ULockOnTargetComponent::FindExtensionByClass(TSubclassOf<ULockOnTargetExtensionBase> ExtensionClass) const
{
	auto* const Extension = Extensions.FindByPredicate([ExtensionClass](const ULockOnTargetExtensionBase* const Extension)
		{
			return IsValid(Extension) && Extension->IsA(ExtensionClass);
		});

	return Extension ? *Extension : nullptr;
}

void ULockOnTargetComponent::AddDefaultExtension(ULockOnTargetExtensionBase* InDefaultExtension)
{
	Extensions.Add(InDefaultExtension);
}

ULockOnTargetExtensionBase* ULockOnTargetComponent::AddExtensionByClass(TSubclassOf<ULockOnTargetExtensionBase> ExtensionClass)
{
	ULockOnTargetExtensionBase* const NewExtension = NewObject<ULockOnTargetExtensionBase>(this, ExtensionClass);

	if (NewExtension)
	{
		InitializeSubobject(NewExtension);
		Extensions.Add(NewExtension);
	}

	return NewExtension;
}

bool ULockOnTargetComponent::RemoveExtensionByClass(TSubclassOf<ULockOnTargetExtensionBase> ExtensionClass)
{
	if (ExtensionClass.Get())
	{
		for (int32 i = 0; i < Extensions.Num(); ++i)
		{
			ULockOnTargetExtensionBase* const Extension = Extensions[i];

			if (ensure(IsValid(Extension)) && Extension->IsA(ExtensionClass))
			{
				DestroySubobject(Extension);
				Extensions.RemoveAtSwap(i, 1, false);

				return true;
			}
		}
	}

	return false;
}

void ULockOnTargetComponent::RemoveAllExtensions()
{
	for (ULockOnTargetExtensionBase* const Extension : Extensions)
	{
		DestroySubobject(Extension);
	}

	Extensions.Empty();
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
	LOT_SCOPED_EVENT(ProcessInput);

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
		bInputFrozen = bUseInputFreezing;
		ActivateInputDelay(); //Prevent reliable buffer overflow.
		{
			FFindTargetRequestParams RequestParams;
			RequestParams.PlayerInput = InputBuffer;
			RequestFindTarget(RequestParams);
		}
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
