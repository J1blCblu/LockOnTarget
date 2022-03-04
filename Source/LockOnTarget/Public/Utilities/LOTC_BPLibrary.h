// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LOTC_BPLibrary.generated.h"

UCLASS()
class ULOTC_BPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Determines if the Actor is in viewport even if there are no bounds vertices bounds on the viewport.
	 * Useful for large objects. For small objects just use simple version.
	 * Function uses Actor bounds and ranges overlap.
	 * 
	 * @param Actor - Actor to check if it is on the viewport.
	 * @param PlayerController - PlayerController to determine the viewport size and other things.
	 * @param bRenderCheck - Can little faster determine the result. Also helps to determine if the Actor is behind any object.
	 * @param ScreenOffset - Narrows the screen resolution in percent of screen size for check viewport boundaries. Should be in (0.f, 50.f).
	 * 
	 * @return - Is Actor on the viewport screen.
	 */
	UFUNCTION(BlueprintPure, Category = LockOnTarget)
	static bool IsActorOnScreenComplex(AActor* Actor, APlayerController* PlayerController, bool bRenderCheck = true, const FVector2D& ScreenOffset = FVector2D(0.f, 0.f));

	/**
	 * Determines if the Actor is in viewport.
	 * Uses Actor location, which converts to screen position.
	 * For large objects you can use Complex version.
	 * 
	 * @param Actor - Actor to check if it is on the viewport.
	 * @param PlayerController - PlayerController to determine the viewport size and other things.
	 * @param bRenderCheck - Can little faster determine the result. Also helps to determine if the Actor is behind any object.
	 * @param ScreenOffset - Narrows the screen resolution in percent of screen size for check viewport boundaries. Should be in (0.f, 50.f).
	 * 
	 * @return - Is Actor on the viewport screen.
	 */
	UFUNCTION(BlueprintPure, Category = LockOnTarget)
	static bool IsActorOnScreenSimple(AActor* Actor, APlayerController* PC, bool bRenderCheck = true, const FVector2D& ScreenOffset = FVector2D(0.f, 0.f));

	/** Return array of box vertices with world location. */
	UFUNCTION(BlueprintPure, Category = LockOnTarget)
	static void GetBoxVertices(const FVector& BoxOrigin, const FVector& BoxExtents, TArray<FVector>& Verices);

	/**
	 * Determines if World location is on the screen.
	 * 
	 * @param ScreenOffset - Narrows the screen resolution in percent of screen size for check viewport boundaries. Should be in (0.f, 50.f).
	 */
	UFUNCTION(BlueprintPure, Category = LockOnTarget)
	static bool IsVectorOnScreen(APlayerController* PC, const FVector& WorldLocation, const FVector2D& ScreenOffset);

	/**
	 * Determines if 2D point is on the screen.
	 * 
	 * @param ScreenOffset - Narrows the screen resolution in percent of screen size for check viewport boundaries. Should be in (0.f, 50.f).
	 */
	UFUNCTION(BlueprintPure, Category = LockOnTarget)
	static bool Is2DPointOnScreen(APlayerController* PC, const FIntPoint& ScreenPosition, const FVector2D& ScreenOffset = FVector2D(0.f, 0.f));
	
	/**************** Angles *********************/

	/**Get angle between two Directions*/
	static float GetAngle(const FVector& Btw1, const FVector& Btw2);

	/**Get trigonometric angle in 2D space (between [0,360) degrees) compared to (1.f,0.f) Vector (Trigonometric 0.f)*/
	static float GetTrigonometricAngle2D(const FVector2D& Vector);

	/**Get trigonometric angle in 3D space (between [0,360) degrees) with projection Point onto plane.*/
	static float GetTrigonometricAngle3D(const FVector& Target, const FVector& OriginPointOnPlane, const FRotator& Rotation);

	static bool IsAngleInRange(float TargetAngle, float BaseAngle, float Range);
};
