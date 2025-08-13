/**
 * @file MeteorRainActor.h
 * @brief 陨石雨生成Actor头文件
 * @details 在场景中持续生成陨石, 不再进行敌军检测。
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
     * @brief 生成单个陨石
     */
    void SpawnSingleMeteor();

private:
    /** 陨石生成间隔 */
    UPROPERTY(EditAnywhere, Category="Meteor")
    float MeteorInterval = 0.3f;

    /** 陨石Actor类 */
    UPROPERTY(EditAnywhere, Category="Meteor")
    TSubclassOf<AMeteorProjectile> MeteorClass;

    /** 定时器句柄 */
    FTimerHandle SpawnTimer;

    /** 施法者队伍ID */
    int32 OwnerTeam = 0;
};

