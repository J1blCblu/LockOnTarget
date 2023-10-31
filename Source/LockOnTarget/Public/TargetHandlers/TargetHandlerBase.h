// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnTargetExtensions/LockOnTargetExtensionBase.h"
#include "LockOnTargetTypes.h"
#include "TargetHandlerBase.generated.h"

/**
 * A set of optional params for the FindTarget() request.
 */
USTRUCT(BlueprintType)
struct LOCKONTARGET_API FFindTargetRequestParams
{
	GENERATED_BODY()

public:
	
	/** Whether to generate a detailed response. May have a significant impact on performance. */
	UPROPERTY(BlueprintReadWrite, Category = "Request Params")
	bool bGenerateDetailedResponse = false;

	/** Optional player input. */
	UPROPERTY(BlueprintReadWrite, Category = "Request Params")
	FVector2D PlayerInput = FVector2D::ZeroVector;

	/** An optional payload object passed along with the request. Might be useful for custom implementations. */
	UPROPERTY(BlueprintReadWrite, Category = "Request Params")
	TObjectPtr<UObject> Payload = nullptr;
};

/**
 * A set of params returned by the FindTarget() request.
 */
USTRUCT(BlueprintType)
struct LOCKONTARGET_API FFindTargetRequestResponse
{
	GENERATED_BODY()

public:

	/** The actual Target found by the FindTarget() request. May be NULL_TARGET. */
	UPROPERTY(BlueprintReadWrite, Category = "Request Params")
	FTargetInfo Target = FTargetInfo::NULL_TARGET;

	/** An optional payload object passed along with the response. May be useful for custom implementations. */
	UPROPERTY(BlueprintReadWrite, Category = "Request Params")
	TObjectPtr<UObject> Payload = nullptr;
};

/**
 * Special abstract LockOnTargetExtension which is used to handle the Target.
 * Responsible for finding and maintaining the Target.
 * 
 * It's recommended to override FindTarget(), CheckTargetState() and HandleTargetException().
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Abstract, DefaultToInstanced, EditInlineNew, HideDropdown)
class LOCKONTARGET_API UTargetHandlerBase : public ULockOnTargetExtensionProxy
{
	GENERATED_BODY()

public:

	//@TODO: Perhaps someday create a SoftTargetHandler for dynamic Targeting, like Spider-Man from Insomniac Games, Batman: Arkham Knight and others.

	UTargetHandlerBase();

public: /** Target Handler Interface */

	/**
	 * Finds and returns a Target to be captured by LockOnTargetComponent.
	 * 
	 * @param	RequestParams	A set of optional params for the request.
	 * @return	Target to be captured or NULL_TARGET.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LockOnTarget|Target Handler Base", meta = (AutoCreateRefTerm="RequestParams"))
	FFindTargetRequestResponse FindTarget(const FFindTargetRequestParams& RequestParams = FFindTargetRequestParams());

	/**
	 * (Optional) Checks the Target state between updates.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Target Handler Base")
	void CheckTargetState(const FTargetInfo& Target, float DeltaTime);

	/**
	 * (Optional) Processes an exception from the Target.
	 *
	 * @Note: Target has already been cleared.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Target Handler Base", meta = (ForceAsFunction))
	void HandleTargetException(const FTargetInfo& Target, ETargetExceptionType Exception);

	/** Whether the target meets all the requirements for being captured. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget|Target Handler Base")
	bool IsTargetValid(const UTargetComponent* Target) const;

private: /** Internal */

	virtual FFindTargetRequestResponse FindTarget_Implementation(const FFindTargetRequestParams& RequestParams);
	virtual void CheckTargetState_Implementation(const FTargetInfo& Target, float DeltaTime);
	virtual void HandleTargetException_Implementation(const FTargetInfo& Target, ETargetExceptionType Exception);
};
