/**
 * @file MeteorProjectile.h
 * @brief 陨石Actor头文件
 * @details 定义了简单的陨石Actor, 用于从空中下落并在落地时造成范围伤害。
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MeteorProjectile.generated.h"

class UBFSubjectiveAgentComponent;

/**
 * @brief 陨石Actor
 * @details 从生成点向下移动, 落地后对敌对队伍造成伤害。
 */
UCLASS()
class ANTTEST_API AMeteorProjectile : public AActor
{
    GENERATED_BODY()

public:
    AMeteorProjectile();

    /**
     * @brief 初始化陨石
     * @param InTargetLocation 落点位置
     * @param InInstigatorTeam 施法者队伍ID
     */
    void Init(const FVector& InTargetLocation, int32 InInstigatorTeam);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /**
     * @brief 处理陨石爆炸与伤害
     */
    void Explode();

private:
    /** 陨石下落速度 */
    UPROPERTY(EditDefaultsOnly, Category="Meteor")
    float FallSpeed = 1200.f;

    /** 伤害半径 */
    UPROPERTY(EditDefaultsOnly, Category="Meteor")
    float DamageRadius = 200.f;

    /** 造成的基础伤害 */
    UPROPERTY(EditDefaultsOnly, Category="Meteor")
    float Damage = 50.f;

    /** 预期落点 */
    FVector TargetLocation;

    /** 施法者队伍ID */
    int32 InstigatorTeam = 0;
};

