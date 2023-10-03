// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "TargetComponent.h"
#include "TargetManager.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetDefines.h"

#include "Components/MeshComponent.h"

UTargetComponent::UTargetComponent()
	: bCanBeCaptured(true)
	, AssociatedComponentName(NAME_None)
	, CaptureRadius(2200.f)
	, LostOffsetRadius(150.f)
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
	return UTargetManager::Get(*GetWorld());
}

void UTargetComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (!AssociatedComponent.IsValid())
	{
		if (AssociatedComponentName.IsNone())
		{
			if (const AActor* const Owner = GetOwner())
			{
				AssociatedComponent = Owner->GetRootComponent();
			}
		}
		else
		{
			AssociatedComponent = FindComponentByName<UMeshComponent>(GetOwner(), AssociatedComponentName);
		}
	}
}

void UTargetComponent::SetAssociatedComponent(USceneComponent* InAssociatedComponent)
{
	if (IsValid(InAssociatedComponent) && InAssociatedComponent != AssociatedComponent.Get())
	{
		AssociatedComponent = InAssociatedComponent;
	}
}

void UTargetComponent::BeginPlay()
{
	Super::BeginPlay();
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
	Invaders.RemoveSingle(Instigator);
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
	const USceneComponent* const TrackedComponent = GetAssociatedComponent();
	return TrackedComponent ? TrackedComponent->GetSocketLocation(Socket) : GetOwner()->GetActorLocation();
}

bool UTargetComponent::AddSocket(FName Socket)
{
	bool bAdded = false;

	const USceneComponent* const TrackedComponent = GetAssociatedComponent();

	if (TrackedComponent && TrackedComponent->DoesSocketExist(Socket) && !Sockets.Contains(Socket))
	{
		Sockets.Add(Socket);
		bAdded = true;
	}

	return bAdded;
}

bool UTargetComponent::RemoveSocket(FName Socket)
{
	const bool bRemoved = Sockets.RemoveSingle(Socket) > 0;

	if (bRemoved)
	{
		DispatchTargetException(ETargetExceptionType::SocketInvalidation);
	}

	return bRemoved;
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
