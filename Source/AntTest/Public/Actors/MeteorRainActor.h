/**
 * @file MeteorRainActor.h
 * @brief 陨石雨生成Actor头文件
 * @details 持续检查范围内的敌对士兵, 在其上方生成陨石并造成范围伤害。
 */
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MeteorRainActor.generated.h"

class AMeteorProjectile;
class UBFSubjectiveAgentComponent;

/**
 * @brief 陨石雨生成器
 */
UCLASS()
class ANTTEST_API AMeteorRainActor : public AActor
{
    GENERATED_BODY()

public:
    AMeteorRainActor();

protected:
    virtual void BeginPlay() override;

    /**
     * @brief 定时检测敌人
     */
    void DetectEnemies();

    /**
     * @brief 对目标敌人开始生成陨石
     * @param Target 目标敌人
     */
    void StartMeteorRain(AActor* Target);

    /**
     * @brief 生成单个陨石
     * @param Target 目标敌人
     */
    void SpawnSingleMeteor(AActor* Target);

private:
    /** 检测范围 */
    UPROPERTY(EditAnywhere, Category="Meteor")
    float DetectRadius = 1000.f;

    /** 每个目标生成的陨石数量 */
    UPROPERTY(EditAnywhere, Category="Meteor")
    int32 MeteorCount = 3;

    /** 陨石生成间隔 */
    UPROPERTY(EditAnywhere, Category="Meteor")
    float MeteorInterval = 0.3f;

    /** 陨石Actor类 */
    UPROPERTY(EditAnywhere, Category="Meteor")
    TSubclassOf<AMeteorProjectile> MeteorClass;

    /** 定时器句柄 */
    FTimerHandle DetectTimer;

    /** 施法者队伍ID */
    int32 OwnerTeam = 0;
};

