// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#include "Utilities/LOTC_BPLibrary.h"
#include "Kismet/GameplayStatics.h"

bool ULOTC_BPLibrary::IsActorOnScreenComplex(AActor* Actor, APlayerController* PlayerController, bool bRenderCheck, const FVector2D& ScreenOffset)
{
#if LOC_INSIGHTS
	SCOPED_NAMED_EVENT(LOC_IsActorOnScreenComplex, FColor::Red);
#endif

	if (!IsValid(Actor) || !IsValid(PlayerController))
	{
		return true;
	}

	//For faster detecting if Actor doesn't rendered, then it definitely not on the viewport.
	//The following steps are necessary, because shadows require a rendered object, that maybe off screen.
	if (bRenderCheck && !Actor->WasRecentlyRendered(0.1f))
	{
		return false;
	}

	//Get bound vertices.
	FVector BoxOrigin, BoxExtents;
	Actor->GetActorBounds(true, BoxOrigin, BoxExtents);

	TArray<FVector> Vertices;
	Vertices.Reserve(9);
	Vertices.Add(Actor->GetActorLocation());
	GetBoxVertices(BoxOrigin, BoxExtents, Vertices);

	//Init max and min values for correct range overlapping.
	//Ranges helps to detect is Actor on screen, even if no one bound vertex isn't on screen.
	int32 XMax = INT32_MIN, YMax = INT32_MIN;
	int32 XMin = INT32_MAX, YMin = INT32_MAX;
	FVector2D ScreenLocation;
	const FVector2D ClampedScreenOffset = ScreenOffset.ClampAxes(0.f, 50.f);

	//Find max and min projected vertex on viewport axis values.
	for (const FVector& Elem : Vertices)
	{
		if (UGameplayStatics::ProjectWorldToScreen(PlayerController, Elem, ScreenLocation))
		{
			if (Is2DPointOnScreen(PlayerController, FIntPoint(ScreenLocation.X, ScreenLocation.Y), ClampedScreenOffset))
			{
				return true;
			}

			if (ScreenLocation.X < XMin)
			{
				XMin = ScreenLocation.X;
			}

			if (ScreenLocation.X > XMax)
			{
				XMax = ScreenLocation.X;
			}

			if (ScreenLocation.Y < YMin)
			{
				YMin = ScreenLocation.Y;
			}

			if (ScreenLocation.Y > YMax)
			{
				YMax = ScreenLocation.Y;
			}
		}
	}

	//Projected bound vertices to range.
	const TRange<int32> ProjectionX(XMin, XMax);
	const TRange<int32> ProjectionY(YMin, YMax);

	int32 ViewX, ViewY;
	PlayerController->GetViewportSize(ViewX, ViewY);

	//Viewport ranges from viewport size.
	const TRange<int32> ViewportXRange(0 + static_cast<int32>(ClampedScreenOffset.X * ViewX / 100.f), ViewX - static_cast<int32>(ClampedScreenOffset.X * ViewX / 100.f));
	const TRange<int32> ViewportYRange(0 + static_cast<int32>(ClampedScreenOffset.Y * ViewY / 100.f), ViewY - static_cast<int32>(ClampedScreenOffset.Y * ViewY / 100.f));

	//Check Ranges overlaps. If they don't overlap, then Actor is not on screen.
	return ViewportXRange.Overlaps(ProjectionX) && ViewportYRange.Overlaps(ProjectionY);
}

bool ULOTC_BPLibrary::IsActorOnScreenSimple(AActor* Actor, APlayerController* PC, bool bRenderCheck /* = true */, const FVector2D& ScreenOffset /* = {0.f, 0.f} */)
{
#if LOC_INSIGHTS
	SCOPED_NAMED_EVENT(LOC_IsActorOnScreenSimple, FColor::Green);
#endif

	if (!IsValid(Actor) || !IsValid(PC))
	{
		return false;
	}

	//For faster detecting if Actor doesn't rendered, then it definitely not on the viewport.
	if (bRenderCheck && !Actor->WasRecentlyRendered(0.1f))
	{
		return false;
	}

	return IsVectorOnScreen(PC, Actor->GetActorLocation(), ScreenOffset);
}

void ULOTC_BPLibrary::GetBoxVertices(const FVector& BoxOrigin, const FVector& BoxExtents, TArray<FVector>& Vertices)
{
	const FVector PointsMultiplier[8] =
		{
			{1, 1, 1},
			{1, 1, -1},
			{1, -1, 1},
			{1, -1, -1},
			{-1, 1, 1},
			{-1, 1, -1},
			{-1, -1, 1},
			{-1, -1, -1}
	};

	for (uint8 i = 0; i < 8; ++i)
	{
		Vertices.Add(PointsMultiplier[i] * BoxExtents + BoxOrigin);
	}
}

bool ULOTC_BPLibrary::Is2DPointOnScreen(APlayerController* PC, const FIntPoint& ScreenPosition, const FVector2D& ScreenOffset)
{
	int32 VX, VY;
	PC->GetViewportSize(VX, VY);
	const FVector2D ClampedScreenOffset = ScreenOffset.ClampAxes(0.f, 50.f);

	return ScreenPosition.X > (0 + static_cast<int32>(VX * ScreenOffset.X / 100.f)) && ScreenPosition.X < (VX - static_cast<int32>(VX * ScreenOffset.X / 100.f)) && ScreenPosition.Y > (0 + static_cast<int32>(VY * ScreenOffset.Y / 100.f)) && ScreenPosition.Y < (VY - static_cast<int32>(VY * ScreenOffset.Y / 100.f));
}

bool ULOTC_BPLibrary::IsVectorOnScreen(APlayerController* PC, const FVector& WorldLocation, const FVector2D& ScreenOffset)
{
	if (PC)
	{
		FVector2D ScreenLocation;

		if (PC->ProjectWorldLocationToScreen(WorldLocation, ScreenLocation))
		{
			return Is2DPointOnScreen(PC, FIntPoint(ScreenLocation.X, ScreenLocation.Y), ScreenOffset);
		}
	}

	return false;
}

float ULOTC_BPLibrary::GetAngle(const FVector& Btw1, const FVector& Btw2)
{
	return (180.f) / PI * FMath::Acos(Btw1.GetSafeNormal() | Btw2.GetSafeNormal());
}

float ULOTC_BPLibrary::GetTrigonometricAngle2D(const FVector2D& Vector)
{
	const FVector2D Vector1(1.f, 0.f);
	const FVector2D Vector2 = Vector.GetSafeNormal();
	float Result = (180.f) / PI * FMath::Acos(Vector1 | Vector2);

	if (Vector.Y < 0.f)
	{
		Result = 360.f - Result;
	}

	return Result;
}

float ULOTC_BPLibrary::GetTrigonometricAngle3D(const FVector& Target, const FVector& OriginPointOnPlane, const FRotator& Rotation)
{
	const FVector RightVector = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);
	const FVector ForwardVector = Rotation.Vector();
	const FVector UpVector = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z);

	const FVector ProjectedPoint = FVector::PointPlaneProject(Target, OriginPointOnPlane, ForwardVector);
	const FVector DirVect = (ProjectedPoint - OriginPointOnPlane).GetSafeNormal();
	float Angle = GetAngle(RightVector, DirVect);

	if ((UpVector | DirVect) <= 0)
	{
		Angle = 360.f - Angle;
	}

	return Angle;
}

bool ULOTC_BPLibrary::IsAngleInRange(float TargetAngle, float BaseAngle, float Range)
{
	return FMath::Abs(FMath::FindDeltaAngleDegrees(TargetAngle, BaseAngle)) < Range;
}
