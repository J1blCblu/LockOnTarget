/// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Structs.generated.h"

class UTargetingHelperComponent;

USTRUCT(BlueprintType)
struct FTargetInfo
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

	UPROPERTY(BlueprintReadWrite, Category = LOT_TargetInfo)
	UTargetingHelperComponent* HelperComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = LOT_TargetInfo)
	FName SocketForCapturing = NAME_None;

public:

	void Reset()
	{
		HelperComponent = nullptr;
		SocketForCapturing = NAME_None;
	}

	AActor* GetActor() const;
};
