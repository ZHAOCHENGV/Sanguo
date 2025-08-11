// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "VectorTypes.h"
#include "NavCorridor.h"
#include "Math/MathFwd.h"

// radian
constexpr auto RAD_1 = 0.01745f;
constexpr auto RAD_2 = 0.03490f;
constexpr auto RAD_3 = 0.05235f;
constexpr auto RAD_5 = 0.08726f;
constexpr auto RAD_10 = 0.17453f;
constexpr auto RAD_20 = 0.34906f;
constexpr auto RAD_30 = 0.52359f;
constexpr auto RAD_45 = 0.78539f;
constexpr auto RAD_70 = 1.22173f;
constexpr auto RAD_75 = 1.309f;
constexpr auto RAD_80 = 1.39626f;
constexpr auto RAD_85 = 1.48353f;
constexpr auto RAD_90 = 1.57079f;
constexpr auto RAD_270 = 4.71238f;
constexpr auto RAD_360 = 6.28318f;

// convert radian to degree
constexpr auto RAD_TO_DEG = 57.29582f;

template <typename T>
struct ANT_API FAntSegment
{
	T Start;
	T End;
};

struct ANT_API FAntRay
{
	FVector2f Start;
	FVector2f Dir;
};

struct ANT_API FAntCorridorLocation
{
	/** Location on the path */
	FVector NavLocation;

	/** Index of the start portal in the section where the location lies. */
	int32 PortalIndex = INDEX_NONE;

	/** Interpolation value representing the point between PortalIndex and PortalIndex+1 */
	float VertT = 0.0f;

	/** Interpolation value representing the point between PortalIndex and PortalIndex+1 */
	float HoriT = 0.0f;
};

/** Light-weight version of USplineComponent class with usefull functions. */
struct ANT_API FAntSplineCurve 
{
	/** Spline built from position data. */
	FInterpCurveVector Positions;

	/** Spline built from rotation data. */
	FInterpCurveQuat Rotations;

	/** Input: distance along curve, output: parameter that puts you there. */
	FInterpCurveFloat ReparamTable;

	int32 StepsPerSegment = 10;
	
	bool operator==(const FAntSplineCurve& Other) const
	{
		return Positions == Other.Positions;
	}

	bool operator!=(const FAntSplineCurve& Other) const
	{
		return !(*this == Other);
	}

	/** 
	 * Update the spline's internal data according to the passed-in params 
	 * @param	bClosedLoop				Whether the spline is to be considered as a closed loop.
	 * @param	bStationaryEndpoints	Whether the endpoints of the spline are considered stationary when traversing the spline at non-constant velocity.  Essentially this sets the endpoints' tangents to zero vectors.
	 * @param	ReparamStepsPerSegment	Number of steps per spline segment to place in the reparameterization table
	 * @param	bLoopPositionOverride	Whether to override the loop position with LoopPosition
	 * @param	LoopPosition			The loop position to use instead of the last key
	 * @param	Scale3D					The world scale to override
	 */
	void UpdateSpline(bool bClosedLoop = false, bool bStationaryEndpoints = false, int32 ReparamStepsPerSegment = 10, bool bLoopPositionOverride = false, float LoopPosition = 0.0f);

	/** Returns the length of the specified spline segment up to the parametric value given */
	float GetSegmentLength(const int32 Index, const float Param, bool bClosedLoop = false) const;

	/** Adds a point to the spline */
	void AddSplinePoint(const FVector& Positions, bool bUpdateSpline = true);

	/** Returns total length along this spline */
	float GetSplineLength() const;

	/** Get location along spline at the provided input key value */
	FVector GetLocationAtSplineInputKey(float InKey) const;

	/** Given a distance along the length of this spline, return the point in space where this puts you */
	FVector GetLocationAtDistanceAlongSpline(float Distance) const;

	/** Get the distance along the spline at the spline point */
	int32 GetNumberOfSplineSegments() const;

	/** Get the distance along the spline at the spline point */
	float GetDistanceAlongSplineAtSplinePoint(int32 PointIndex) const;

	/** Given a threshold, returns a list of vertices along the spline segment that, treated as a list of segments (polyline), matches the spline shape. */
	bool ConvertSplineSegmentToPolyLine(int32 SplinePointStartIndex, const float MaxSquareDistanceFromSpline, TArray<FVector>& OutPoints) const;

	/** Given a threshold, returns a list of vertices along the spline that, treated as a list of segments (polyline), matches the spline shape. */
	bool ConvertSplineToPolyLine(const float MaxSquareDistanceFromSpline, TArray<FVector> &OutPoints) const;

private:
	// Internal helper function called by ConvertSplineSegmentToPolyLine -- assumes the input is within a half-segment, so testing the distance to midpoint will be an accurate guide to subdivision
	bool DivideSplineIntoPolylineRecursiveWithDistancesHelper(float StartDistanceAlongSpline, float EndDistanceAlongSpline, const float MaxSquareDistanceFromSpline, TArray<FVector> &OutPoints, TArray<double> &OutDistancesAlongSpline) const;

	// Internal helper function called by ConvertSplineSegmentToPolyLine -- assumes the input is within a half-segment, so testing the distance to midpoint will be an accurate guide to subdivision
	bool DivideSplineIntoPolylineRecursiveHelper(float StartDistanceAlongSpline, float EndDistanceAlongSpline, const float MaxSquareDistanceFromSpline, TArray<FVector> &OutPoints) const;
};

class ANT_API FAntMath
{
public:
	static float TriangularNumber(int32 TriangleRow)
	{
		return (TriangleRow * (TriangleRow + 1)) / 2;
	}

	static float Determinant(const FVector2f &V1, const FVector2f &V2)
	{
		return V1.X * V2.Y - V1.Y * V2.X;
	}

	static FVector2f RotateCCW90(const FVector2f &Vec);

	// checking if Target direction is inside Start and End directions. 
	// note: start to end order is clock-wise.
	// return -1 : not inside
	// return 0 : zero input dir
	// return 1 : is inside
	static int8 IsInsideCW(const FVector2f &StartDir, const FVector2f &EndDir, const FVector2f &Target);

	// return true if the given point is left side of the given line segment
	static bool IsPointInLeftSide(const FAntSegment<FVector2f> &Segment, const FVector2f &Cylinder);

	static float WrapRadiansAngle(float Angle);

	// check within range in radians
	static bool IsWithinRange(float Target, float Start, float End);

	// return line segment slope in radiance.
	// multiple it to RAD_TO_DEG if you need degree
	static float SegmentSlope(const FAntSegment<FVector2f> &Segment);

	// ret == 0 : cross
	// ret > 0 : collided
	// ret < 0 : not collided
	static float CirclePoint(const FVector2f &Base, float Radius, const FVector2f &Cylinder);

	// Axis is vertical and horizontal radius
	// return < 1 : inside
	// return == 0 : on the edge
	// return > 1 : outside
	static int EllipsePoint(const FVector2f &Base, const FVector2f &Axis, const FVector2f &Cylinder);

	static bool PolygonPoint(const TArray<FVector2f> &Points, const FVector2f &HitPoint);

	static bool PolygonPoint(const FVector2f *Points, int ArraySize, const FVector2f &HitPoint);

	static bool PolygonCircle(const TArray<FVector2f> &Points, const FVector2f &CircleCenter, float CircleRadius);

	static bool PolygonSegment(const TArray<FVector2f> &Points, const FAntSegment<FVector2f> &Segment);

	// bounding box circles (circles with same radius)
	static FBox2f AABBCircles(const TArray<FVector2f> &Centers, float Radius);

	// faster version of the circleCircle which don't use sqrt
	// return overlapped squared distance
	// ret == 0 : cross
	// ret > 0 : collided
	// ret < 0 : not collided
	static float CircleCircleSq(const FVector2f &Center1, float Radius1, const FVector2f &Center2, float Radius2);

	// circle intersect using sqrt
	// return overlapped distance
	// ret == 0 : cross
	// ret > 0 : collided
	// ret < 0 : not collided
	static float CircleCircle(const FVector2f &Center1, float Radius1, const FVector2f &Center2, float Radius2);

	// Intersects ray r = p + td, |d| = 1, with sphere s and, if intersecting,
	// returns t value of intersection and intersection point q
	// OutTimeOfImpact 0: collision from inside
	// OutTimeOfImpact > 1: cross
	static bool CircleRay(const FVector2f &Base, float Radius, const FAntRay &Ray, float &OutTimeOfImpact);

	static bool CylinderSegment(const FAntSegment<FVector3f> &Cylinder, float Radius, const FAntSegment<FVector3f> &Segment, float &OutTimeOfImpact);

	// OutTimeOfImpact 0: collision from inside
	static bool CylinderSegment(const FVector2f &CylinderStart, const FVector2f &CylinderEnd, float Radius, const FAntSegment<FVector2f> &Segment, float &OutTimeOfImpact);

	static bool CapsuleRay(const FVector2f &CapsuleStart, const FVector2f &CapsuleEnd, float Radius, const FAntRay &Ray, float &OutTimeOfImpact);

	static bool SegmentSegment(const FAntSegment<FVector2f> &Segment1, const FAntSegment<FVector2f> &Segment2, float &T1, float &T2);

	// T length of the ray at the impact point
	static bool SegmentRay(const FAntSegment<FVector2f> &Segment, const FAntRay &Ray, float &T);

	static FVector2f ClosestPointSegment(const FAntSegment<FVector2f> &Segment, const FVector2f &Point, float &T);

	static FVector3f ClosestPointSegment(const FAntSegment<FVector3f> &Segment, const FVector3f &Point, float &T);

	// OutPoint1: closest point on line 1
	// OutPoint2: closest point on line 2
	// return squared distance between tow points
	static float ClosestPointSegmentSegment(const FAntSegment<FVector2f> &Segment1, const FAntSegment<FVector2f> &Segment2, float &T1, float &T2, FVector2f &OutPoint1, FVector2f &OutPoint2);

	static FVector2f ClosestPointPolygon(const TArray<TPair<FVector2f, FVector2f>> &PolySegments, const FVector2f &Cylinder);

	static FVector2f ClosestPointPolygon(const TArray<FVector2f> &Verts, const FVector2f &Cylinder);

	// OutPoint1: closest point on polygon
	// OutPoint2: closest point on line
	static void ClosestPointSegmentPolygon(const TArray<FVector2f> &Verts, const FAntSegment<FVector2f> &Segment, FVector2f &OutPoint1, FVector2f &OutPoint2);

	// any polygon with any order
	static bool RectPolygon(const FBox2f &Rect, const TArray<FVector2f> &Polygon);

	static bool RectSegment(const FBox2f &Rect, const FAntSegment<FVector2f> &Segment);

	static bool RectCircle(const FBox2f &Rect, const FVector2f &CircleCenter, float CircleRadius);

	static bool TrianglePoint(const FVector2f &TriA, const FVector2f &TriB, const FVector2f &TriC, const FVector2f &Cylinder);

	static float PointHeightOnTriangle(const FVector3f &TriA, const FVector3f &TriB, const FVector3f &TriC, const FVector2f &Cylinder);

	// sort and merge line segments.
	// all segments must be part of an circular shape, but multiple circular shape is allowed.
	// if MakeConnection passed true, we will connect end of the last segment to the start of the new relevant segment.
	// MaxDistance is maximum distance between two relevant segment.
	// InLines will be cleared during process.
	// OutLines will not be cleared.
	static void SortLineSegments(TArray<TPair<FVector2f, FVector2f>> &InLines, TArray<TPair<FVector2f, FVector2f>> &OutLines, float MaxDistance, bool MakeConnection);

	static unsigned int GetUniqueNumber();

	static FVector2f RadToNorm(float Angle);

	static float Angle(const FVector2f &A, const FVector2f &B);
};
