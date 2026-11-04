#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Rules/ProfileRules.h"
#include "AlphaProfileSave.generated.h"

/** Local preferences and chapter unlocks only; never an encounter/network snapshot. */
UCLASS()
class ALPHAGAME_API UAlphaProfileSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 SchemaVersion = 1;
    UPROPERTY(SaveGame) int32 HighestUnlockedChapter = 0;
    UPROPERTY(SaveGame) int32 LastChapter = 0;
    UPROPERTY(SaveGame) int32 PreferredHero = 0;
    UPROPERTY(SaveGame) float AudioVolume = .75f;
    UPROPERTY(SaveGame) float CameraShake = .35f;

    AlphaProfileRules::Data GetData() const;
    void SetData(const AlphaProfileRules::Data& Data);
    bool IsCompatible() const;
};
