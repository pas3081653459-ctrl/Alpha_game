#include "Visuals/AlphaScenicBackdrop.h"

#include "Visuals/AlphaArtLibrary.h"
#include "World/AlphaGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

AAlphaScenicBackdrop::AAlphaScenicBackdrop()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .25f;
    bReplicates = false;
    SetActorEnableCollision(false);
    Vista = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaintedVista"));
    SetRootComponent(Vista);
    Vista->SetMobility(EComponentMobility::Movable);
    Vista->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Vista->SetGenerateOverlapEvents(false);
    Vista->SetCanEverAffectNavigation(false);
    Vista->SetCastShadow(false);
    Vista->SetReceivesDecals(false);
    Vista->SetVisibility(false);
}

void AAlphaScenicBackdrop::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer)
    {
        SetActorTickEnabled(false);
        return;
    }
    Vista->SetStaticMesh(AlphaArt::Mesh(EAlphaPrimitive::Plane));
    if (const UTexture2D* Texture = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/Art/Backdrops/T_ChapterPanoramas.T_ChapterPanoramas")))
    {
        PanoramaAspect = 3.f * Texture->GetSizeX() / FMath::Max(1, Texture->GetSizeY());
    }
    for (int32 Chapter = 0; Chapter != 3; ++Chapter)
    {
        const FString Path = FString::Printf(TEXT("/Game/Art/Backdrops/M_Chapter%d.M_Chapter%d"), Chapter, Chapter);
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Path);
        ChapterMaterials.Add(Material);
        if (!Material) UE_LOG(LogTemp, Warning,
            TEXT("AlphaGame: missing painted vista %s. Run Scripts/create_backdrop_assets.py in UE Editor."), *Path);
    }
    RefreshChapter();
}

void AAlphaScenicBackdrop::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshChapter();
}

void AAlphaScenicBackdrop::RefreshChapter()
{
    const AAlphaGameState* State = GetWorld() ? GetWorld()->GetGameState<AAlphaGameState>() : nullptr;
    if (!State) return;
    const int32 Chapter = FMath::Clamp(State->ChapterIndex, 0, 2);
    if (Chapter != LastChapter)
    {
        LastChapter = Chapter;
        // Camera direction is (0, 1, -.39). At Y=2800 its centre ray passes
        // through Z=-1070..-695 for the allowed focus range. Align the painting
        // perpendicular to that ray, rather than placing it high above the view.
        SetActorLocation(AAlphaGameState::GetChapterOrigin(Chapter) + FVector(450.f, 2800.f, -900.f));
        SetActorRotation(FRotator(0.f, 0.f, FMath::RadiansToDegrees(FMath::Atan(1.f / .39f))));
        const bool bReady = ChapterMaterials.IsValidIndex(Chapter) && ChapterMaterials[Chapter];
        Vista->SetVisibility(bReady);
        if (bReady) Vista->SetMaterial(0, ChapterMaterials[Chapter]);
    }

    int32 Width = 0, Height = 0;
    if (APlayerController* Player = GetWorld()->GetFirstPlayerController()) Player->GetViewportSize(Width, Height);
    const float Aspect = Width > 0 && Height > 0 ? static_cast<float>(Width) / Height : 16.f / 9.f;
    // Max camera distance 5200 + the vista's 2800 offset, projected onto the
    // tilted plane, is below 9500cm. FOV55 means half-width = distance*.520567.
    // Fit vertical FOV too, preserving the painting's aspect even in portrait.
    // Extra 400cm covers focus-height/depth changes and local camera feedback.
    const float RequiredHeight = 2.f * (9500.f * .5205671f / FMath::Max(.1f, Aspect) + 400.f);
    const float VistaWidth = FMath::Max(16000.f, RequiredHeight * PanoramaAspect);
    if (!FMath::IsNearlyEqual(VistaWidth, LastWidth, .1f))
    {
        LastWidth = VistaWidth;
        SetActorScale3D(FVector(VistaWidth / 100.f, VistaWidth / (100.f * FMath::Max(.1f, PanoramaAspect)), 1.f));
    }
}
