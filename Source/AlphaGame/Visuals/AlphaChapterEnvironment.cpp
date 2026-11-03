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
    const FLinearColor& Tint, float Glow, bool bShadow)
{
    UInstancedStaticMeshComponent* Batch = NewObject<UInstancedStaticMeshComponent>(this);
    AddInstanceComponent(Batch);
    Batch->SetupAttachment(RootComponent);
    Batch->SetMobility(EComponentMobility::Movable);
    Batch->SetStaticMesh(AlphaArt::Mesh(Shape));
    Batch->SetMaterial(0, AlphaArt::Material(this, Tint, .12f, .78f, Glow, .55f));
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
    // Flat floor markings identify the NPC staging area and eastward route.
    // Nothing here blocks players, attacks, navigation or camera traces.
    Add(Trim, FVector(-1100, 0, 2), FVector(360, 660, 3));
    Add(Light, FVector(-1100, -326, 4), FVector(350, 5, 3));
    Add(Light, FVector(-1100, 326, 4), FVector(350, 5, 3));
    Add(Trim, FVector(1210, 0, 2), FVector(220, 660, 3));
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const float X = 420.f + Index * 155.f;
        Beam(Light, FVector(X - 25, -310, 5), FVector(X + 15, -285, 5), 5.f);
        Beam(Light, FVector(X + 15, -285, 5), FVector(X - 25, -260, 5), 5.f);
    }
    for (float X : {-1360.f, 1360.f})
    {
        Add(Trim, FVector(X, 390, 170), FVector(35, 35, 340));
        Add(Light, FVector(X, 364, 245), FVector(16, 12, 95));
    }
}

void AAlphaChapterEnvironment::BuildDock()
{
    UInstancedStaticMeshComponent* Wood = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.36f, .25f, .17f));
    UInstancedStaticMeshComponent* Deck = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.46f, .37f, .27f));
    UInstancedStaticMeshComponent* Iron = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.16f, .23f, .3f));
    UInstancedStaticMeshComponent* Brass = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.7f, .46f, .17f));
    UInstancedStaticMeshComponent* Red = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.68f, .09f, .12f));
    UInstancedStaticMeshComponent* Water = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.08f, .34f, .46f), .08f, false);
    UInstancedStaticMeshComponent* Foam = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.35f, .65f, .72f), .1f, false);
    UInstancedStaticMeshComponent* Amber = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(1.f, .65f, .2f), 1.f, false);
    UInstancedStaticMeshComponent* Boat = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.13f, .2f, .29f));
    UInstancedStaticMeshComponent* Roof = MakeBatch(EAlphaPrimitive::Cone, FLinearColor(.18f, .42f, .45f));

    Add(Iron, FVector(0, 0, -18), FVector(2820, 810, 34));
    for (int32 X = 0; X < 35; ++X)
    {
        for (int32 Y = 0; Y < 4; ++Y)
            Add((X + Y) % 4 ? Deck : Wood, FVector(-1360 + X * 80, -300 + Y * 200, -2), FVector(77, 197, 6));
    }
    for (float Y : {-540.f, 580.f})
    {
        Add(Water, FVector(0, Y, -63), FVector(3300, 270, 12));
        for (int32 Index = 0; Index < 13; ++Index)
            Add(Foam, FVector(-1450 + Index * 230, Y + (Index % 2 ? 40 : -40), -55), FVector(100, 5, 2));
    }
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const float X = -1300.f + Index * 370.f;
        for (float Y : {-405.f, 405.f})
        {
            Add(Wood, FVector(X, Y, -80), FVector(36, 36, 190));
            Add(Brass, FVector(X, Y, 16), FVector(48, 48, 14));
        }
    }
    // Cargo cranes and suspended supplies stay behind the combat boundary.
    for (float X : {-520.f, 720.f})
    {
        Add(Iron, FVector(X, 590, 245), FVector(55, 55, 490));
        Beam(Iron, FVector(X, 590, 480), FVector(X + 400, 590, 590), 35);
        Beam(Brass, FVector(X + 370, 590, 575), FVector(X + 370, 590, 320), 6);
        Add(Wood, FVector(X + 370, 590, 270), FVector(125, 110, 100));
        Beam(Iron, FVector(X - 100, 590, 0), FVector(X, 590, 300), 20);
    }
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const float X = -700 + Index * 760.f;
        Add(Boat, FVector(X, 930, -20), FVector(550, 210, 90));
        Add(Brass, FVector(X, 920, 160), FVector(15, 15, 360));
        Add(Red, FVector(X + 90, 920, 210), FVector(170, 12, 190));
    }
    // Entry shelter frames the fixed NPC without occupying the safe floor.
    for (float X : {-1280.f, -920.f}) Add(Red, FVector(X, 510, 120), FVector(30, 30, 240));
    Add(Roof, FVector(-1100, 510, 285), FVector(520, 320, 130));
    Add(Wood, FVector(-1100, 540, 140), FVector(340, 18, 210));
    Add(Amber, FVector(-1100, 480, 245), FVector(85, 25, 15));
    BuildRouteMarkers(Brass, Amber);
    AddLamp(FVector(-1100, 200, 220), FLinearColor(1.f, .72f, .4f));
    AddLamp(FVector(1100, 180, 220), FLinearColor(.5f, .75f, 1.f));
}

void AAlphaChapterEnvironment::BuildCanal()
{
    UInstancedStaticMeshComponent* Stone = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.33f, .42f, .48f));
    UInstancedStaticMeshComponent* PaleStone = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.47f, .54f, .56f));
    UInstancedStaticMeshComponent* Teal = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.12f, .47f, .48f));
    UInstancedStaticMeshComponent* Brass = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.67f, .46f, .2f));
    UInstancedStaticMeshComponent* Water = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.07f, .35f, .42f), .12f, false);
    UInstancedStaticMeshComponent* Ripple = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(.27f, .62f, .64f), .15f, false);
    UInstancedStaticMeshComponent* Light = MakeBatch(EAlphaPrimitive::Cube, FLinearColor(1.f, .75f, .25f), 1.2f, false);
    UInstancedStaticMeshComponent* Lamp = MakeBatch(EAlphaPrimitive::Sphere, FLinearColor(1.f, .57f, .2f), 1.2f, false);
    UInstancedStaticMeshComponent* Roof = MakeBatch(EAlphaPrimitive::Cone, FLinearColor(.12f, .32f, .39f));

    Add(Stone, FVector(0, 0, -18), FVector(2820, 810, 34));
    for (int32 X = 0; X < 14; ++X)
        for (int32 Y = 0; Y < 4; ++Y)
            Add((X + Y) % 3 ? PaleStone : Stone, FVector(-1300 + X * 200, -300 + Y * 200, -2), FVector(195, 195, 6));
    for (float Y : {-610.f, 660.f})
    {
        Add(Water, FVector(0, Y, -75), FVector(3300, 420, 14));
        for (int32 Index = 0; Index < 18; ++Index)
            Add(Ripple, FVector(-1500 + Index * 175, Y + (Index % 3 - 1) * 80, -66), FVector(100, 5, 2));
    }
    // Low edge bands retain depth readability; no tall foreground rail hides players.
    for (float Y : {-390.f, 390.f})
    {
        Add(Teal, FVector(0, Y, 8), FVector(2800, 20, 16));
        for (int32 Index = 0; Index < 11; ++Index)
            Add(Brass, FVector(-1300 + Index * 260, Y, 21), FVector(35, 35, 10));
    }
    // Monumental rear aqueduct arches, layered behind the usable ground.
    for (int32 Arch = 0; Arch < 3; ++Arch)
    {
        const float CenterX = -850 + Arch * 850.f;
        for (int32 Segment = 0; Segment < 12; ++Segment)
        {
            const float A = Segment * PI / 12.f;
            const float B = (Segment + 1) * PI / 12.f;
            Beam(Teal, FVector(CenterX + FMath::Cos(A) * 350, 690, 150 + FMath::Sin(A) * 350),
                FVector(CenterX + FMath::Cos(B) * 350, 690, 150 + FMath::Sin(B) * 350), 50.f);
        }
        for (float Side : {-1.f, 1.f})
            Add(Stone, FVector(CenterX + Side * 350, 690, 45), FVector(65, 90, 210));
        Add(PaleStone, FVector(CenterX, 690, 530), FVector(820, 120, 45));
    }
    for (int32 Index = 0; Index < 11; ++Index)
    {
        const float X = -1250 + Index * 250.f;
        const float Height = 330.f + FMath::Cos(Index * .7f) * 35.f;
        Beam(Brass, FVector(X, 490, 500), FVector(X, 490, Height + 25), 4.f);
        Add(Lamp, FVector(X, 490, Height), FVector(36, 32, 48));
        Add(Brass, FVector(X, 490, Height + 28), FVector(45, 40, 8));
    }
    Beam(Brass, FVector(-1340, 490, 500), FVector(1340, 490, 500), 6.f);
    // A keeper's pavilion and an illuminated exit gate frame story interactions.
    for (float X : {-1300.f, -900.f}) Add(Teal, FVector(X, 530, 130), FVector(32, 32, 260));
    Add(Roof, FVector(-1100, 530, 310), FVector(560, 330, 150));
    Add(Stone, FVector(-1100, 540, 90), FVector(300, 35, 180));
    Add(Brass, FVector(1240, 460, 225), FVector(260, 35, 30));
    BuildRouteMarkers(Brass, Light);
    AddLamp(FVector(-1100, 160, 230), FLinearColor(1.f, .78f, .45f));
    AddLamp(FVector(1090, 140, 230), FLinearColor(.6f, .85f, 1.f));
}
