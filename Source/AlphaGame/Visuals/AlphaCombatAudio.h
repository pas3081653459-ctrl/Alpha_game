#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AlphaCombatAudio.generated.h"

class USoundBase;
class USoundAttenuation;

enum class EAlphaSoundCue : uint8 { Slash, Hit, Poise, Roll, Skill, Ultimate, Count };

/** Keeps loaded generated sounds reachable; import is exclusively an editor step. */
UCLASS()
class ALPHAGAME_API UAlphaCombatAudioLibrary : public UObject
{
    GENERATED_BODY()
public:
    UAlphaCombatAudioLibrary();
    USoundBase* GetSound(EAlphaSoundCue Cue);
    USoundAttenuation* GetAttenuation();
private:
    UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> Sounds;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Attenuation;
    TArray<double> RetryAt;
    TArray<bool> Warned;
};

namespace AlphaAudio
{
    ALPHAGAME_API bool SetVolume(float Volume);
    /** Call once from a confirmed cosmetic event. Never predicts or changes gameplay. */
    ALPHAGAME_API void Play(UWorld* World, const FVector& Location, EAlphaSoundCue Cue, float Volume = 1.f);
}
