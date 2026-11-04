#include "World/AlphaProfileSave.h"

AlphaProfileRules::Data UAlphaProfileSave::GetData() const
{
    return {SchemaVersion, HighestUnlockedChapter, LastChapter, PreferredHero, AudioVolume, CameraShake};
}

void UAlphaProfileSave::SetData(const AlphaProfileRules::Data& Data)
{
    SchemaVersion = Data.Version;
    HighestUnlockedChapter = Data.HighestUnlocked;
    LastChapter = Data.LastChapter;
    PreferredHero = Data.Hero;
    AudioVolume = static_cast<float>(Data.Volume);
    CameraShake = static_cast<float>(Data.Shake);
}

bool UAlphaProfileSave::IsCompatible() const
{
    return GetClass() == UAlphaProfileSave::StaticClass() && AlphaProfileRules::Compatible(GetData());
}
