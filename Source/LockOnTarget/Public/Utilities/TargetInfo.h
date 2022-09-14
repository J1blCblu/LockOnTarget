/// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetInfo.generated.h"

class UTargetingHelperComponent;

/**
 * 
 */
USTRUCT(BlueprintType)
struct LOCKONTARGET_API FTargetInfo
{
	GENERATED_BODY()

public:
	FTargetInfo() = default;

	FTargetInfo(UTargetingHelperComponent* HC, FName S)
		: HelperComponent(HC)
		, SocketForCapturing(S)
	{
	}

public:
	UPROPERTY(BlueprintReadWrite, Category = TargetInfo)
	TObjectPtr<UTargetingHelperComponent> HelperComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = TargetInfo)
	FName SocketForCapturing = NAME_None;

public:
	void Reset();
	AActor* GetActor() const;
};

inline void FTargetInfo::Reset()
{
	HelperComponent = nullptr;
	SocketForCapturing = NAME_None;
}

inline bool operator==(const FTargetInfo& lhs, const FTargetInfo& rhs)
{
	return lhs.HelperComponent == rhs.HelperComponent && lhs.SocketForCapturing == rhs.SocketForCapturing;
}

inline bool operator!=(const FTargetInfo& lhs, const FTargetInfo& rhs)
{
	return !(lhs == rhs);
}
