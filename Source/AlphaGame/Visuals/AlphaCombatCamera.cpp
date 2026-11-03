#include "Visuals/AlphaCombatCamera.h"

#include "Camera/CameraComponent.h"
#include "Characters/AlphaCharacter.h"
#include "Characters/Boss/AlphaForgeBoss.h"
#include "Combat/AlphaCombatComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Rules/PresentationRules.h"
#include "World/AlphaGameState.h"

AAlphaCombatCamera::AAlphaCombatCamera()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = false;
    GetCameraComponent()->SetFieldOfView(55.f);
    GetCameraComponent()->bConstrainAspectRatio = false;
    GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
    GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.f;
    GetCameraComponent()->PostProcessSettings.bOverride_SceneFringeIntensity = true;
    GetCameraComponent()->PostProcessSettings.SceneFringeIntensity = 0.f;
}

void AAlphaCombatCamera::SetShakeStrength(float Value)
{
    if (FMath::IsFinite(Value)) ShakeStrength = FMath::Clamp(Value, 0.f, 1.f);
}

void AAlphaCombatCamera::KickAt(UWorld* World, const FVector& Location, float Strength)
{
    if (!World || World->GetNetMode() == NM_DedicatedServer || Location.ContainsNaN()
        || !FMath::IsFinite(Strength)) return;
    for (TActorIterator<AAlphaCombatCamera> It(World); It; ++It)
    {
        APlayerController* Player = Cast<APlayerController>(It->GetOwner());
        if (!Player || !Player->IsLocalController()) continue;
        const float Distance = FVector::Distance(It->SmoothedFocus, Location);
        const float Attenuation = 1.f - FMath::Clamp(Distance / 2200.f, 0.f, 1.f);
        It->Impact = FMath::Max(It->Impact, .2f * FMath::Clamp(Strength, 0.f, 1.f) * Attenuation);
    }
}

void AAlphaCombatCamera::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    APlayerController* Player = Cast<APlayerController>(GetOwner());
    if (!Player || !Player->IsLocalController()) return;
    const AAlphaGameState* ChapterState = GetWorld()->GetGameState<AAlphaGameState>();
    const int32 Chapter = ChapterState ? ChapterState->ChapterIndex : 0;
    const FVector ChapterOrigin = AAlphaGameState::GetChapterOrigin(Chapter);
    if (LastChapter != Chapter)
    {
        LastChapter = Chapter;
        bInitialized = false; // Do not pan across the empty gap between chapters.
        Impact = 0.f;
        LastHealth = -1.f;
    }
    float MinimumX = TNumericLimits<float>::Max();
    float MaximumX = -TNumericLimits<float>::Max();
    const auto Include = [&MinimumX, &MaximumX](float X)
    {
        MinimumX = FMath::Min(MinimumX, X);
        MaximumX = FMath::Max(MaximumX, X);
    };
    for (TActorIterator<AAlphaCharacter> It(GetWorld()); It; ++It)
    {
        // Downed teammates stay in frame so rescuers can locate them.
        // Remote player controllers do not exist on every client. Team is replicated.
        if (It->GetCombatComponent()->TeamId == 1
            && FMath::Abs(It->GetActorLocation().X - ChapterOrigin.X) < 1700.)
            Include(static_cast<float>(It->GetActorLocation().X));
    }
    if (const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>())
    {
        if (IsValid(State->ForgeBoss) && State->ForgeBoss->GetCombatComponent()->IsAlive())
            Include(static_cast<float>(State->ForgeBoss->GetActorLocation().X));
    }
    if (MinimumX > MaximumX) MinimumX = MaximumX = static_cast<float>(ChapterOrigin.X);
    const float CenterX = FMath::Clamp((MinimumX + MaximumX) * .5f,
        static_cast<float>(ChapterOrigin.X) - 350.f, static_cast<float>(ChapterOrigin.X) + 350.f);
    const float HalfWidth = FMath::Max(FMath::Abs(MinimumX - CenterX), FMath::Abs(MaximumX - CenterX));
    int32 Width = 0, Height = 0;
    Player->GetViewportSize(Width, Height);
    const double Aspect = Width > 0 && Height > 0 ? static_cast<double>(Width) / Height : 16. / 9.;
    const double DesiredDistance = AlphaPresentation::CameraDistance(HalfWidth * 2., Aspect);
    const FVector DesiredFocus(CenterX, 70, 120);
    const double Weight = bInitialized ? AlphaPresentation::BlendWeight(DeltaSeconds, 3.5) : 1.;
    SmoothedFocus = FMath::Lerp(SmoothedFocus, DesiredFocus, Weight);
    SmoothedDistance = FMath::Lerp(SmoothedDistance, DesiredDistance, Weight);
    bInitialized = true;

    if (AAlphaCharacter* Pawn = Cast<AAlphaCharacter>(Player->GetPawn()))
    {
        const float Health = Pawn->GetCombatComponent()->GetHealth();
        if (LastPawn.Get() == Pawn && LastHealth >= 0.f && Health < LastHealth) Impact = .2f;
        LastHealth = Health;
        LastPawn = Pawn;
    }
    Impact = FMath::Max(0.f, Impact - DeltaSeconds);
    const float Envelope = static_cast<float>(AlphaPresentation::EffectEnvelope(.2f - Impact, .2f));
    const double Time = GetWorld()->GetTimeSeconds();
    const FVector Feedback(FMath::Sin(Time * 80.) * 7. * Envelope * ShakeStrength, 0,
        FMath::Cos(Time * 65.) * 4. * Envelope * ShakeStrength);
    const FVector Location = SmoothedFocus + FVector(0, -SmoothedDistance, SmoothedDistance * .39) + Feedback;
    SetActorLocationAndRotation(Location, (SmoothedFocus - Location).Rotation());
}
