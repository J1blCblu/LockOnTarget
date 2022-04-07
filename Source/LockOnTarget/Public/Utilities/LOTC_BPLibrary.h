// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LOTC_BPLibrary.generated.h"

UCLASS()
class ULOTC_BPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Determines if the Actor is in the viewport even if there are no bounds vertices bounds in the viewport.
	 * Useful for large objects. For small objects just use the simple version.
	 * The function uses the Actor's bounds and the ranges overlap.
	 * 
	 * @param Actor - Actor to check if it is in the viewport.
	 * @param PlayerController - PlayerController to determine the viewport size.
	 * @param bRenderCheck - Can little faster determine the result. Also helps to determine if the Actor is behind any object.
	 * @param ScreenOffset - Narrows the screen resolution in percent of screen size for check viewport boundaries. Should be in (0.f, 50.f).
	 * 
	 * @return - Is Actor on the viewport screen.
	 */
	UFUNCTION(BlueprintPure, Category = LockOnTarget)
	static bool IsActorOnScreenComplex(AActor* Actor, APlayerController* PlayerController, bool bRenderCheck = true, const FVector2D& ScreenOffset = FVector2D(0.f, 0.f));

	/**
	 * Determines if the Actor is in viewport.
	 * Uses the Actor's location, which is converted to a position on the screen.
	 * For large objects you can use the complex version.
	 * 
	 * @param Actor - Actor to check if it is in the viewport.
	 * @param PlayerController - PlayerController to determine the viewport size.
	 * @param bRenderCheck - Can little faster determine the result. Also helps to determine if the Actor is behind any object.
	 * @param ScreenOffset - Narrows the screen resolution in percent of screen size for check viewport boundaries. Should be in (0.f, 50.f).
	 * 
	 * @return - Is Actor on the viewport screen.
	 */
	UFUNCTION(BlueprintPure, Category = LockOnTarget)
	static bool IsActorOnScreenSimple(AActor* Actor, APlayerController* PC, bool bRenderCheck = true, const FVector2D& ScreenOffset = FVector2D(0.f, 0.f));

	/** Returns an array of the world locations of the box's vertices. */
	UFUNCTION(BlueprintPure, Category = LockOnTarget)
	static void GetBoxVertices(const FVector& BoxOrigin, const FVector& BoxExtents, TArray<FVector>& Verices);

	/**
	 * Determines if the location is on the screen.
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

	/** Get the angle between two directions. */
	static float GetAngleDeg(const FVector& Btw1, const FVector& Btw2);

	/** Get the trigonometric angle in 2D space (between [0,360) degrees) compared to the (1.f,0.f) Vector (trigonometric 0.f). */
	static float GetTrigonometricAngle2D(const FVector2D& Vector);

	/** Get the trigonometric angle in 3D space (between [0,360) degrees) with the projection a point onto a plane. */
	static float GetTrigonometricAngle3D(const FVector& Target, const FVector& OriginPointOnPlane, const FRotator& Rotation);

	static bool IsAngleInRange(float TargetAngle, float BaseAngle, float Range);
};
