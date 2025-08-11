#pragma once

#include "CoreMinimal.h"
#include "SubjectHandle.h"
#include "Curves/CurveFloat.h"
#include "BattleFrameStructs.h"
#include "Traits/Trace.h"
#include "ProjectileConfig.generated.h" 

class UNeighborGridComponent;
class UProjectileConfigDataAsset;

UENUM(BlueprintType)
enum class EProjectileSolveMode : uint8
{
	FromPitch UMETA(DisplayName = "FromPitch", Tooltip = ""),
	FromSpeed UMETA(DisplayName = "FromSpeed", Tooltip = ""),
};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileParamsRT_Ballistic
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	float ScaleMult = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	int32 Iterations = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	float Gravity = -1000;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	EProjectileSolveMode SolveMode = EProjectileSolveMode::FromPitch;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	float PitchAngle = 35;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	float Speed = 3000;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	bool bFavorHighArc = false;
};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileParamsRT_Interped
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	float ScaleMult = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	float Speed = 3000;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	float XYOffsetMult = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	float ZOffsetMult = 1;
};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileParamsRT_Tracking
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	float ScaleMult = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	float Speed = 3000;

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileParamsRT_Static
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	float ScaleMult = 1;

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileParamsRT_DA
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	TSoftObjectPtr<UProjectileConfigDataAsset> ProjectileConfig;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	FProjectileParamsRT_Ballistic Ballistic;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	FProjectileParamsRT_Interped Interped;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	FProjectileParamsRT_Tracking Tracking;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = ""))
	FProjectileParamsRT_Static Static;

};


USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileParams
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = "绘制调试图形"))
	bool bDrawDebugShape = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "碰撞检测半径"))
	float Radius = 100;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "剩余寿命"))
	float LifeSpan = 5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "血量，每发生一次碰撞，血量-1，归零后不再造成伤害"))
	int32 Health = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = "朝向运动方向"))
	bool bRotationFollowVelocity = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = "仅在抵达目标点后进行一次碰撞检测，可以节约性能"))
	bool bTraceOnlyOnArrival = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = "只对每个目标最多造成1次伤害。如果禁用，子弹与目标停止重叠后再重新发生碰撞就又可以造成伤害"))
	bool bHurtTargetOnlyOnce = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = "能否与环境碰撞"))
	bool bCheckObstacle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ToolTip = "碰撞条件"))
	FBFFilter Filter = FBFFilter();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = "寿命归零后销毁"))
	bool bRemoveOnNoLifeSpan = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = "血量归零后销毁"))
	bool bRemoveOnNoHealth = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = "抵达目标后销毁"))
	bool bRemoveOnArrival = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = "与环境碰撞后销毁"))
	bool bRemoveOnHitObstacle = true;

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileParamsRT
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = "伤害归属者"))
	FSubjectHandle Instigator = FSubjectHandle();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ToolTip = "忽略列表"))
	FSubjectArray IgnoreSubjects = FSubjectArray();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "在哪个邻居网格中检索目标, 不填会尝试自动获取关卡中第一个"))
	UNeighborGridComponent* NeighborGridComponent;

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileMove_Static
{
	GENERATED_BODY()

public:

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileMove_Interped
{
	GENERATED_BODY()

public:

	FProjectileMove_Interped()
	{
		InitializeCurve(XYOffset, { {0.0f, 0.0f}, {1.0f, 0.0f} });
		InitializeCurve(ZOffset, { {0.0f, 0.0f}, {1.0f, 0.0f} });
	}

private:

	void InitializeCurve(FRuntimeFloatCurve& Curve, const TArray<TPair<float, float>>& Keyframes)
	{
		Curve.GetRichCurve()->Reset();

		for (const auto& Keyframe : Keyframes)
		{
			FKeyHandle KeyHandle = Curve.GetRichCurve()->AddKey(Keyframe.Key, Keyframe.Value);
			Curve.GetRichCurve()->SetKeyInterpMode(KeyHandle, RCIM_Cubic);
			Curve.GetRichCurve()->SetKeyTangentMode(KeyHandle, RCTM_Auto);
		}
	}

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Curves, meta = (Tooltip = "XY平面位置偏移曲线"))
	FRuntimeFloatCurve XYOffset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Curves, meta = (Tooltip = "XY平面位置偏移乘数映射"))
	FVector4 XYScaleRangeMap = FVector4(0, 1, 10000, 1);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Curves, meta = (Tooltip = "Z轴向高度偏移曲线"))
	FRuntimeFloatCurve ZOffset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Curves, meta = (Tooltip = "Z轴向高度偏移乘数映射"))
	FVector4 ZScaleRangeMap = FVector4(0, 1, 10000, 1);

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileMoving_Interped
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "起点"))
	FVector FromPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "终点"))
	FVector ToPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "目标"))
	FSubjectHandle Target = FSubjectHandle();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "生成时间"))
	float BirthTime = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "速度"))
	float Speed = 2000;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = ""))
	float XYOffsetMult = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = ""))
	float ZOffsetMult = 1;

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileMove_Ballistic
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "最大速度"))
	float MaxSpeed = 5000;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "重力"))
	float Gravity = -980;

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileMoving_Ballistic
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "起点"))
	FVector FromPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "终点"))
	FVector ToPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "初速度"))
	FVector InitialVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "生成时间"))
	float BirthTime = 0;

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileMove_Tracking
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "加速度"))
	float Acceleration = 1000;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "最大速度"))
	float MaxSpeed = 5000;

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FProjectileMoving_Tracking
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "起点"))
	FVector FromPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "终点"))
	FVector ToPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "目标"))
	FSubjectHandle Target = FSubjectHandle();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "目标速度"))
	FVector TargetVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "自身速度"))
	FVector CurrentVelocity = FVector::ZeroVector;

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FIsAttachedFx
{
	GENERATED_BODY()

public:

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FIsBurstFx
{
	GENERATED_BODY()

public:

};

