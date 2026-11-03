#include "Visuals/AlphaChapterEnvironment.h"

#include "Visuals/AlphaArtLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AAlphaChapterEnvironment::AAlphaChapterEnvironment()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(false);
    SetActorEnableCollision(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ChapterSetRoot"));
}

void AAlphaChapterEnvironment::ClearSet()
{
    for (UInstancedStaticMeshComponent* Batch : Batches) if (Batch) Batch->DestroyComponent();
    Batches.Reset();
    for (UPointLightComponent* Lamp : Lamps) if (Lamp) Lamp->DestroyComponent();
    Lamps.Reset();
    if (ChapterLabel) ChapterLabel->DestroyComponent();
    ChapterLabel = nullptr;
}

void AAlphaChapterEnvironment::BuildForChapter(int32 Chapter)
{
    if (GetNetMode() == NM_DedicatedServer || (Chapter != 0 && Chapter != 2)) return;
    if (ChapterIndex == Chapter && !Batches.IsEmpty()) return;
    ClearSet();
    ChapterIndex = Chapter;
    if (Chapter == 0) BuildDock();
    else BuildCanal();

    ChapterLabel = NewObject<UTextRenderComponent>(this);
    AddInstanceComponent(ChapterLabel);
    ChapterLabel->SetupAttachment(RootComponent);
    ChapterLabel->SetRelativeLocation(FVector(-1240, 495, 320));
    ChapterLabel->SetRelativeRotation(FRotator(0, -90, 0));
    ChapterLabel->SetText(FText::FromString(Chapter == 0 ? TEXT("01  GRAYFERRY DOCK") : TEXT("03  LANTERN CANAL")));
    ChapterLabel->SetWorldSize(35.f);
    ChapterLabel->SetTextRenderColor(FColor(245, 218, 158));
    ChapterLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ChapterLabel->SetCanEverAffectNavigation(false);
    ChapterLabel->SetCastShadow(false);
    ChapterLabel->RegisterComponent();
}

UInstancedStaticMeshComponent* AAlphaChapterEnvironment::MakeBatch(EAlphaPrimitive Shape,
    const FLinearColor& Tint, float Glow, bool bShadow, int32 SurfaceStyle, float TextureStrength)
{
    UInstancedStaticMeshComponent* Batch = NewObject<UInstancedStaticMeshComponent>(this);
    AddInstanceComponent(Batch);
    Batch->SetupAttachment(RootComponent);
    Batch->SetMobility(EComponentMobility::Movable);
    Batch->SetStaticMesh(AlphaArt::Mesh(Shape));
    Batch->SetMaterial(0, AlphaArt::Material(this, Tint, .12f, .78f, Glow, .55f, SurfaceStyle, TextureStrength));
    Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Batch->SetGenerateOverlapEvents(false);
    Batch->SetCanEverAffectNavigation(false);
    Batch->SetCastShadow(bShadow);
    Batch->RegisterComponent();
    Batches.Add(Batch);
    return Batch;
}

void AAlphaChapterEnvironment::Add(UInstancedStaticMeshComponent* Batch, const FVector& Position,
    const FVector& Dimensions, const FRotator& Rotation)
{
    if (Batch) Batch->AddInstance(FTransform(Rotation, Position, Dimensions / 100.f));
}

void AAlphaChapterEnvironment::Beam(UInstancedStaticMeshComponent* Batch,
    const FVector& Start, const FVector& End, float Width)
{
    const FVector Delta = End - Start;
    if (Delta.IsNearlyZero()) return;
    Add(Batch, (Start + End) * .5f, FVector(Delta.Size(), Width, Width), Delta.Rotation());
}

void AAlphaChapterEnvironment::AddLamp(const FVector& Location, const FLinearColor& Color)
{
    if (Lamps.Num() >= 2) return;
    UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
    AddInstanceComponent(Lamp);
    Lamp->SetupAttachment(RootComponent);
    Lamp->SetRelativeLocation(Location);
    Lamp->SetMobility(EComponentMobility::Movable);
    Lamp->SetIntensity(500.f);
    Lamp->SetAttenuationRadius(450.f);
    Lamp->SetLightColor(Color);
    Lamp->SetCastShadows(false);
    Lamp->RegisterComponent();
    Lamps.Add(Lamp);
}

void AAlphaChapterEnvironment::BuildRouteMarkers(UInstancedStaticMeshComponent* Trim,
    UInstancedStaticMeshComponent* Light)
{
    // Local milestones only. Shared RouteGeometry owns the full walkable
    // surface and outline; no rectangular decorative floor bridges its gaps.
    Add(Trim, FVector(-1100, 0, 2), FVector(250, 250, 3));
    for (float Side : {-1.f, 1.f})
        Add(Light, FVector(-1100, Side * 127.f, 5), FVector(250, 4, 3));
    Add(Trim, FVector(2410, 0, 2), FVector(150, 180, 3));
    for (float X : {700.f, 1050.f})
    {
        Beam(Light, FVector(X - 25, -25, 5), FVector(X + 15, 0, 5), 5.f);
        Beam(Light, FVector(X + 15, 0, 5), FVector(X - 25, 25, 5), 5.f);
    }
}

void AAlphaChapterEnvironment::BuildDock()
{
    UInstancedStaticMeshComponent* Wood = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.94f, .86f, .72f), 0, true, 0, .75f);
    UInstancedStaticMeshComponent* Iron = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.24f, .33f, .43f));
    UInstancedStaticMeshComponent* Brass = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.86f, .6f, .24f));
    UInstancedStaticMeshComponent* RedSail = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(1.f, .72f, .68f), 0, true, 3, .8f);
    UInstancedStaticMeshComponent* Water = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.12f, .51f, .66f), .08f, false);
    UInstancedStaticMeshComponent* Foam = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.53f, .85f, .88f), .05f, false);
    UInstancedStaticMeshComponent* Amber = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(1.f, .72f, .3f), .65f, false);
    UInstancedStaticMeshComponent* Boat = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.17f, .29f, .39f));
    UInstancedStaticMeshComponent* Roof = MakeBatch(EAlphaPrimitive::Cone, FLinearColor(.66f, .94f, .91f), 0, true, 2, .6f);

    // Low water fills the negative spaces between the REAL cargo platforms.
    // It is set dressing only, 85cm below the RouteGeometry deck.
    Add(Water, FVector(500, 0, -85), FVector(4400, 2050, 12));
    for (int32 Index = 0; Index < 16; ++Index)
    {
        const float X = -1400.f + Index * 250.f;
        const float Y = Index % 2 == 0 ? -760.f : 850.f;
        Add(Foam, FVector(X, Y, -77), FVector(95 + (Index % 3) * 35, 6, 2));
    }
    // Six structural piles belong to the broad unloading and mooring aprons,
    // not a repeated fence that falsely outlines a rectangular playable area.
    for (const FVector& Pile : {FVector(-1250, 310, -90), FVector(-350, 480, -90),
        FVector(450, -480, -90), FVector(1300, 380, -90), FVector(2180, -380, -90), FVector(2330, 380, -90)})
    {
        Add(Wood, Pile, FVector(45, 45, 190));
        Add(Brass, Pile + FVector(0, 0, 90), FVector(58, 58, 12));
    }

    // Landmark A: one asymmetric cargo crane overlooking the first encounter.
    const FVector Crane(80, 730, 0);
    Add(Iron, Crane + FVector(0, 0, 280), FVector(62, 62, 560));
    Beam(Iron, Crane + FVector(-150, 0, 40), Crane + FVector(0, 0, 400), 26);
    Beam(Iron, Crane + FVector(0, 0, 530), Crane + FVector(470, 0, 665), 38);
    Beam(Brass, Crane + FVector(430, 0, 650), Crane + FVector(430, 0, 340), 6);
    Add(Wood, Crane + FVector(430, 0, 285), FVector(145, 115, 110));
    Add(RedSail, Crane + FVector(-10, -37, 450), FVector(110, 7, 85), FRotator(0, 0, -10));
    // Dock storage stays behind the wide yard, away from the narrow connector.
    Add(Wood, FVector(-300, 680, 80), FVector(190, 125, 160));
    Add(Wood, FVector(-100, 690, 55), FVector(160, 120, 110), FRotator(0, 12, 0));

    // Only the 600..1150 connector gets hanging ropes. Its thin silhouette
    // makes the narrow passage legible without covering characters' feet.
    for (float Side : {-1.f, 1.f})
    {
        for (float X : {635.f, 1115.f})
            Add(Wood, FVector(X, Side * 220, 46), FVector(18, 18, 92));
        Beam(Brass, FVector(635, Side * 220, 90), FVector(875, Side * 220, 63), 4);
        Beam(Brass, FVector(875, Side * 220, 63), FVector(1115, Side * 220, 90), 4);
    }

    // Landmark B: one moored junk with a tall, painted crimson sail at the
    // second apron. The silhouette is distinct from the entry shelter/crane.
    Add(Boat, FVector(1810, 860, -13), FVector(810, 250, 105));
    Add(Wood, FVector(1810, 850, 35), FVector(660, 215, 22));
    Add(Brass, FVector(1760, 865, 285), FVector(18, 18, 600));
    for (int32 Strip = 0; Strip < 4; ++Strip)
    {
        const float Width = 320.f - Strip * 35.f;
        Add(RedSail, FVector(1810, 842, 160 + Strip * 95), FVector(Width, 9, 90), FRotator(0, 0, -5));
        Beam(Brass, FVector(1760, 832, 115 + Strip * 95), FVector(1810 + Width * .5f, 832, 115 + Strip * 95), 4);
    }
    Add(Boat, FVector(2260, 865, 85), FVector(160, 205, 130), FRotator(0, 0, -15));

    // Entry guide shelter and small exit buoy are subordinate route landmarks.
    for (float X : {-1290.f, -930.f}) Add(Wood, FVector(X, 485, 120), FVector(28, 28, 240));
    Add(Roof, FVector(-1110, 485, 285), FVector(510, 300, 135));
    Add(Wood, FVector(-1110, 530, 140), FVector(330, 18, 205));
    Add(Amber, FVector(-1100, 460, 230), FVector(80, 24, 18));
    Add(Brass, FVector(2470, 305, 80), FVector(25, 25, 160));
    Add(Amber, FVector(2470, 305, 175), FVector(75, 65, 32));
    BuildRouteMarkers(Brass, Amber);
    AddLamp(FVector(-1100, 180, 230), FLinearColor(1.f, .8f, .55f));
    AddLamp(FVector(1780, 260, 270), FLinearColor(.6f, .82f, 1.f));
}

void AAlphaChapterEnvironment::BuildCanal()
{
    UInstancedStaticMeshComponent* Stone = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.92f, .94f, 1.f), 0, true, 1, .72f);
    UInstancedStaticMeshComponent* Teal = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.65f, .96f, .94f), 0, true, 2, .7f);
    UInstancedStaticMeshComponent* Brass = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.85f, .64f, .3f));
    UInstancedStaticMeshComponent* Water = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.1f, .46f, .6f), .08f, false);
    UInstancedStaticMeshComponent* Ripple = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.48f, .81f, .85f), .08f, false);
    UInstancedStaticMeshComponent* Light = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(1.f, .8f, .4f), .8f, false);
    UInstancedStaticMeshComponent* Lamp = MakeBatch(EAlphaPrimitive::Sphere, FLinearColor(1.f, .65f, .28f), .8f, false);
    UInstancedStaticMeshComponent* Roof = MakeBatch(EAlphaPrimitive::Cone, FLinearColor(.66f, .92f, .96f), 0, true, 2, .65f);

    Add(Water, FVector(500, 0, -88), FVector(4400, 2400, 14));
    for (int32 Index = 0; Index < 15; ++Index)
    {
        const float X = -1350.f + Index * 265.f;
        const float Y = (Index % 2 ? 1.f : -1.f) * (880.f + Index % 3 * 70.f);
        Add(Ripple, FVector(X, Y, -78), FVector(120, 6, 2));
    }

    // Landmark A: a SINGLE rear aqueduct arch frames the narrow bridge.
    // Its piers sit well behind the connector, never in its walkable strip.
    const FVector ArchCenter(860, 790, 170);
    for (int32 Segment = 0; Segment < 18; ++Segment)
    {
        const float A = Segment * PI / 18.f;
        const float B = (Segment + 1) * PI / 18.f;
        Beam(Teal, ArchCenter + FVector(FMath::Cos(A) * 440, 0, FMath::Sin(A) * 440),
            ArchCenter + FVector(FMath::Cos(B) * 440, 0, FMath::Sin(B) * 440), 52.f);
    }
    for (float Side : {-1.f, 1.f})
        Add(Stone, ArchCenter + FVector(Side * 440, 0, -55), FVector(70, 110, 230));
    Add(Stone, ArchCenter + FVector(0, 0, 495), FVector(1060, 145, 45));
    Add(Light, ArchCenter + FVector(0, -35, 470), FVector(155, 8, 12));

    // The front court has only low, readable detail. Tall ornaments live at
    // the rear court and exit, leaving the asymmetrical playable wings visible.
    for (const FVector& Seat : {FVector(1430, -680, 24), FVector(1930, -680, 24), FVector(2200, 690, 24)})
    {
        Add(Stone, Seat, FVector(170, 55, 48));
        Add(Teal, Seat + FVector(0, 0, 25), FVector(185, 65, 8));
    }
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const float X = 1640.f + Index * 270.f;
        const float Height = 330.f + (Index == 1 ? 60.f : 0.f);
        Beam(Brass, FVector(X, 740, 500), FVector(X, 740, Height + 28), 5.f);
        Add(Lamp, FVector(X, 740, Height), FVector(45, 40, 65));
        Add(Brass, FVector(X, 740, Height + 35), FVector(55, 48, 8));
    }
    Beam(Brass, FVector(1580, 740, 500), FVector(2270, 740, 500), 7.f);
    // Small memory tablets identify the two courts without blocking the route.
    Add(Teal, FVector(1450, -710, 15), FVector(85, 15, 30));
    Add(Teal, FVector(2160, 710, 90), FVector(85, 18, 180));
    Add(Light, FVector(2160, 696, 135), FVector(55, 5, 10));

    // Landmark B: the unique terminal lamp gate. No row of identical arches.
    for (float Y : {-300.f, 300.f})
    {
        Add(Teal, FVector(2440, Y, 175), FVector(38, 38, 350));
        Add(Brass, FVector(2440, Y, 350), FVector(75, 75, 18));
    }
    Beam(Teal, FVector(2440, -330, 380), FVector(2440, 330, 380), 48.f);
    Add(Lamp, FVector(2440, 0, 315), FVector(95, 70, 125));
    Add(Brass, FVector(2440, 0, 385), FVector(120, 95, 15));

    // Guardhouse: one warm entry silhouette, deliberately smaller than the gate.
    for (float X : {-1280.f, -920.f}) Add(Teal, FVector(X, 480, 125), FVector(28, 28, 250));
    Add(Roof, FVector(-1100, 480, 300), FVector(520, 300, 145));
    Add(Stone, FVector(-1100, 510, 90), FVector(290, 25, 180));
    Add(Light, FVector(-1100, 460, 225), FVector(70, 12, 18));
    BuildRouteMarkers(Brass, Light);
    AddLamp(FVector(-1100, 160, 235), FLinearColor(1.f, .85f, .62f));
    AddLamp(FVector(1860, 100, 270), FLinearColor(.65f, .9f, 1.f));
}
