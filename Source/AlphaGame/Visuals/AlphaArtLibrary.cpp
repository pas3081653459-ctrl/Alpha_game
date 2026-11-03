#include "Visuals/AlphaArtLibrary.h"

#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "MaterialTypes.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

UAlphaArtLibrary::UAlphaArtLibrary()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SurfaceAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    Cube = CubeAsset.Object;
    Sphere = SphereAsset.Object;
    Cylinder = CylinderAsset.Object;
    Cone = ConeAsset.Object;
    Plane = PlaneAsset.Object;
    FallbackSurface = SurfaceAsset.Object;
}

UStaticMesh* UAlphaArtLibrary::GetMesh(EAlphaPrimitive Shape) const
{
    switch (Shape)
    {
    case EAlphaPrimitive::Sphere: return Sphere;
    case EAlphaPrimitive::Cylinder: return Cylinder;
    case EAlphaPrimitive::Cone: return Cone;
    case EAlphaPrimitive::Plane: return Plane;
    default: return Cube;
    }
}

namespace
{
bool bFlatColorEnabled = false;
TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> SurfaceInstances;

bool HasArtParameters(UMaterialInterface* Material, bool bSurface)
{
    if (!Material) return false;
    TArray<FMaterialParameterInfo> Vectors, Scalars, Textures;
    TArray<FGuid> Ids;
    Material->GetAllVectorParameterInfo(Vectors, Ids);
    Ids.Reset();
    Material->GetAllScalarParameterInfo(Scalars, Ids);
    const bool bTint = Vectors.ContainsByPredicate([](const FMaterialParameterInfo& Info) { return Info.Name == TEXT("Tint"); });
    const bool bGlow = Scalars.ContainsByPredicate([](const FMaterialParameterInfo& Info) { return Info.Name == TEXT("Glow"); });
    if (!bSurface) return bTint && bGlow;
    Ids.Reset();
    Material->GetAllTextureParameterInfo(Textures, Ids);
    const bool bFlatColor = Scalars.ContainsByPredicate([](const FMaterialParameterInfo& Info) { return Info.Name == TEXT("FlatColor"); });
    return bTint && bGlow && bFlatColor && Textures.ContainsByPredicate([](const FMaterialParameterInfo& Info) { return Info.Name == TEXT("SurfaceAtlas"); });
}

void MissingArtWarning(const TCHAR* Name)
{
    UE_LOG(LogTemp, Error, TEXT("AlphaGame art: %s missing or incompatible. Engine fallback does NOT guarantee Tint/Color support. Open the editor to run Content/Python/init_unreal.py, or execute Scripts/create_art_assets.py. Then restart PIE. Run AlphaArtCheck for status."), Name);
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(0x414C5048, 60.f, FColor::Orange,
            TEXT("AlphaGame art NOT READY: generated color material missing/outdated. Run create_art_assets.py; restart PIE. Console: AlphaArtCheck"));
}

FAutoConsoleCommand ArtCheckCommand(TEXT("AlphaArtCheck"),
    TEXT("Report generated material assets and color parameter readiness (not a shader/render test)."),
    FConsoleCommandDelegate::CreateLambda([]() { GetMutableDefault<UAlphaArtLibrary>()->ReportStatus(); }));

FAutoConsoleCommand FlatColorCommand(TEXT("AlphaArtFlatColor"),
    TEXT("AlphaArtFlatColor 1/0: isolate project surface Tint from lighting/textures; effects are unchanged."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() != 1 || (Args[0] != TEXT("0") && Args[0] != TEXT("1")))
        {
            UE_LOG(LogTemp, Display, TEXT("Usage: AlphaArtFlatColor 1 (isolate Tint), AlphaArtFlatColor 0 (normal). Requires generated art revision 3."));
            return;
        }
        bFlatColorEnabled = Args[0] == TEXT("1");
        SurfaceInstances.RemoveAll([](const TWeakObjectPtr<UMaterialInstanceDynamic>& Entry) { return !Entry.IsValid(); });
        for (const TWeakObjectPtr<UMaterialInstanceDynamic>& Entry : SurfaceInstances)
            if (UMaterialInstanceDynamic* Instance = Entry.Get())
                Instance->SetScalarParameterValue(TEXT("FlatColor"), bFlatColorEnabled ? 1.f : 0.f);
        UE_LOG(LogTemp, Display, TEXT("AlphaArtFlatColor %d: updated %d project surface instances. Effects unchanged. Exposure/tonemapping still applies."),
            bFlatColorEnabled ? 1 : 0, SurfaceInstances.Num());
        GetMutableDefault<UAlphaArtLibrary>()->ReportStatus();
    }));
}

UMaterialInterface* UAlphaArtLibrary::GetSurface()
{
    const double Now = FPlatformTime::Seconds();
    if (!GeneratedSurface && Now >= NextSurfaceRetryAt)
    {
        NextSurfaceRetryAt = Now + 5.;
        UMaterialInterface* Candidate = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Art/Materials/M_AlphaSurface.M_AlphaSurface"), nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (HasArtParameters(Candidate, true)) GeneratedSurface = Candidate;
        else if (!bWarnedSurface) { bWarnedSurface = true; MissingArtWarning(TEXT("M_AlphaSurface")); }
    }
    return GeneratedSurface ? GeneratedSurface.Get() : FallbackSurface.Get();
}

UMaterialInterface* UAlphaArtLibrary::GetEffect()
{
    const double Now = FPlatformTime::Seconds();
    if (!GeneratedEffect && Now >= NextEffectRetryAt)
    {
        NextEffectRetryAt = Now + 5.;
        UMaterialInterface* Candidate = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Art/Materials/M_AlphaEffect.M_AlphaEffect"), nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (HasArtParameters(Candidate, false)) GeneratedEffect = Candidate;
        else if (!bWarnedEffect) { bWarnedEffect = true; MissingArtWarning(TEXT("M_AlphaEffect")); }
    }
    return GeneratedEffect ? GeneratedEffect.Get() : FallbackSurface.Get();
}

void UAlphaArtLibrary::ReportStatus()
{
    NextSurfaceRetryAt = NextEffectRetryAt = 0.;
    GetSurface();
    GetEffect();
    const bool bReady = GeneratedSurface && GeneratedEffect;
    const FString Message = FString::Printf(TEXT("AlphaGame art: Surface=%s Effect=%s. Asset/parameter check only; inspect shader errors and rendered output separately."),
        GeneratedSurface ? TEXT("ready") : TEXT("MISSING/OUTDATED"), GeneratedEffect ? TEXT("ready") : TEXT("MISSING/OUTDATED"));
    UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
    if (GEngine) GEngine->AddOnScreenDebugMessage(0x414C5048, 30.f, bReady ? FColor::Green : FColor::Orange, Message);
}

UStaticMesh* AlphaArt::Mesh(EAlphaPrimitive Shape)
{
    return GetDefault<UAlphaArtLibrary>()->GetMesh(Shape);
}

UMaterialInstanceDynamic* AlphaArt::Material(UObject* Outer, FLinearColor Tint,
    float Metallic, float Roughness, float Glow, float AnimeShade, int32 SurfaceStyle, float TextureStrength)
{
    if (!Outer) return nullptr;
    UMaterialInterface* Surface = GetMutableDefault<UAlphaArtLibrary>()->GetSurface();
    if (!Surface) return nullptr;
    UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Surface, Outer);
    if (Instance)
    {
        Instance->SetVectorParameterValue(TEXT("Tint"), Tint);
        Instance->SetVectorParameterValue(TEXT("Color"), Tint); // Best-effort compatibility only; readiness warning covers unsupported fallbacks.
        Instance->SetScalarParameterValue(TEXT("Metallic"), FMath::Clamp(Metallic, 0.f, 1.f));
        Instance->SetScalarParameterValue(TEXT("Roughness"), FMath::Clamp(Roughness, .05f, 1.f));
        Instance->SetScalarParameterValue(TEXT("Glow"), FMath::Clamp(Glow, 0.f, 8.f));
        Instance->SetScalarParameterValue(TEXT("AnimeShade"), FMath::Clamp(AnimeShade, 0.f, 1.f));
        Instance->SetScalarParameterValue(TEXT("SurfaceStyle"), FMath::Clamp(SurfaceStyle, 0, 3));
        Instance->SetScalarParameterValue(TEXT("TextureStrength"), FMath::Clamp(TextureStrength, 0.f, 1.f));
        Instance->SetScalarParameterValue(TEXT("FlatColor"), bFlatColorEnabled ? 1.f : 0.f);
        SurfaceInstances.RemoveAll([](const TWeakObjectPtr<UMaterialInstanceDynamic>& Entry) { return !Entry.IsValid(); });
        SurfaceInstances.Add(Instance);
    }
    return Instance;
}

UMaterialInstanceDynamic* AlphaArt::EffectMaterial(UObject* Outer, FLinearColor Tint, float Glow)
{
    if (!Outer) return nullptr;
    UMaterialInterface* Surface = GetMutableDefault<UAlphaArtLibrary>()->GetEffect();
    if (!Surface) return nullptr;
    UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Surface, Outer);
    if (Instance)
    {
        Instance->SetVectorParameterValue(TEXT("Tint"), Tint);
        Instance->SetVectorParameterValue(TEXT("Color"), Tint);
        Instance->SetScalarParameterValue(TEXT("Glow"), FMath::Clamp(Glow, 0.f, 8.f));
    }
    return Instance;
}
