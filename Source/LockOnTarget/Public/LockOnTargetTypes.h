// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LockOnTargetTypes.generated.h"

class UTargetComponent;

/**
 * Holds information related to the Target.
 */
USTRUCT(BlueprintType)
struct LOCKONTARGET_API FTargetInfo
{
	GENERATED_BODY()

public:

	static const FTargetInfo NULL_TARGET;

	FTargetInfo() = default;

	FTargetInfo(UTargetComponent* InTarget, FName InSocket = NAME_None)
		: TargetComponent(InTarget)
		, Socket(InSocket)
	{
	}

	[[nodiscard]] UTargetComponent& operator*() const
	{
		check(TargetComponent);
		return *TargetComponent;
	}

	[[nodiscard]] UTargetComponent* operator->() const
	{
		check(TargetComponent);
		return TargetComponent;
	}

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

private:

	uint32 GetSocketIndex() const;

public:

	UPROPERTY(BlueprintReadWrite, Category = "TargetInfo")
	TObjectPtr<UTargetComponent> TargetComponent = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category = "TargetInfo")
	FName Socket = NAME_None;
};

template<>
struct TStructOpsTypeTraits<FTargetInfo> : public TStructOpsTypeTraitsBase2<FTargetInfo>
{
	enum
	{
		WithNetSerializer = true,		
		WithIdenticalViaEquality = true,
	};
};

inline bool operator==(const FTargetInfo& lhs, const FTargetInfo& rhs)
{
	return lhs.TargetComponent == rhs.TargetComponent && lhs.Socket == rhs.Socket;
}

inline bool operator!=(const FTargetInfo& lhs, const FTargetInfo& rhs)
{
	return !(lhs == rhs);
}

/**
 * The types of exceptions/interrupts that Targets can dispatch to Invaders. Supports event-driven design.
 */
UENUM(BlueprintType)
enum class ETargetExceptionType : uint8
{
	Destruction			UMETA(ToolTip="Target is being removed from the level."),
	StateInvalidation	UMETA(ToolTip="Target has entered an invalid state."),
	SocketInvalidation	UMETA(ToolTip="Target has deleted a Socket.")
};

//Finds a component within an Actor by name. If not found or Name == None, then nulltpr will be returned.
template<typename T>
typename TEnableIf<TPointerIsConvertibleFromTo<T, const class UActorComponent>::Value, T>::Type* FindComponentByName(const AActor* Actor, FName Name)
{
	T* ReturnComponent = nullptr;

	if (IsValid(Actor) && !Name.IsNone())
	{
		TInlineComponentArray<T*> Components(Actor);

		T** FoundComponent = Components.FindByPredicate(
			[Name](const auto* const Comp)
			{
				return Comp->GetFName() == Name;
			}
		);

		ReturnComponent = FoundComponent ? *FoundComponent : nullptr;
	}

	return ReturnComponent;
}
