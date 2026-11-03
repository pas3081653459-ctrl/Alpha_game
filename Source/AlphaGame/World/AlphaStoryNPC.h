#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AlphaStoryNPC.generated.h"

class AAlphaCharacter;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
enum class EAlphaPrimitive : uint8;

/** Deterministic, non-combat chapter guide. Each peer creates the same fixed NPC;
 * only the authority can accept interaction and advance the shared story. */
UCLASS(NotBlueprintable)
class ALPHAGAME_API AAlphaStoryNPC : public AActor
{
    GENERATED_BODY()
public:
    AAlphaStoryNPC();
    virtual void BeginPlay() override;

    /** Set before FinishSpawning; actor location is the NPC's feet. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story")
    int32 ChapterIndex = 0;

    static FVector GetLocalPosition() { return FVector(-1000.f, 0.f, 0.f); }
    static constexpr float InteractionRadius = 240.f;
    static FText GetSpeakerName(int32 Chapter);
    static FText GetDialogue(int32 Chapter, int32 Page, bool bDebrief = false);
    static int32 GetPageCount(int32 Chapter);

    /** Authority-side validation; UI can show proximity separately but cannot commit progress. */
    bool CanTalk(AAlphaCharacter* Speaker) const;

private:
    void BuildAppearance();
    void AddPart(EAlphaPrimitive Shape, const FVector& Location, const FVector& Dimensions,
        UMaterialInstanceDynamic* Material, const FRotator& Rotation = FRotator::ZeroRotator);

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> Parts;
};
