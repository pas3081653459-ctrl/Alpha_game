#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "AlphaGameState.generated.h"

UENUM(BlueprintType)
enum class EAlphaRoundState : uint8
{
    Waiting,
    Fighting,
    Intermission,
    Victory,
    Defeat
};

UENUM(BlueprintType)
enum class EAlphaEncounterStage : uint8
{
    Preparation,
    Waves,
    Boss,
    Finished
};

class AAlphaForgeBoss;

UCLASS()
class ALPHAGAME_API AAlphaGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Round")
    int32 Wave = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Round")
    int32 RemainingEnemies = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Round")
    EAlphaRoundState RoundState = EAlphaRoundState::Waiting;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Encounter")
    EAlphaEncounterStage EncounterStage = EAlphaEncounterStage::Preparation;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Encounter")
    TObjectPtr<AAlphaForgeBoss> ForgeBoss;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Encounter")
    int32 BossPhase = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Encounter")
    int32 EncounterPlayerCount = 1;

    static constexpr int32 TotalWaves = 3;
private:
    void BuildPrototypeArena();
};
