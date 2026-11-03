#include "World/AlphaGameState.h"
#include "World/AlphaBreakableProp.h"
#include "World/AlphaStoryNPC.h"
#include "Rules/StoryRules.h"
#include "Rules/LevelLayoutRules.h"
#include "World/AlphaRouteGeometry.h"
#include "Visuals/AlphaChapterEnvironment.h"
#include "Characters/Boss/AlphaForgeBoss.h"
#include "Visuals/AlphaArtLibrary.h"
#include "Visuals/AlphaForgeEnvironment.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void AAlphaGameState::BeginPlay()
{
    Super::BeginPlay();
    // Deterministic static arena exists locally on every peer, including collision.
    // These actors deliberately do not replicate; no binary map asset is required.
    BuildPrototypeArena();
}

void AAlphaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAlphaGameState, ChapterIndex);
    DOREPLIFETIME(AAlphaGameState, StoryStage);
    DOREPLIFETIME(AAlphaGameState, StoryRevision);
    DOREPLIFETIME(AAlphaGameState, bAwaitingRouteAdvance);
    DOREPLIFETIME(AAlphaGameState, RouteObjective);
    DOREPLIFETIME(AAlphaGameState, Wave);
    DOREPLIFETIME(AAlphaGameState, RemainingEnemies);
    DOREPLIFETIME(AAlphaGameState, RoundState);
    DOREPLIFETIME(AAlphaGameState, EncounterStage);
    DOREPLIFETIME(AAlphaGameState, ForgeBoss);
    DOREPLIFETIME(AAlphaGameState, BossPhase);
    DOREPLIFETIME(AAlphaGameState, EncounterPlayerCount);
    DOREPLIFETIME(AAlphaGameState, HumanPlayerCount);
    DOREPLIFETIME(AAlphaGameState, CompanionCount);
    DOREPLIFETIME(AAlphaGameState, bCompanionFillEnabled);
}

void AAlphaGameState::BuildPrototypeArena()
{
    for (int32 Chapter = 0; Chapter < AlphaStoryRules::ChapterCount; ++Chapter)
    {
        const FVector Origin = GetChapterOrigin(Chapter);
        const FTransform RouteTransform(FRotator::ZeroRotator,Origin);
        if (AAlphaRouteGeometry* Route=GetWorld()->SpawnActorDeferred<AAlphaRouteGeometry>(AAlphaRouteGeometry::StaticClass(),RouteTransform))
        {
            Route->ChapterIndex=Chapter;
            Route->FinishSpawning(RouteTransform);
        }
        if (HasAuthority())
        {
            for (int32 Side : {-1, 1})
            {
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    const AlphaLayoutRules::Point Position=AlphaLayoutRules::Project(Chapter,
                        {Side * (420. + Index * 250.), Index % 2 == 0 ? 260. : -260.},70.);
                    GetWorld()->SpawnActor<AAlphaBreakableProp>(
                        Origin + FVector(Position.X,Position.Y,34.+AlphaLayoutRules::SurfaceHeight(Chapter,Position)),FRotator::ZeroRotator);
                }
            }
        }
        // NPCs are deterministic local scenery on every peer; only the server
        // GameMode accepts an interaction or changes the shared story stage.
        const FTransform NPCTransform(FRotator::ZeroRotator, Origin + AAlphaStoryNPC::GetLocalPosition());
        if (AAlphaStoryNPC* NPC = GetWorld()->SpawnActorDeferred<AAlphaStoryNPC>(AAlphaStoryNPC::StaticClass(), NPCTransform))
        {
            NPC->ChapterIndex = Chapter;
            NPC->FinishSpawning(NPCTransform);
        }
        if (GetNetMode() != NM_DedicatedServer)
        {
            if (Chapter == 1)
                GetWorld()->SpawnActor<AAlphaForgeEnvironment>(Origin, FRotator::ZeroRotator);
            else if (AAlphaChapterEnvironment* Environment = GetWorld()->SpawnActor<AAlphaChapterEnvironment>(Origin, FRotator::ZeroRotator))
                Environment->BuildForChapter(Chapter);
        }
    }
}

FVector AAlphaGameState::GetChapterOrigin(int32 Chapter)
{
    return FVector(FMath::Clamp(Chapter, 0, AlphaStoryRules::ChapterCount - 1) * 6000.f, 0, 0);
}

int32 AAlphaGameState::GetChapterWaveCount() const
{
    return AlphaStoryRules::WaveCount(ChapterIndex);
}

bool AAlphaGameState::ChapterHasBoss() const
{
    return AlphaStoryRules::HasBoss(ChapterIndex);
}
