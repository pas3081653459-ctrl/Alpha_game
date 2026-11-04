#if WITH_DEV_AUTOMATION_TESTS

#include "World/AlphaProfileSave.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlphaProfileSerializationTest,
    "AlphaGame.Profile.Serialization.MemoryRoundTripAndUnsupportedData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlphaProfileSerializationTest::RunTest(const FString& Parameters)
{
    // Never initialize the production GameInstance or touch AlphaGameLocalProfile:
    // these tests cannot overwrite a developer's real progress/preferences.
    UAlphaProfileSave* Profile = Cast<UAlphaProfileSave>(
        UGameplayStatics::CreateSaveGameObject(UAlphaProfileSave::StaticClass()));
    if (!TestNotNull(TEXT("Profile SaveGame object exists"), Profile)) return false;
    Profile->HighestUnlockedChapter = 2;
    Profile->LastChapter = 1;
    Profile->PreferredHero = 2;
    Profile->AudioVolume = .25f;
    Profile->CameraShake = 0.f;
    TestTrue(TEXT("Known valid values are compatible before serialization"), Profile->IsCompatible());
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Real SaveGame memory serialization succeeds"), UGameplayStatics::SaveGameToMemory(Profile, Bytes))) return false;
    UAlphaProfileSave* Loaded = Cast<UAlphaProfileSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Real memory deserialization preserves class"), Loaded)) return false;
    TestTrue(TEXT("Deserialized known schema remains compatible"), Loaded->IsCompatible());
    TestEqual(TEXT("Unlocks survive serialization"), Loaded->HighestUnlockedChapter, 2);
    TestEqual(TEXT("Replay choice survives serialization"), Loaded->LastChapter, 1);
    TestEqual(TEXT("Hero choice survives serialization"), Loaded->PreferredHero, 2);
    TestEqual(TEXT("Sound preference survives serialization"), Loaded->AudioVolume, .25f);
    TestEqual(TEXT("Disabled shake survives serialization"), Loaded->CameraShake, 0.f);

    Profile->SchemaVersion = 99;
    Bytes.Reset();
    if (!TestTrue(TEXT("Future-schema fixture serializes without using a disk slot"), UGameplayStatics::SaveGameToMemory(Profile, Bytes))) return false;
    UAlphaProfileSave* Future = Cast<UAlphaProfileSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Unsupported data remains available for safe validation"), Future)) return false;
    TestEqual(TEXT("Unknown version is not silently migrated or overwritten"), Future->SchemaVersion, 99);
    TestFalse(TEXT("Future schema is rejected by the same validator as production load"), Future->IsCompatible());

    Profile->SchemaVersion = 1;
    Profile->AudioVolume = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Nonfinite data cannot become a valid profile"), Profile->IsCompatible());
    Profile->AudioVolume = .75f;
    Profile->LastChapter = 3;
    TestFalse(TEXT("Nonexistent chapter cannot become a replay preference"), Profile->IsCompatible());
    return true;
}

#endif
