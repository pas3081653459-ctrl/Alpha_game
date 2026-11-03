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

UENUM(BlueprintType)
enum class EAlphaStoryStage : uint8
{
    Arrival,
    Ready,
    Combat,
    Debrief,
    Complete
};

class AAlphaForgeBoss;

UCLASS()
class ALPHAGAME_API AAlphaGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Story")
    int32 ChapterIndex = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Story")
    EAlphaStoryStage StoryStage = EAlphaStoryStage::Arrival;
    /** Invalidates conversations across retries of the same chapter. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Story")
    int32 StoryRevision = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Route")
    bool bAwaitingRouteAdvance = false;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Route")
    FVector RouteObjective = FVector::ZeroVector;
    static FVector GetChapterOrigin(int32 Chapter);
    int32 GetChapterWaveCount() const;
    bool ChapterHasBoss() const;

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

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Party")
    int32 HumanPlayerCount = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Party")
    int32 CompanionCount = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Party")
    bool bCompanionFillEnabled = true;

    UFUNCTION(BlueprintPure, Category="Party")
    int32 GetPartyCount() const { return HumanPlayerCount + CompanionCount; }

    static constexpr int32 TotalWaves = 3;
private:
    void BuildPrototypeArena();
};
