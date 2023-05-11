// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "TargetComponent.h"
#include "TargetManager.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetDefines.h"

#include "Components/MeshComponent.h"

UTargetComponent::UTargetComponent()
	: bCanBeCaptured(true)
	, TrackedMeshName(NAME_None)
	, CaptureRadius(1700.f)
	, LostOffsetRadius(100.f)
	, FocusPoint(EFocusPoint::CapturedSocket)
	, FocusPointCustomSocket(NAME_None)
	, FocusPointOffset(0.f)
	, bWantsDisplayWidget(true)
	, WidgetRelativeOffset(0.f)
	, bSkipMeshInitializationByName(false)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	Sockets.Add(NAME_None);
}

UTargetManager& UTargetComponent::GetTargetManager() const
{
	return UTargetManager::Get(*GetWorld());
}

void UTargetComponent::BeginPlay()
{
	Super::BeginPlay();
	verify(GetTargetManager().RegisterTarget(this));

	if (!bSkipMeshInitializationByName)
	{
		TrackedMeshComponent = FindMeshComponent();
	}
}

void UTargetComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
	bCanBeCaptured = false;
	DispatchTargetException(ETargetExceptionType::Destruction);
	verify(GetTargetManager().UnregisterTarget(this));
}

/**
 * Target State
 */

bool UTargetComponent::CanBeCaptured() const
{
	return bCanBeCaptured && (Sockets.Num() > 0) && GetOwner() && CanBeReferencedOverNetwork();
}

bool UTargetComponent::CanBeReferencedOverNetwork() const
{
	//IsSupportedForNetworking() is not required, but leaved as a precaution.
	return IsNetMode(NM_Standalone) || (IsSupportedForNetworking() && GetOwner()->GetIsReplicated());
}

bool UTargetComponent::IsCaptured() const
{
	return Invaders.Num() > 0;
}

void UTargetComponent::SetCanBeCaptured(bool bInCanBeCaptured)
{
	bCanBeCaptured = bInCanBeCaptured;

	if (!bCanBeCaptured)
	{
		DispatchTargetException(ETargetExceptionType::StateInvalidation);
	}
}

TArray<ULockOnTargetComponent*> UTargetComponent::GetInvaders() const
{
	return TArray<ULockOnTargetComponent*>(Invaders);
}

void UTargetComponent::CaptureTarget(ULockOnTargetComponent* Instigator)
{
	if (ensure(IsValid(Instigator)))
	{
		checkf(IsSocketValid(Instigator->GetCapturedSocket()), TEXT("Captured socket doesn't exists in the Target."));
		Invaders.Add(Instigator);
		K2_OnCaptured(Instigator);
		OnCaptured.Broadcast(Instigator);
	}
}

void UTargetComponent::ReleaseTarget(ULockOnTargetComponent* Instigator)
{
	if (ensure(IsValid(Instigator)))
	{
		verify(Invaders.RemoveSingleSwap(Instigator, false));
		K2_OnReleased(Instigator);
		OnReleased.Broadcast(Instigator);
	}
}

void UTargetComponent::DispatchTargetException(ETargetExceptionType Exception)
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

/**
 * Sockets
 */

bool UTargetComponent::IsSocketValid(FName Socket) const
{
	return Sockets.Contains(Socket);
}

FVector UTargetComponent::GetSocketLocation(FName Socket) const
{
	return GetTrackedMeshComponent() ? GetTrackedMeshComponent()->GetSocketLocation(Socket) : GetOwner()->GetActorLocation();
}

bool UTargetComponent::AddSocket(FName Socket)
{
	bool bAdded = false;

	if (GetTrackedMeshComponent() && GetTrackedMeshComponent()->DoesSocketExist(Socket) && !Sockets.Contains(Socket))
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

/**
 * Focus Point
 */

FVector UTargetComponent::GetFocusLocation(const ULockOnTargetComponent* Instigator) const
{
	FVector FocusPointLocation{ 0.f };

	switch (FocusPoint)
	{
	case EFocusPoint::CapturedSocket:

		FocusPointLocation = GetSocketLocation(IsValid(Instigator) ? Instigator->GetCapturedSocket() : NAME_None);
		break;

	case EFocusPoint::CustomSocket:

		FocusPointLocation = GetSocketLocation(FocusPointCustomSocket);
		break;

	case EFocusPoint::Custom:

		FocusPointLocation = GetCustomFocusPoint(Instigator);
		break;

	default:

		LOG_ERROR("Unknown EFocusPoint is discovered.");
		checkNoEntry();
		break;
	}

	return FocusPointLocation + FocusPointOffset;
}

FVector UTargetComponent::GetCustomFocusPoint_Implementation(const ULockOnTargetComponent* Instigator) const
{
	LOG_WARNING("Default implementation is called. Please override in child classes.");
	return FVector{ 0.f };
}

/**
 * Tracked Mesh
 */

USceneComponent* UTargetComponent::GetTrackedMeshComponent() const
{
	return TrackedMeshComponent.IsValid() ? TrackedMeshComponent.Get() : GetRootComponent();
}

void UTargetComponent::SetTrackedMeshComponent(USceneComponent* InTrackedComponent)
{
	if (IsValid(InTrackedComponent) && InTrackedComponent != TrackedMeshComponent.Get())
	{
		TrackedMeshComponent = InTrackedComponent;

		if (!HasBegunPlay())
		{
			bSkipMeshInitializationByName = true;
		}
	}
}

USceneComponent* UTargetComponent::FindMeshComponent() const
{
	USceneComponent* Mesh = FindComponentByName<UMeshComponent>(GetOwner(), TrackedMeshName);

	//If not found or None.
	if (!Mesh)
	{
		Mesh = GetRootComponent();
	}

	return Mesh;
}

USceneComponent* UTargetComponent::GetRootComponent() const
{
	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}
