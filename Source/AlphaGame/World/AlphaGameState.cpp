#include "World/AlphaGameState.h"
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
    DOREPLIFETIME(AAlphaGameState, Wave);
    DOREPLIFETIME(AAlphaGameState, RemainingEnemies);
    DOREPLIFETIME(AAlphaGameState, RoundState);
    DOREPLIFETIME(AAlphaGameState, EncounterStage);
    DOREPLIFETIME(AAlphaGameState, ForgeBoss);
    DOREPLIFETIME(AAlphaGameState, BossPhase);
    DOREPLIFETIME(AAlphaGameState, EncounterPlayerCount);
}

void AAlphaGameState::BuildPrototypeArena()
{
    UStaticMesh* Cube = AlphaArt::Mesh(EAlphaPrimitive::Cube);
    if (!Cube) return;
    const auto AddCollisionBlock = [this, Cube](const FVector& Location, const FVector& Scale)
    {
        AStaticMeshActor* Block = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
        if (!Block) return;
        Block->SetReplicates(false);
        UStaticMeshComponent* Mesh = Block->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(Cube);
        Mesh->SetWorldScale3D(Scale);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        // These five deterministic collision volumes exist even on dedicated
        // servers. Rendering is a separate, entirely non-colliding local actor.
        Block->SetActorHiddenInGame(true);
        Mesh->SetCastShadow(false);
    };
    AddCollisionBlock(FVector(0, 0, -50), FVector(28, 8, 1));
    AddCollisionBlock(FVector(0, -450, 500), FVector(30, 1, 12));
    AddCollisionBlock(FVector(0, 450, 500), FVector(30, 1, 12));
    AddCollisionBlock(FVector(-1450, 0, 500), FVector(1, 10, 12));
    AddCollisionBlock(FVector(1450, 0, 500), FVector(1, 10, 12));
    if (GetNetMode() != NM_DedicatedServer)
    {
        GetWorld()->SpawnActor<AAlphaForgeEnvironment>(FVector::ZeroVector, FRotator::ZeroRotator);
    }
}
