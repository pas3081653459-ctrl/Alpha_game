#include "Visuals/AlphaCombatAudio.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
constexpr int32 CueCount = static_cast<int32>(EAlphaSoundCue::Count);
const TCHAR* CueNames[] = {TEXT("S_AlphaSlash"), TEXT("S_AlphaHit"), TEXT("S_AlphaPoise"),
    TEXT("S_AlphaRoll"), TEXT("S_AlphaSkill"), TEXT("S_AlphaUltimate")};
constexpr double CueIntervals[] = {.045, .035, .12, .08, .09, .25};
float MasterVolume = .75f;

struct FWorldAudioBudget
{
    TWeakObjectPtr<UWorld> World;
    TArray<double> PlayingUntil;
    double LastCueAt[CueCount] = {-1000., -1000., -1000., -1000., -1000., -1000.};
};
TArray<FWorldAudioBudget> WorldBudgets;

FAutoConsoleCommand VolumeCommand(TEXT("AlphaAudioVolume"),
    TEXT("AlphaAudioVolume 0..1: local combat sound volume; 0 mutes newly triggered cues."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() != 1 || !Args[0].IsNumeric())
        {
            UE_LOG(LogTemp, Display, TEXT("Usage: AlphaAudioVolume 0..1 (0 mutes combat cues)."));
            return;
        }
        const float Requested = FCString::Atof(*Args[0]);
        if (FMath::IsFinite(Requested)) MasterVolume = FMath::Clamp(Requested, 0.f, 1.f);
        UE_LOG(LogTemp, Display, TEXT("AlphaGame local combat audio volume: %.2f"), MasterVolume);
    }));
}

UAlphaCombatAudioLibrary::UAlphaCombatAudioLibrary()
{
    Sounds.SetNum(CueCount);
    RetryAt.Init(0., CueCount);
    Warned.Init(false, CueCount);
}

USoundBase* UAlphaCombatAudioLibrary::GetSound(EAlphaSoundCue Cue)
{
    const int32 Index = static_cast<int32>(Cue);
    if (Index < 0 || Index >= CueCount) return nullptr;
    const double Now = FPlatformTime::Seconds();
    if (!Sounds[Index] && Now >= RetryAt[Index])
    {
        RetryAt[Index] = Now + 5.;
        const FString Path = FString::Printf(TEXT("/Game/Audio/Generated/%s.%s"), CueNames[Index], CueNames[Index]);
        Sounds[Index] = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (!Sounds[Index] && !Warned[Index])
        {
            Warned[Index] = true;
            UE_LOG(LogTemp, Warning, TEXT("AlphaGame sound missing: %s. Run Scripts/create_audio_assets.py in the UE editor; audio save is not playback verification."), *Path);
        }
    }
    return Sounds[Index].Get();
}

USoundAttenuation* UAlphaCombatAudioLibrary::GetAttenuation()
{
    if (!Attenuation)
    {
        Attenuation = NewObject<USoundAttenuation>(this, NAME_None, RF_Transient);
        // The listener is the shared side camera, thousands of cm from actors.
        // Broad attenuation preserves audibility while avoiding global full volume.
        Attenuation->Attenuation.bAttenuate = true;
        Attenuation->Attenuation.bSpatialize = true;
        Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
        Attenuation->Attenuation.AttenuationShapeExtents = FVector(1800.f, 0.f, 0.f);
        Attenuation->Attenuation.FalloffDistance = 8000.f;
    }
    return Attenuation.Get();
}

bool AlphaAudio::SetVolume(float Volume)
{
    if (!FMath::IsFinite(Volume)) return false;
    MasterVolume = FMath::Clamp(Volume, 0.f, 1.f);
    return true;
}

void AlphaAudio::Play(UWorld* World, const FVector& Location, EAlphaSoundCue Cue, float Volume)
{
    const int32 Index = static_cast<int32>(Cue);
    if (!World || World->GetNetMode() == NM_DedicatedServer || Location.ContainsNaN()
        || Index < 0 || Index >= CueCount || !FMath::IsFinite(Volume) || Volume <= 0.f || MasterVolume <= 0.f) return;
    // Ignore sounds in another distant chapter, even if their actors share a world.
    const APlayerController* Listener = World->GetFirstPlayerController();
    if (!Listener || !Listener->IsLocalController()) return;
    if (const APawn* Pawn = Listener->GetPawn())
        if (FVector::DistSquared2D(Pawn->GetActorLocation(), Location) > FMath::Square(5000.)) return;

    WorldBudgets.RemoveAll([](const FWorldAudioBudget& Budget) { return !Budget.World.IsValid(); });
    FWorldAudioBudget* Budget = WorldBudgets.FindByPredicate([World](const FWorldAudioBudget& Entry) { return Entry.World.Get() == World; });
    if (!Budget)
    {
        FWorldAudioBudget Fresh;
        Fresh.World = World;
        Budget = &WorldBudgets.Add_GetRef(Fresh);
    }
    const double Now = World->GetTimeSeconds();
    Budget->PlayingUntil.RemoveAll([Now](double Until) { return Until <= Now; });
    // Estimate active one-shots by source duration. This is a presentation
    // admission budget, not a claim to control the engine's global voice mixer.
    if (Budget->PlayingUntil.Num() >= 12 || Now - Budget->LastCueAt[Index] < CueIntervals[Index]) return;
    UAlphaCombatAudioLibrary* Library = GetMutableDefault<UAlphaCombatAudioLibrary>();
    USoundBase* Sound = Library->GetSound(Cue);
    if (!Sound) return;
    Budget->LastCueAt[Index] = Now;
    const float Duration = Sound->GetDuration();
    Budget->PlayingUntil.Add(Now + (FMath::IsFinite(Duration) && Duration > 0.f ? FMath::Clamp(Duration, .05f, 2.f) : .5f));
    UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, FRotator::ZeroRotator,
        FMath::Clamp(Volume, 0.f, 1.f) * MasterVolume, 1.f, 0.f, Library->GetAttenuation());
}
