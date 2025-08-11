// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "AntMath.h"
#include "VectorTypes.h"
#include "Math/Box2D.h"
#include "Math/InterpCurve.h"
#include <algorithm>

void FAntSplineCurve::UpdateSpline(bool bClosedLoop, bool bStationaryEndpoints, int32 ReparamStepsPerSegment, bool bLoopPositionOverride, float LoopPosition)
{
	const int32 NumPoints = Positions.Points.Num();
	check(Rotations.Points.Num() == NumPoints);

#if DO_CHECK
	// Ensure input keys are strictly ascending
	for (int32 Index = 1; Index < NumPoints; Index++)
	{
		ensureAlways(Positions.Points[Index - 1].InVal < Positions.Points[Index].InVal);
	}
#endif

	// Ensure splines' looping status matches with that of the spline component
	if (bClosedLoop)
	{
		const float LastKey = Positions.Points.Num() > 0 ? Positions.Points.Last().InVal : 0.0f;
		const float LoopKey = bLoopPositionOverride ? LoopPosition : LastKey + 1.0f;
		Positions.SetLoopKey(LoopKey);
		Rotations.SetLoopKey(LoopKey);
	}
	else
	{
		Positions.ClearLoopKey();
		Rotations.ClearLoopKey();
	}

	// Automatically set the tangents on any CurveAuto keys
	Positions.AutoSetTangents(0.0f, bStationaryEndpoints);
	Rotations.AutoSetTangents(0.0f, bStationaryEndpoints);

	// Now initialize the spline reparam table
	const int32 NumSegments = bClosedLoop ? NumPoints : FMath::Max(0, NumPoints - 1);
	StepsPerSegment = ReparamStepsPerSegment;

	// Start by clearing it
	ReparamTable.Points.Reset(NumSegments * ReparamStepsPerSegment + 1);
	float AccumulatedLength = 0.0f;
	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		for (int32 Step = 0; Step < ReparamStepsPerSegment; ++Step)
		{
			const float Param = static_cast<float>(Step) / ReparamStepsPerSegment;
			const float SegmentLength = (Step == 0) ? 0.0f : GetSegmentLength(SegmentIndex, Param, bClosedLoop);

			ReparamTable.Points.Emplace(SegmentLength + AccumulatedLength, SegmentIndex + Param, 0.0f, 0.0f, CIM_Linear);
		}
		AccumulatedLength += GetSegmentLength(SegmentIndex, 1.0f, bClosedLoop);
	}

	ReparamTable.Points.Emplace(AccumulatedLength, static_cast<float>(NumSegments), 0.0f, 0.0f, CIM_Linear);
}

float FAntSplineCurve::GetSegmentLength(const int32 Index, const float Param, bool bClosedLoop) const
{
	const int32 NumPoints = Positions.Points.Num();
	const int32 LastPoint = NumPoints - 1;

	check(Index >= 0 && (!bClosedLoop && Index < LastPoint));
	check(Param >= 0.0f && Param <= 1.0f);

	// Evaluate the length of a Hermite spline segment.
	// This calculates the integral of |dP/dt| dt, where P(t) is the spline equation with components (x(t), y(t), z(t)).
	// This isn't solvable analytically, so we use a numerical method (Legendre-Gauss quadrature) which performs very well
	// with functions of this type, even with very few samples.  In this case, just 5 samples is sufficient to yield a
	// reasonable result.

	struct FLegendreGaussCoefficient
	{
		float Abscissa;
		float Weight;
	};

	static const FLegendreGaussCoefficient LegendreGaussCoefficients[] =
	{
		{ 0.0f, 0.5688889f },
		{ -0.5384693f, 0.47862867f },
		{ 0.5384693f, 0.47862867f },
		{ -0.90617985f, 0.23692688f },
		{ 0.90617985f, 0.23692688f }
	};

	const auto &StartPoint = Positions.Points[Index];
	const auto &EndPoint = Positions.Points[Index == LastPoint ? 0 : Index + 1];

	const auto &P0 = StartPoint.OutVal;
	const auto &T0 = StartPoint.LeaveTangent;
	const auto &P1 = EndPoint.OutVal;
	const auto &T1 = EndPoint.ArriveTangent;

	// Special cases for linear or constant segments
	if (StartPoint.InterpMode == CIM_Linear)
	{
		return (P1 - P0).Size() * Param;
	}
	else if (StartPoint.InterpMode == CIM_Constant)
	{
		// Special case: constant interpolation acts like distance = 0 for all p in [0, 1[ but for p == 1, the distance returned is the linear distance between start and end
		return Param == 1.f ? (P1 - P0).Size() : 0.0f;
	}

	// Cache the coefficients to be fed into the function to calculate the spline derivative at each sample point as they are constant.
	const FVector Coeff1 = ((P0 - P1) * 2.0f + T0 + T1) * 3.0f;
	const FVector Coeff2 = (P1 - P0) * 6.0f - T0 * 4.0f - T1 * 2.0f;
	const FVector Coeff3 = T0;

	const float HalfParam = Param * 0.5f;

	float Length = 0.0f;
	for (const auto &LegendreGaussCoefficient : LegendreGaussCoefficients)
	{
		// Calculate derivative at each Legendre-Gauss sample, and perform a weighted sum
		const float Alpha = HalfParam * (1.0f + LegendreGaussCoefficient.Abscissa);
		const FVector Derivative = ((Coeff1 * Alpha + Coeff2) * Alpha + Coeff3);
		Length += Derivative.Size() * LegendreGaussCoefficient.Weight;
	}
	Length *= HalfParam;

	return Length;
}

void FAntSplineCurve::AddSplinePoint(const FVector &Position, bool bUpdateSpline)
{
	// Add the spline point at the end of the array, adding 1.0 to the current last input key.
	// This continues the former behavior in which spline points had to be separated by an interval of 1.0.
	const float InKey = Positions.Points.Num() ? Positions.Points.Last().InVal + 1.0f : 0.0f;

	Positions.Points.Emplace(InKey, Position, FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
	Rotations.Points.Emplace(InKey, FQuat::Identity, FQuat::Identity, FQuat::Identity, CIM_CurveAuto);

	if (bUpdateSpline)
		UpdateSpline();
}

float FAntSplineCurve::GetSplineLength() const
{
	const int32 NumPoints = ReparamTable.Points.Num();

	// This is given by the input of the last entry in the remap table
	if (NumPoints > 0)
		return ReparamTable.Points.Last().InVal;

	return 0.0f;
}

FVector FAntSplineCurve::GetLocationAtSplineInputKey(float InKey) const
{
	FVector NavLocation = Positions.Eval(InKey, FVector::ZeroVector);
	return NavLocation;
}

FVector FAntSplineCurve::GetLocationAtDistanceAlongSpline(float Distance) const
{
	const float Param = ReparamTable.Eval(Distance, 0.0f);
	return GetLocationAtSplineInputKey(Param);
}

int32 FAntSplineCurve::GetNumberOfSplineSegments() const
{
	const int32 NumPoints = Positions.Points.Num();
	return FMath::Max(0, NumPoints - 1);
}

float FAntSplineCurve::GetDistanceAlongSplineAtSplinePoint(int32 PointIndex) const
{
	const int32 NumPoints = Positions.Points.Num();
	const int32 NumSegments = NumPoints - 1;

	// Ensure that if the reparam table is not prepared yet we don't attempt to access it. This can happen
	// early in the construction of the spline component object.
	if ((PointIndex >= 0) && (PointIndex < NumSegments + 1) && ((PointIndex * StepsPerSegment) < ReparamTable.Points.Num()))
	{
		return ReparamTable.Points[PointIndex * StepsPerSegment].InVal;
	}

	return 0.0f;
}

bool FAntSplineCurve::ConvertSplineSegmentToPolyLine(int32 SplinePointStartIndex, const float MaxSquareDistanceFromSpline, TArray<FVector> &OutPoints) const
{
	OutPoints.Empty();

	const double StartDist = GetDistanceAlongSplineAtSplinePoint(SplinePointStartIndex);
	const double StopDist = GetDistanceAlongSplineAtSplinePoint(SplinePointStartIndex + 1);

	const int32 NumLines = 2; // Dichotomic subdivision of the spline segment
	double Dist = StopDist - StartDist;
	double SubstepSize = Dist / NumLines;
	if (SubstepSize == 0.0)
	{
		// There is no distance to cover, so handle the segment with a single point
		OutPoints.Add(GetLocationAtDistanceAlongSpline(StopDist));
		return true;
	}

	double SubstepStartDist = StartDist;
	for (int32 i = 0; i < NumLines; ++i)
	{
		double SubstepEndDist = SubstepStartDist + SubstepSize;
		TArray<FVector> NewPoints;
		// Recursively sub-divide each segment until the requested precision is reached :
		if (DivideSplineIntoPolylineRecursiveHelper(SubstepStartDist, SubstepEndDist, MaxSquareDistanceFromSpline, NewPoints))
		{
			if (OutPoints.Num() > 0)
			{
				check(OutPoints.Last() == NewPoints[0]); // our last point must be the same as the new segment's first
				OutPoints.RemoveAt(OutPoints.Num() - 1);
			}
			OutPoints.Append(NewPoints);
		}

		SubstepStartDist = SubstepEndDist;
	}

	return (OutPoints.Num() > 0);
}

bool FAntSplineCurve::ConvertSplineToPolyLine(const float MaxSquareDistanceFromSpline, TArray<FVector> &OutPoints) const
{
	int32 NumSegments = GetNumberOfSplineSegments();
	OutPoints.Empty();
	OutPoints.Reserve(NumSegments * 2); // We sub-divide each segment in at least 2 sub-segments, so let's start with this amount of points

	TArray<FVector> SegmentPoints;
	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		if (ConvertSplineSegmentToPolyLine(SegmentIndex, MaxSquareDistanceFromSpline, SegmentPoints))
		{
			if (OutPoints.Num() > 0)
			{
				check(OutPoints.Last() == SegmentPoints[0]); // our last point must be the same as the new segment's first
				OutPoints.RemoveAt(OutPoints.Num() - 1);
			}
			OutPoints.Append(SegmentPoints);
		}
	}

	return (OutPoints.Num() > 0);
}

bool FAntSplineCurve::DivideSplineIntoPolylineRecursiveWithDistancesHelper(float StartDistanceAlongSpline, float EndDistanceAlongSpline, const float MaxSquareDistanceFromSpline, TArray<FVector> &OutPoints, TArray<double> &OutDistancesAlongSpline) const
{
	double Dist = EndDistanceAlongSpline - StartDistanceAlongSpline;
	if (Dist <= 0.0f)
	{
		return false;
	}
	double MiddlePointDistancAlongSpline = StartDistanceAlongSpline + Dist / 2.0f;
	FVector Samples[3];
	Samples[0] = GetLocationAtDistanceAlongSpline(StartDistanceAlongSpline);
	Samples[1] = GetLocationAtDistanceAlongSpline(MiddlePointDistancAlongSpline);
	Samples[2] = GetLocationAtDistanceAlongSpline(EndDistanceAlongSpline);

	if (FMath::PointDistToSegmentSquared(Samples[1], Samples[0], Samples[2]) > MaxSquareDistanceFromSpline)
	{
		TArray<FVector> NewPoints[2];
		TArray<double> NewDistancesAlongSpline[2];
		DivideSplineIntoPolylineRecursiveWithDistancesHelper(StartDistanceAlongSpline, MiddlePointDistancAlongSpline, MaxSquareDistanceFromSpline, NewPoints[0], NewDistancesAlongSpline[0]);
		DivideSplineIntoPolylineRecursiveWithDistancesHelper(MiddlePointDistancAlongSpline, EndDistanceAlongSpline, MaxSquareDistanceFromSpline, NewPoints[1], NewDistancesAlongSpline[1]);
		if ((NewPoints[0].Num() > 0) && (NewPoints[1].Num() > 0))
		{
			check(NewPoints[0].Last() == NewPoints[1][0]);
			check(NewDistancesAlongSpline[0].Last() == NewDistancesAlongSpline[1][0]);
			NewPoints[0].RemoveAt(NewPoints[0].Num() - 1);
			NewDistancesAlongSpline[0].RemoveAt(NewDistancesAlongSpline[0].Num() - 1);
		}
		NewPoints[0].Append(NewPoints[1]);
		NewDistancesAlongSpline[0].Append(NewDistancesAlongSpline[1]);
		OutPoints.Append(NewPoints[0]);
		OutDistancesAlongSpline.Append(NewDistancesAlongSpline[0]);
	}
	else
	{
		// The middle point is close enough to the other 2 points, let's keep those and stop the recursion :
		OutPoints.Add(Samples[0]);
		OutPoints.Add(Samples[2]);
		OutDistancesAlongSpline.Add(StartDistanceAlongSpline);
		OutDistancesAlongSpline.Add(EndDistanceAlongSpline);
	}

	check(OutPoints.Num() == OutDistancesAlongSpline.Num())
		return (OutPoints.Num() > 0);
}

bool FAntSplineCurve::DivideSplineIntoPolylineRecursiveHelper(float StartDistanceAlongSpline, float EndDistanceAlongSpline, const float MaxSquareDistanceFromSpline, TArray<FVector> &OutPoints) const
{
	TArray<double> DummyDistancesAlongSpline;
	return DivideSplineIntoPolylineRecursiveWithDistancesHelper(StartDistanceAlongSpline, EndDistanceAlongSpline, MaxSquareDistanceFromSpline, OutPoints, DummyDistancesAlongSpline);
}

int8 FAntMath::IsInsideCW(const FVector2f &StartDir, const FVector2f &EndDir, const FVector2f &Target)
{
	if (StartDir.IsZero() || EndDir.IsZero() || Target.IsZero())
		return 0;

	const auto crsStartEnd = FVector2f::CrossProduct(StartDir, EndDir);
	const auto crsStartTarget = FVector2f::CrossProduct(StartDir, Target);
	const auto crsTargetEnd = FVector2f::CrossProduct(Target, EndDir);

	// 0..180 range
	if (crsStartEnd >= 0)
		return (crsStartTarget >= 0 && crsTargetEnd >= 0) ? 1 : -1;

	// 180..360 range
	return (crsStartTarget >= 0 || crsTargetEnd >= 0) ? 1 : -1;
}

bool FAntMath::IsPointInLeftSide(const FAntSegment<FVector2f> &Segment, const FVector2f &Cylinder)
{
	return (Segment.End.X - Segment.Start.X) * (Cylinder.Y - Segment.Start.Y) - (Segment.End.Y - Segment.Start.Y) * (Cylinder.X - Segment.Start.X) > 0.0f;
}

float FAntMath::WrapRadiansAngle(float Angle)
{
	Angle = fmodf(Angle, RAD_360);
	return Angle += Angle < 0.0f ? RAD_360 : 0.0f;
}

bool FAntMath::IsWithinRange(float Target, float Start, float End)
{
	Start -= Target;
	End -= Target;
	return (Start * End >= 0) ? false : fabs(Start - End) < RAD_360;
}

float FAntMath::SegmentSlope(const FAntSegment<FVector2f> &Segment)
{
	const float tanx = (Segment.End.Y - Segment.Start.Y) / (Segment.End.X - Segment.Start.X);
	const float s = atan(tanx);
	return s;
}

float FAntMath::CirclePoint(const FVector2f &Base, float Radius, const FVector2f &Cylinder)
{
	return (Radius * Radius) - FVector2f::DistSquared(Base, Cylinder);
}

int FAntMath::EllipsePoint(const FVector2f &Base, const FVector2f &Axis, const FVector2f &Cylinder)
{
	return Axis.IsZero() ? 2 : (FMath::Pow((Cylinder.X - Base.X), 2.0) / FMath::Pow(Axis.X, 2.0)) + (FMath::Pow((Cylinder.Y - Base.Y), 2.0) / FMath::Pow(Axis.Y, 2.0));
}

bool FAntMath::PolygonPoint(const TArray<FVector2f> &Points, const FVector2f &HitPoint)
{
	if (Points.Num() < 3)
		return false;

	int i = 0, j = 0;
	bool c = false;
	for (i = 0, j = Points.Num() - 1; i < Points.Num(); j = i++)
	{
		if (((Points[i].Y > HitPoint.Y) != (Points[j].Y > HitPoint.Y)) &&
			(HitPoint.X < (Points[j].X - Points[i].X) * (HitPoint.Y - Points[i].Y) / (Points[j].Y - Points[i].Y)
				+ Points[i].X))
			c = !c;
	}
	return c;
}

bool FAntMath::PolygonPoint(const FVector2f *Points, int ArraySize, const FVector2f &HitPoint)
{
	if (ArraySize < 3 || !Points)
		return false;

	int i = 0, j = 0;
	bool c = false;
	for (i = 0, j = ArraySize - 1; i < ArraySize; j = i++)
	{
		if (((Points[i].Y > HitPoint.Y) != (Points[j].Y > HitPoint.Y)) &&
			(HitPoint.X < (Points[j].X - Points[i].X) * (HitPoint.Y - Points[i].Y) / (Points[j].Y - Points[i].Y)
				+ Points[i].X))
			c = !c;
	}
	return c;
}

bool FAntMath::PolygonCircle(const TArray<FVector2f> &Points, const FVector2f &CircleCenter, float CircleRadius)
{
	// check each point of the polygon intersecting cyrcle
	for (const auto &it : Points)
		if (CirclePoint(CircleCenter, CircleRadius, it) > 0)
			return true;

	// in case of circle inside polygon
	if (PolygonPoint(Points, CircleCenter))
		return true;

	// check each line segment of the polygon intersecting circle
	float toi = 0.0f;
	for (int32 Idx = 0; Idx < Points.Num() - 1; ++Idx)
	{
		const auto len = (Points[Idx + 1] - Points[Idx]).Size();
		if (len == 0.0f)
			continue;

		const auto dir = (Points[Idx + 1] - Points[Idx]) / len;
		if (CircleRay(CircleCenter, CircleRadius, { Points[Idx], dir }, toi) && toi < len)
			return true;
	}

	return false;
}

bool FAntMath::PolygonSegment(const TArray<FVector2f> &Points, const FAntSegment<FVector2f> &Segment)
{
	// check each polygon segment against line segment
	float t1, t2;
	for (int32 Idx = 0; Idx < Points.Num() - 1; ++Idx)
		if (SegmentSegment({ Points[Idx], Points[Idx + 1] }, Segment, t1, t2))
			return true;

	// check start and end of the line segment
	return (PolygonPoint(Points, Segment.Start) || PolygonPoint(Points, Segment.End));
}

FBox2f FAntMath::AABBCircles(const TArray<FVector2f> &Centers, float Radius)
{
	if (Centers.IsEmpty())
		return FBox2f();

	float maxX = Centers[0].X,
		maxY = Centers[0].Y,
		minX = Centers[0].X,
		minY = Centers[0].Y;

	for (const auto &it : Centers)
	{
		if (maxX < it.X) maxX = it.X;
		if (maxY < it.Y) maxY = it.Y;
		if (minX > it.X) minX = it.X;
		if (minY > it.Y) minY = it.Y;
	}

	return FBox2f({ minX - Radius, minY - Radius }, { (maxX + Radius), (maxY + Radius) });
}

float FAntMath::CircleCircleSq(const FVector2f &Center1, float Radius1, const FVector2f &Center2, float Radius2)
{
	return (Radius1 + Radius2) * (Radius1 + Radius2) - FVector2f::DistSquared(Center1, Center2);
}

float FAntMath::CircleCircle(const FVector2f &Center1, float Radius1, const FVector2f &Center2, float Radius2)
{
	return (Radius1 + Radius2) - FVector2f::Distance(Center1, Center2);
}

bool FAntMath::CircleRay(const FVector2f &Base, float Radius, const FAntRay &Ray, float &OutTimeOfImpact)
{
	const FVector2f m = Ray.Start - Base;
	const float b = m.Dot(Ray.Dir);
	const float c = m.Dot(m) - Radius * Radius;

	// Exit if cross(r, s) origin outside s (c > 0) and r pointing away from s (b > 0)
	if (c > 0.0f && b > 0.0f)
		return false;

	// A negative discriminant corresponds to ray missing sphere
	float discr = b * b - c;
	if (discr < 0.0f)
		return false;

	// Ray now found to intersect sphere, compute smallest t value of intersection
	OutTimeOfImpact = -b - FMath::Sqrt(discr);

	// If t is negative, ray started inside sphere so clamp t to zero
	if (OutTimeOfImpact < 0.0f)
		OutTimeOfImpact = 0.0f;

	return OutTimeOfImpact >= 0;
}

bool FAntMath::CylinderSegment(const FAntSegment<FVector3f> &Cylinder, float Radius, const FAntSegment<FVector3f> &Segment, float &OutTimeOfImpact)
{
	const auto d = Cylinder.End - Cylinder.Start, m = Segment.Start - Cylinder.Start, n = Segment.End - Segment.Start;
	const float md = m.Dot(d);
	const float nd = n.Dot(d);
	const float dd = d.Dot(d);

	// Test if segment fully outside either endcap of cylinder
	if (md < 0.0f && md + nd < 0.0f) 
		return false; // Segment outside p side of cylinder

	if (md > dd && md + nd > dd) 
		return false; // Segment outside q side of cylinder

	const float nn = n.Dot(n);
	const float mn = m.Dot(n);
	const float a = dd * nn - nd * nd;
	const float k = m.Dot(m) - Radius * Radius;
	const float c = dd * k - md * md;
	if (FMath::Abs(a) < FLT_EPSILON) 
	{
		// Segment runs parallel to cylinder axis
		if (c > 0.0f) 
			return false; // a and thus the segment lie outside cylinder

		// Now known that segment intersects cylinder; figure out how it intersects
		if (md < 0.0f) 
			OutTimeOfImpact = -mn / nn; // Intersect segment against p endcap
		else if (md > dd) 
			OutTimeOfImpact = (nd - mn) / nn; // Intersect segment against q endcap
		else 
			OutTimeOfImpact = 0.0f; // a lies inside cylinder

		return true;
	}

	const float b = dd * mn - nd * md;
	const float discr = b * b - a * c;

	// No real roots; no intersection
	if (discr < 0.0f) 
		return false; 

	OutTimeOfImpact = (-b - FMath::Sqrt(discr)) / a;

	// Intersection lies outside segment
	if (OutTimeOfImpact < 0.0f || OutTimeOfImpact > 1.0f)
		return false; 

	if (md + OutTimeOfImpact * nd < 0.0f) 
	{
		// Intersection outside cylinder on p side
		if (nd <= 0.0f) 
			return false; // Segment pointing away from endcap

		OutTimeOfImpact = -md / nd;

		// Keep intersection if Dot(S(t) - p, S(t) - p) <= r.pow(2)
		return k + 2 * OutTimeOfImpact * (mn + OutTimeOfImpact * nn) <= 0.0f;
	}
	else if (md + OutTimeOfImpact * nd > dd) 
	{
		// Intersection outside cylinder on q side
		if (nd >= 0.0f) 
			return false; // Segment pointing away from endcap

		OutTimeOfImpact = (dd - md) / nd;
		// Keep intersection if Dot(S(t) - q, S(t) - q) <= r.pow(2)
		return k + dd - 2 * md + OutTimeOfImpact * (2 * (mn - nd) + OutTimeOfImpact * nn) <= 0.0f;
	}

	// Segment intersects cylinder between the endcaps; t is correct
	return true;
}

bool FAntMath::CylinderSegment(const FVector2f &CapStart, const FVector2f &CapEnd, float Radius, const FAntSegment<FVector2f> &Segment, float &OutTimeOfImpact)
{
	const FVector2f d = CapEnd - CapStart;
	const FVector2f m = Segment.Start - CapStart;
	const FVector2f n = Segment.End - Segment.Start;
	const float md = m.Dot(d);
	const float nd = n.Dot(d);
	const float dd = d.Dot(d);

	// Test if segment fully outside either endcap of cylinder
	if (md < 0.0f && md + nd < 0.0f)
		return false; // Segment outside 'CapStart' side of cylinder
	if (md > dd && md + nd > dd)
		return false; // Segment outside 'CapEnd' side of cylinder

	const float nn = n.Dot(n);
	const float mn = m.Dot(n);
	const float a = dd * nn - nd * nd;
	const float k = m.Dot(m) - Radius * Radius;
	const float c = dd * k - md * md;
	OutTimeOfImpact = 0.0f;
	if (FMath::Abs(a) < FLT_EPSILON) {
		// Segment runs parallel to cylinder axis
		if (c > 0.0f)
			return false; // 'RayStart' and thus the segment lie outside cylinder

		// Now known that segment intersects cylinder; figure out how it intersects
		if (md < 0.0f)
			OutTimeOfImpact = -mn / nn; // Intersect segment against 'CapStart' endcap
		else if (md > dd)
			OutTimeOfImpact = (nd - mn) / nn; // Intersect segment against 'CapEnd' endcap
		else
			OutTimeOfImpact = 0.0f; // 'RayStart' lies inside cylinder

		return true;
	}

	const float b = dd * mn - nd * md;
	const float discr = b * b - a * c;
	if (discr < 0.0f)
		return false; // No real roots; no intersection

	OutTimeOfImpact = (-b - FMath::Sqrt(discr)) / a;
	if (OutTimeOfImpact < 0.0f || OutTimeOfImpact > 1.0f)
		return false; // Intersection lies outside segment

	if (md + OutTimeOfImpact * nd < 0.0f) {
		// Intersection outside cylinder on 'CapStart' side
		if (nd <= 0.0f)
			return false; // Segment pointing away from endcap

		OutTimeOfImpact = -md / nd;

		// Keep intersection if Dot(S(t) - CapStart, S(TimeOfImpact) - CapStart) <= pow(Radius, 2)
		const auto intersect = k + 2 * OutTimeOfImpact * (mn + OutTimeOfImpact * nn) <= 0.0f;
		return intersect;
	}
	else if (md + OutTimeOfImpact * nd > dd) {
		// Intersection outside cylinder on 'CapEnd' side
		if (nd >= 0.0f)
			return false; // Segment pointing away from endcap

		OutTimeOfImpact = (dd - md) / nd;

		// Keep intersection if Dot(S(TimeOfImpact) - CapEnd, S(TimeOfImpact) - CapEnd) <= pow(Radius, 2)
		const auto intersect = k + dd - 2 * md + OutTimeOfImpact * (2 * (mn - nd) + OutTimeOfImpact * nn) <= 0.0f;
		return intersect;
	}

	// Segment intersects cylinder between the endcaps; TimeOfImpact is correct
	return true;
}

bool FAntMath::CapsuleRay(const FVector2f &CapsuleStart, const FVector2f &CapsuleEnd, float Radius, const FAntRay &Ray, float &OutTimeOfImpact)
{
	// check ray agaist 2 circle at the start and end of the capsue
	float t = 1.0f;
	OutTimeOfImpact = 1.f;

	const auto c1 = CircleRay(CapsuleStart, Radius, Ray, t);
	OutTimeOfImpact = c1 ? t : OutTimeOfImpact;

	const auto c2 = CircleRay(CapsuleEnd, Radius, Ray, t);
	OutTimeOfImpact = c2 && (!c1 || t < OutTimeOfImpact) ? t : OutTimeOfImpact;

	// check ray agianst top and bottom side of the capsue
	const auto capNorm = (CapsuleEnd - CapsuleStart).GetSafeNormal();
	const FVector2f ccw(capNorm.Y, -capNorm.X);
	const FVector2f cw(-capNorm.Y, capNorm.X);

	// top side lime segment
	const FVector2f topSegStart(CapsuleStart + ccw * Radius);
	const FVector2f topSegEnd(CapsuleEnd + ccw * Radius);

	// bot side line segmnet
	const FVector2f botSegStart(CapsuleStart + cw * Radius);
	const FVector2f botSegEnd(CapsuleEnd + cw * Radius);

	t = 1;
	const auto l1 = SegmentRay({ topSegStart, topSegEnd }, Ray, t);
	OutTimeOfImpact = l1 && ((!c1 && !c2) || t < OutTimeOfImpact) ? t : OutTimeOfImpact;

	t = 1;
	const auto l2 = SegmentRay({ botSegStart, botSegEnd }, Ray, t);
	OutTimeOfImpact = l2 && ((!c1 && !c2 && !l1) || t < OutTimeOfImpact) ? t : OutTimeOfImpact;

	return (c1 || c2 || l1 || l2);
}

bool FAntMath::SegmentSegment(const FAntSegment<FVector2f> &Segment1, const FAntSegment<FVector2f> &Segment2, float &T1, float &T2)
{
	const float s1_x = Segment1.End.X - Segment1.Start.X;
	const float s1_y = Segment1.End.Y - Segment1.Start.Y;
	const float s2_x = Segment2.End.X - Segment2.Start.X;
	const float s2_y = Segment2.End.Y - Segment2.Start.Y;

	const float t1 = (-s1_y * (Segment1.Start.X - Segment2.Start.X) + s1_x * (Segment1.Start.Y - Segment2.Start.Y)) / (-s2_x * s1_y + s1_x * s2_y);
	const float t2 = (s2_x * (Segment1.Start.Y - Segment2.Start.Y) - s2_y * (Segment1.Start.X - Segment2.Start.X)) / (-s2_x * s1_y + s1_x * s2_y);
	T1 = t1;
	T2 = t2;

	// Collision detected
	if (t1 >= 0 && t1 <= 1 && t2 >= 0 && t2 <= 1)
		return true;

	return false; // No collision
}

bool FAntMath::SegmentRay(const FAntSegment<FVector2f> &Segment, const FAntRay &Ray, float &T)
{
	const auto v1 = Ray.Start - Segment.Start;
	const auto v2 = Segment.End - Segment.Start;
	const FVector2f v3(-Ray.Dir.Y, Ray.Dir.X);

	const auto dot = v2.Dot(v3);
	if (FMath::Abs(dot) < FLT_EPSILON)
		return false;

	T = FVector2f::CrossProduct(v2, v1) / dot;
	const auto t2 = v1.Dot(v3) / dot;

	if (T >= 0.0f && (t2 >= 0.0f && t2 <= 1.0f))
		return true;

	return false;
}

FVector2f FAntMath::ClosestPointSegment(const FAntSegment<FVector2f> &Segment, const FVector2f &Point, float &T)
{
	FVector2f result;
	FVector2f ab = Segment.End - Segment.Start;
	// Project c onto ab, but deferring divide by Dot(ab, ab)
	T = (Point - Segment.Start).Dot(ab);
	if (T <= 0.0f) {
		// c projects outside the [a,b] interval, on the a side; clamp to a
		T = 0.0f;
		result = Segment.Start;
	}
	else {
		float denom = ab.Dot(ab); // Always nonnegative 
		if (T >= denom) {
			// c projects outside the [a,b] interval, on the b side; clamp to b
			T = 1.0f;
			result = Segment.End;
		}
		else {
			// c projects inside the [a,b] interval; must do deferred divide now
			T = T / denom;
			result = Segment.Start + T * ab;
		}
	}

	return result;
}

FVector3f FAntMath::ClosestPointSegment(const FAntSegment<FVector3f> &Segment, const FVector3f &Point, float &T)
{
	FVector3f result;
	FVector3f ab = Segment.End - Segment.Start;
	// Project c onto ab, but deferring divide by Dot(ab, ab)
	T = (Point - Segment.Start).Dot(ab);
	if (T <= 0.0f) {
		// c projects outside the [a,b] interval, on the a side; clamp to a
		T = 0.0f;
		result = Segment.Start;
	}
	else {
		float denom = ab.Dot(ab); // Always nonnegative 
		if (T >= denom) {
			// c projects outside the [a,b] interval, on the b side; clamp to b
			T = 1.0f;
			result = Segment.End;
		}
		else {
			// c projects inside the [a,b] interval; must do deferred divide now
			T = T / denom;
			result = Segment.Start + T * ab;
		}
	}

	return result;
}

float FAntMath::ClosestPointSegmentSegment(const FAntSegment<FVector2f> &Segment1, const FAntSegment<FVector2f> &Segment2, float &T1, float &T2, FVector2f &OutPoint1, FVector2f &OutPoint2)
{
	FVector2f d1 = Segment1.End - Segment1.Start; // Direction vector of segment S1
	FVector2f d2 = Segment2.End - Segment2.Start; // Direction vector of segment S2
	FVector2f r = Segment1.Start - Segment2.Start;
	float a = d1.Dot(d1); // Squared length of segment S1, always nonnegative
	float e = d2.Dot(d2); // Squared length of segment S2, always nonnegative
	float f = d2.Dot(r);
	float t1, t2;
	// Check if either or both segments degenerate into points
	if (a <= FLT_EPSILON && e <= FLT_EPSILON)
	{
		// Both segments degenerate into points
		T1 = T2 = 0.0f;
		OutPoint1 = Segment1.Start;
		OutPoint2 = Segment2.Start;
		return (Segment1.Start - Segment2.Start).Dot(Segment1.Start - Segment2.Start);
	}
	if (a <= FLT_EPSILON)
	{
		// First segment degenerates into a point
		t1 = 0.0f;
		t2 = f / e; // s = 0 => t = (b*s + f) / e = f / e
		t2 = FMath::Clamp(t2, 0.0f, 1.0f);
	}
	else {
		float c = d1.Dot(r);
		if (e <= FLT_EPSILON)
		{
			// Second segment degenerates into a point
			t1 = 0.0f;
			t2 = FMath::Clamp(-c / a, 0.0f, 1.0f); // t = 0 => s = (b*t - c) / a = -c / a
		}
		else
		{
			// The general nondegenerate case starts here
			float b = d1.Dot(d2);
			float denom = a * e - b * b; // Always nonnegative
			// If segments not parallel, compute closest point on L1 to L2 and
			// clamp to segment S1. Else pick arbitrary s (here 0)
			if (denom != 0.0f)
			{
				t1 = FMath::Clamp((b * f - c * e) / denom, 0.0f, 1.0f);
			}
			else t1 = 0.0f;
			// Compute point on L2 closest to S1(s) using
			// t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
			t2 = (b * t1 + f) / e;
			// If t in [0,1] done. Else clamp t, recompute s for the new value
			// of t using s = Dot((P2 + D2*t) - P1,D1) / Dot(D1,D1)= (t*b - c) / a
			// and clamp s to [0, 1]
			if (t2 < 0.0f)
			{
				t2 = 0.0f;
				t1 = FMath::Clamp(-c / a, 0.0f, 1.0f);
			}
			else if (t2 > 1.0f)
			{
				t2 = 1.0f;
				t1 = FMath::Clamp((b - c) / a, 0.0f, 1.0f);
			}
		}
	}
	OutPoint1 = Segment1.Start + d1 * t1;
	OutPoint2 = Segment2.Start + d2 * t2;
	T1 = t1;
	T2 = t2;
	return (OutPoint1 - OutPoint2).Dot(OutPoint1 - OutPoint2);
}

FVector2f FAntMath::ClosestPointPolygon(const TArray<TPair<FVector2f, FVector2f>> &PolySegments, const FVector2f &Cylinder)
{
	// check intersection
	float t;
	auto closestPoint = ClosestPointSegment({ PolySegments[0].Key, PolySegments[0].Value }, Cylinder, t);
	auto minDist = FVector2f::DistSquared(closestPoint, Cylinder);
	for (const auto &it : PolySegments)
	{
		// check collision
		const auto tempPoint = ClosestPointSegment({ it.Key, it.Value }, Cylinder, t);
		const auto tempDist = FVector2f::DistSquared(tempPoint, Cylinder);
		if (tempDist < minDist)
		{
			closestPoint = tempPoint;
			minDist = tempDist;
		}
	}

	return closestPoint;
}

FVector2f FAntMath::ClosestPointPolygon(const TArray<FVector2f> &Verts, const FVector2f &Cylinder)
{
	if (Verts.Num() == 1)
		return Verts[0];

	float t;
	if (Verts.Num() == 2)
		return ClosestPointSegment({ Verts[0], Verts[1] }, Cylinder, t);

	// check intersection
	auto closestPoint = ClosestPointSegment({ Verts[0], Verts[1] }, Cylinder, t);
	auto minDist = FVector2f::DistSquared(closestPoint, Cylinder);
	for (int i = 1; i < Verts.Num(); ++i)
	{
		// line segment
		const auto &start = Verts[i];
		const auto &end = i + 1 == Verts.Num() ? Verts[0] : Verts[i + 1];

		// check collision
		const auto tempPoint = ClosestPointSegment({ start, end }, Cylinder, t);
		const auto tempDist = FVector2f::DistSquared(tempPoint, Cylinder);
		if (tempDist < minDist)
		{
			closestPoint = tempPoint;
			minDist = tempDist;
		}
	}

	return closestPoint;
}

void FAntMath::ClosestPointSegmentPolygon(const TArray<FVector2f> &Verts, const FAntSegment<FVector2f> &Segment, FVector2f &OutPoint1, FVector2f &OutPoint2)
{
	// line vs point
	if (Verts.Num() == 1)
	{
		float t;
		OutPoint1 = Verts[0];
		OutPoint2 = ClosestPointSegment(Segment, Verts.Last(), t);
		return;
	}

	// line vs line
	float t1, t2;
	if (Verts.Num() == 2)
	{
		ClosestPointSegmentSegment({ Verts[0], Verts.Last() }, Segment, t1, t2, OutPoint1, OutPoint2);
		return;
	}

	ClosestPointSegmentSegment({ Verts[0], Verts[1] }, Segment, t1, t2, OutPoint1, OutPoint2);
	auto minDist = FVector2f::DistSquared(OutPoint1, OutPoint2);
	for (int i = 1; i < Verts.Num(); ++i)
	{
		const auto start = Verts[i];
		const auto end = (i + 1) == Verts.Num() ? Verts[0] : Verts[i + 1];
		FVector2f out1, out2;
		ClosestPointSegmentSegment({ start, end }, Segment, t1, t2, out1, out2);
		const auto tempDist = FVector2f::DistSquared(out1, out2);
		if (tempDist < minDist)
		{
			OutPoint1 = out1;
			OutPoint2 = out2;
			minDist = tempDist;
		}
	}
}

bool FAntMath::RectPolygon(const FBox2f &Rect, const TArray<FVector2f> &Polygon)
{
	// check all 4 corners of our rectangle against polygon
	if (FAntMath::PolygonPoint(Polygon, { Rect.Min.X, Rect.Min.Y }))
		return true;
	if (FAntMath::PolygonPoint(Polygon, { Rect.Max.X, Rect.Min.Y }))
		return true;
	if (FAntMath::PolygonPoint(Polygon, { Rect.Min.X, Rect.Max.Y }))
		return true;
	if (FAntMath::PolygonPoint(Polygon, { Rect.Max.X, Rect.Max.Y }))
		return true;

	// check polygon corners against our rectangle
	for (auto &it : Polygon)
		if (Rect.IsInsideOrOn(it))
			return true;

	// check polygon lines against our rectangle lines
	float t;
	for (size_t i = 0; i < Polygon.Num(); ++i)
	{
		if (i < Polygon.Num() - 1)
		{
			// top
			if (SegmentSegment({ Polygon[i], Polygon[i + 1] }, { { Rect.Min.X, Rect.Min.Y }, { Rect.Max.X, Rect.Min.Y } }, t, t))
				return true;
			// left
			if (SegmentSegment({ Polygon[i], Polygon[i + 1] }, { { Rect.Min.X, Rect.Min.Y }, { Rect.Max.X, Rect.Min.Y } }, t, t))
				return true;
			// right
			if (SegmentSegment({ Polygon[i], Polygon[i + 1] }, { { Rect.Max.X, Rect.Min.Y }, { Rect.Max.X, Rect.Max.Y } }, t, t))
				return true;
			// bottom
			if (SegmentSegment({ Polygon[i], Polygon[i + 1] }, { { Rect.Min.X, Rect.Max.Y }, { Rect.Max.X, Rect.Max.Y } }, t, t))
				return true;
		}
	}

	return false;
}

bool FAntMath::RectSegment(const FBox2f &Rect, const FAntSegment<FVector2f> &Segment)
{
	// check all 4 corners of our rectangle against polygon
	if (Rect.IsInsideOrOn(Segment.Start))
		return true;
	if (Rect.IsInsideOrOn(Segment.End))
		return true;

	// check rectangle lines against line segment
	float t;
	// top
	if (SegmentSegment(Segment, { { Rect.Min.X, Rect.Min.Y }, { Rect.Max.X, Rect.Min.Y } }, t, t))
		return true;
	// left
	if (SegmentSegment(Segment, { { Rect.Min.X, Rect.Min.Y }, { Rect.Max.X, Rect.Min.Y } }, t, t))
		return true;
	// right
	if (SegmentSegment(Segment, { { Rect.Max.X, Rect.Min.Y }, { Rect.Max.X, Rect.Max.Y } }, t, t))
		return true;
	// bottom
	if (SegmentSegment(Segment, { { Rect.Min.X, Rect.Max.Y }, { Rect.Max.X, Rect.Max.Y } }, t, t))
		return true;

	return false;
}

bool FAntMath::RectCircle(const FBox2f &Rect, const FVector2f &CircleCenter, float CircleRadius)
{
	float testX = CircleCenter.X < Rect.Min.X ? Rect.Min.X : CircleCenter.X;
	testX = testX > Rect.Max.X ? Rect.Max.X : testX;

	float testY = CircleCenter.Y < Rect.Min.Y ? Rect.Min.Y : CircleCenter.Y;
	testY = testY > Rect.Max.Y ? Rect.Max.Y : testY;

	return ((CircleCenter.X - testX) * (CircleCenter.X - testX) + (CircleCenter.Y - testY) * (CircleCenter.Y - testY)) < CircleRadius * CircleRadius;
}

bool FAntMath::TrianglePoint(const FVector2f &TriA, const FVector2f &TriB, const FVector2f &TriC, const FVector2f &Cylinder)
{
	int32 as_x = Cylinder.X - TriA.X;
	int32 as_y = Cylinder.Y - TriA.Y;
	bool s_ab = (TriB.X - TriA.X) * as_y - (TriB.Y - TriA.Y) * as_x > 0;

	if ((TriC.X - TriA.X) * as_y - (TriC.Y - TriA.Y) * as_x > 0 == s_ab)
		return false;

	if ((TriC.X - TriB.X) * (Cylinder.Y - TriB.Y) - (TriC.Y - TriB.Y) * (Cylinder.X - TriB.X) > 0 != s_ab)
		return false;

	return true;
}

float FAntMath::PointHeightOnTriangle(const FVector3f &TriA, const FVector3f &TriB, const FVector3f &TriC, const FVector2f &Cylinder)
{
	float det = (TriB.Y - TriC.Y) * (TriA.X - TriC.X) + (TriC.X - TriB.X) * (TriA.Y - TriC.Y);

	float l1 = ((TriB.Y - TriC.Y) * (Cylinder.X - TriC.X) + (TriC.X - TriB.X) * (Cylinder.Y - TriC.Y)) / det;
	float l2 = ((TriC.Y - TriA.Y) * (Cylinder.X - TriC.X) + (TriA.X - TriC.X) * (Cylinder.Y - TriC.Y)) / det;
	float l3 = 1.0f - l1 - l2;

	return l1 * TriA.Z + l2 * TriB.Z + l3 * TriC.Z;
}

void FAntMath::SortLineSegments(TArray<TPair<FVector2f, FVector2f>> &InLines, TArray<TPair<FVector2f, FVector2f>> &OutLines, float MaxDistance, bool MakeConnection)
{
	// foreward list is relevant segments to the end of line
	TArray<TPair<FVector2f, FVector2f>> foreward;
	foreward.Reserve(InLines.Num());

	while (!InLines.IsEmpty())
	{
		// step 1: we pick a segment from list and put it to foreward/backward list
		if (foreward.IsEmpty())
		{
			foreward.Push(InLines.Last());
			InLines.Pop();
		}

		// step 2: searching for relevant segments to the picked segment in previuos step
		bool segmentFound = false;
		for (int i = 0; i < InLines.Num(); /* custom */)
		{
			// first to second distance
			auto dist = (foreward.Last().Value - InLines[i].Key).GetAbs();
			if (dist.X <= MaxDistance && dist.Y <= MaxDistance)
			{
				// add segment to forward list and remove it from list.
				foreward.Push(MakeConnection ? TPair<FVector2f, FVector2f>{ foreward.Last().Value, InLines[i].Key } : InLines[i]);
				InLines[i] = InLines.Last();
				InLines.Pop();
				segmentFound = true;
				continue;
			}

			// second to second distance
			dist = (foreward.Last().Value - InLines[i].Value).GetAbs();
			if (dist.X <= MaxDistance && dist.Y <= MaxDistance)
			{
				// add segment to foreard list and remove it from list
				foreward.Push(MakeConnection ? TPair<FVector2f, FVector2f>{ foreward.Last().Value, InLines[i].Key } : TPair<FVector2f, FVector2f>{ InLines[i].Value, InLines[i].Key });
				InLines[i] = InLines.Last();
				InLines.Pop();
				segmentFound = true;
				continue;
			}

			++i;
		}

		// store segment to the final list
		if (!segmentFound || InLines.IsEmpty())
		{
			// fix connection of the last point
			if (MakeConnection)
				foreward[0].Key = foreward.Last().Value;

			// add complete sorted shape to the final list
			OutLines.Insert(foreward, OutLines.Num());
			foreward.Empty();
		}
	}
}

unsigned int FAntMath::GetUniqueNumber()
{
	static unsigned int num = 0;
	return num++;
}

FVector2f FAntMath::RadToNorm(float Angle)
{
	return { cos(Angle), sin(Angle) };
}

float FAntMath::Angle(const FVector2f &A, const FVector2f &B)
{
	return FMath::Acos(FVector2f::DotProduct(A, B));
}

