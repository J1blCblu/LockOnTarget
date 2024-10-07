// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "TargetComponent.h"
#include "TargetManager.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetDefines.h"

#include "Components/SceneComponent.h"

UTargetComponent::UTargetComponent()
	: bCanBeCaptured(true)
	, AssociatedComponentName(NAME_None)
	, bForceCustomCaptureRadius(false)
	, CustomCaptureRadius(2700.f)
	, Priority(0.5)
	, FocusPointType(ETargetFocusPointType::CapturedSocket)
	, FocusPointCustomSocket(NAME_None)
	, FocusPointRelativeOffset(0.f)
	, bWantsDisplayWidget(true)
	, WidgetRelativeOffset(0.f)
	, AssociatedComponent(nullptr)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = true;
	Sockets.Add(NAME_None);
}

UTargetManager& UTargetComponent::GetTargetManager() const
{
	check(GetWorld());
	return UTargetManager::Get(*GetWorld());
}

void UTargetComponent::InitializeComponent()
{
	Super::InitializeComponent();

	//If the associated component isn't set yet or changed from details.
	if (!AssociatedComponent.IsValid() || AssociatedComponent->GetFName() != AssociatedComponentName)
	{
		if (const AActor* const Owner = GetOwner())
		{
			if (AssociatedComponentName.IsNone())
			{
				AssociatedComponent = Owner->GetRootComponent();
			}
			else
			{
				AssociatedComponent = FindComponentByName<USceneComponent>(Owner, AssociatedComponentName);
			}
		}
	}
}

void UTargetComponent::SetAssociatedComponent(USceneComponent* InAssociatedComponent)
{
	if (IsValid(InAssociatedComponent) && InAssociatedComponent != AssociatedComponent.Get())
	{
		check(!InAssociatedComponent->IsEditorOnly());
		AssociatedComponent = InAssociatedComponent;
		AssociatedComponentName = InAssociatedComponent->GetFName(); //For proper display in details.
	}
}

void UTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	//@TODO: Move to Initialize() but test multiplayer setup and seamless travel.
	GetTargetManager().RegisterTarget(this);
}

void UTargetComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
	bCanBeCaptured = false;
	DispatchTargetException(ETargetExceptionType::Destruction);
	GetTargetManager().UnregisterTarget(this);
}

bool UTargetComponent::CanBeCaptured() const
{
	return bCanBeCaptured && (Sockets.Num() > 0) && GetOwner() && CanBeReferencedOverNetwork();
}

bool UTargetComponent::CanBeReferencedOverNetwork() const
{
	//IsSupportedForNetworking() is not required, but leaved as a precaution.
	return IsNetMode(NM_Standalone) || (IsSupportedForNetworking() && GetOwner()->GetIsReplicated());
}

void UTargetComponent::SetCanBeCaptured(bool bInCanBeCaptured)
{
	if (bCanBeCaptured != bInCanBeCaptured)
	{
		bCanBeCaptured = bInCanBeCaptured;

		if (!bCanBeCaptured)
		{
			DispatchTargetException(ETargetExceptionType::StateInvalidation);
		}
	}
}

void UTargetComponent::NotifyTargetCaptured(ULockOnTargetComponent* Instigator)
{
	check(IsValid(Instigator) && Instigator->GetTargetComponent() == this);
	check(IsSocketValid(Instigator->GetCapturedSocket())); //Checked here to reduce runtime overhead.
	Invaders.Add(Instigator);
	K2_OnCaptured(Instigator);
	OnTargetComponentCaptured.Broadcast(Instigator);
}

void UTargetComponent::NotifyTargetReleased(ULockOnTargetComponent* Instigator)
{
	check(IsValid(Instigator));
	Invaders.RemoveSingleSwap(Instigator, false);
	K2_OnReleased(Instigator);
	OnTargetComponentReleased.Broadcast(Instigator);
}

void UTargetComponent::DispatchTargetException(ETargetExceptionType Exception)
{
	if (IsCaptured())
	{
		//Reverse loop as elements might be removed.
		for (int32 i = Invaders.Num() - 1; i >= 0; --i)
		{
			if (Exception == ETargetExceptionType::SocketInvalidation && IsSocketValid(Invaders[i]->GetCapturedSocket()))
			{
				continue;
			}

			Invaders[i]->ReceiveTargetException(Exception);
		}
	}
}

FVector UTargetComponent::GetSocketLocation(FName Socket) const
{
	return AssociatedComponent.IsValid() ? AssociatedComponent->GetSocketLocation(Socket) : GetOwner()->GetActorLocation();
}

void UTargetComponent::SetDefaultSocket(FName Socket)
{
	if (Sockets.IsEmpty())
	{
		Sockets.Add(Socket);
	}
	else if (Sockets[0] != Socket)
	{
		Sockets[0] = Socket;

		DispatchTargetException(ETargetExceptionType::SocketInvalidation);
	}
}

bool UTargetComponent::AddSocket(FName Socket)
{
	const bool bIsSuccessful = !Sockets.Contains(Socket);

	if (bIsSuccessful)
	{
		Sockets.Add(Socket);
	}

	return bIsSuccessful;
}

bool UTargetComponent::RemoveSocket(FName Socket)
{
	const bool bIsSuccessful = Sockets.RemoveSingleSwap(Socket, false) > 0;

	if (bIsSuccessful)
	{
		DispatchTargetException(ETargetExceptionType::SocketInvalidation);
	}

	return bIsSuccessful;
}

FVector UTargetComponent::GetFocusPointLocation(const ULockOnTargetComponent* Instigator) const
{
	check(IsValid(Instigator) && GetOwner());
	FVector FocusPointLocation{ 0.f };

	switch (FocusPointType)
	{
	case ETargetFocusPointType::CapturedSocket:
		FocusPointLocation = GetSocketLocation(Instigator->GetCapturedSocket());
		break;

	case ETargetFocusPointType::CustomSocket:
		FocusPointLocation = GetSocketLocation(FocusPointCustomSocket);
		break;

	case ETargetFocusPointType::Custom:
		FocusPointLocation = GetCustomFocusPoint(Instigator);
		break;

	default:
		checkNoEntry();
		break;
	}

	if (!FocusPointRelativeOffset.IsNearlyZero())
	{
		FocusPointLocation += GetOwner()->GetActorTransform().TransformVectorNoScale(FocusPointRelativeOffset);
	}

	return FocusPointLocation;
}

FVector UTargetComponent::GetCustomFocusPoint_Implementation(const ULockOnTargetComponent* Instigator) const
{
	LOG_WARNING("Default implementation is called. Please override in child classes.");
	return FVector::ZeroVector;
}
