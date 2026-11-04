#include "World/AlphaGameInstance.h"
#include "World/AlphaProfileSave.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UAlphaGameInstance::Init()
{
    Super::Init();
    LoadLocalProfile();
    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UAlphaGameInstance::HandleNetworkFailure);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UAlphaGameInstance::HandleTravelFailure);
    }
}

void UAlphaGameInstance::Shutdown()
{
    GetTimerManager().ClearTimer(RecoveryTimer);
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }
    Super::Shutdown();
}

void UAlphaGameInstance::BeginConnectionAttempt()
{
    GetTimerManager().ClearTimer(RecoveryTimer);
    LastConnectionError.Reset();
    bRecoveryAttempted = false;
}

void UAlphaGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error)
{
    // Engine failure delegates are global; another PIE world must not affect us.
    if (World && World->GetGameInstance() != this) return;
    LastConnectionError = FString::Printf(TEXT("Network error %d: %s"), static_cast<int32>(FailureType), *Error.Left(220));
    UE_LOG(LogTemp, Warning, TEXT("AlphaGame: %s"), *LastConnectionError);
    ScheduleLocalRecovery(World);
}

void UAlphaGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
    if (World && World->GetGameInstance() != this) return;
    LastConnectionError = FString::Printf(TEXT("Travel error %d: %s"), static_cast<int32>(FailureType), *Error.Left(220));
    UE_LOG(LogTemp, Warning, TEXT("AlphaGame: %s"), *LastConnectionError);
    ScheduleLocalRecovery(World);
}

void UAlphaGameInstance::ScheduleLocalRecovery(UWorld* FailedWorld)
{
    UWorld* World = FailedWorld ? FailedWorld : GetWorld();
    if (!World || World->GetGameInstance() != this) return;
    // Losing one client must never tear down the host's ongoing game.
    if (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer) return;
    if (bRecoveryAttempted) return;
    bRecoveryAttempted = true;
    // Defer travel until the engine finishes broadcasting this failure.
    RecoveryTimer = GetTimerManager().SetTimerForNextTick(this, &UAlphaGameInstance::ReturnToLocalArena);
}

void UAlphaGameInstance::ReturnToLocalArena()
{
    if (!GetWorld()) return;
    // Absolute travel clears the old listen/address/options. The guard remains
    // set until an explicit HostGame/JoinGame, so recovery itself cannot loop.
    UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true);
}

namespace AlphaProfileStorage
{
const FString SlotName(TEXT("AlphaGameLocalProfile"));
constexpr int32 UserIndex = 0;

bool Same(const AlphaProfileRules::Data& A, const AlphaProfileRules::Data& B)
{
    return A.Version == B.Version && A.HighestUnlocked == B.HighestUnlocked && A.LastChapter == B.LastChapter
        && A.Hero == B.Hero && A.Volume == B.Volume && A.Shake == B.Shake;
}
}

void UAlphaGameInstance::LoadLocalProfile()
{
    if (bProfileLoaded) return;
    bProfileLoaded = true;
    LocalProfile = NewObject<UAlphaProfileSave>(this);
    bProfileReadOnly = false;
    bProfileNeedsSave = false;
    ProfileStatusError.Reset();
    if (!UGameplayStatics::DoesSaveGameExist(AlphaProfileStorage::SlotName, AlphaProfileStorage::UserIndex))
    {
        bProfileNeedsSave = true;
        CommitProfile(LocalProfile->GetData());
        return;
    }
    USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(AlphaProfileStorage::SlotName, AlphaProfileStorage::UserIndex);
    UAlphaProfileSave* Profile = Cast<UAlphaProfileSave>(Loaded);
    if (!Profile || !Profile->IsCompatible())
    {
        // Unknown class/version and malformed fields are not a supported migration.
        // Keep the original file, use defaults in memory and block implicit writes.
        bProfileReadOnly = true;
        ProfileStatusError = TEXT("本地档案损坏或版本不兼容，已使用临时默认设置；原存档保留，本次不会覆盖。 ");
        UE_LOG(LogTemp, Warning, TEXT("AlphaGame profile: existing slot could not be validated; preserved without writes."));
        return;
    }
    LocalProfile = Profile;
}

bool UAlphaGameInstance::CommitProfile(const AlphaProfileRules::Data& Data, bool bExplicitReset)
{
    if (!bProfileLoaded && !bExplicitReset) return false;
    if (!AlphaProfileRules::Compatible(Data))
    {
        ProfileStatusError = TEXT("设置或章节数据无效，未保存。");
        return false;
    }
    if (bProfileReadOnly && !bExplicitReset) return false;
    if (!bExplicitReset && !bProfileNeedsSave && LocalProfile && AlphaProfileStorage::Same(Data, LocalProfile->GetData()))
    {
        ProfileStatusError.Reset();
        return true;
    }
    AlphaProfileRules::Data CandidateData = Data;
    if (!bExplicitReset && UGameplayStatics::DoesSaveGameExist(AlphaProfileStorage::SlotName, AlphaProfileStorage::UserIndex))
    {
        UAlphaProfileSave* Latest = Cast<UAlphaProfileSave>(UGameplayStatics::LoadGameFromSlot(
            AlphaProfileStorage::SlotName, AlphaProfileStorage::UserIndex));
        if (!Latest || !Latest->IsCompatible())
        {
            bProfileReadOnly = true;
            ProfileStatusError = TEXT("磁盘中的档案已变更或不兼容，停止保存以保留原文件。");
            return false;
        }
        // Separate PIE/local game instances may share this user's slot. A
        // settings write must not lower unlocks saved by a newer instance.
        CandidateData.HighestUnlocked = FMath::Max(CandidateData.HighestUnlocked, Latest->HighestUnlockedChapter);
    }
    UAlphaProfileSave* Candidate = NewObject<UAlphaProfileSave>(this);
    Candidate->SetData(CandidateData);
    if (!UGameplayStatics::SaveGameToSlot(Candidate, AlphaProfileStorage::SlotName, AlphaProfileStorage::UserIndex))
    {
        // Do not commit a UI preference or unlock to memory while reporting failure.
        ProfileStatusError = TEXT("无法写入本地档案，设置未保存。请检查存储空间及保存目录权限。");
        UE_LOG(LogTemp, Warning, TEXT("AlphaGame profile: SaveGameToSlot failed."));
        return false;
    }
    LocalProfile = Candidate;
    bProfileLoaded = true;
    bProfileReadOnly = false;
    bProfileNeedsSave = false;
    ProfileStatusError.Reset();
    return true;
}

int32 UAlphaGameInstance::GetHighestUnlockedChapter() const
{
    return LocalProfile ? LocalProfile->HighestUnlockedChapter : 0;
}

int32 UAlphaGameInstance::GetLastChapter() const
{
    return LocalProfile ? LocalProfile->LastChapter : 0;
}

int32 UAlphaGameInstance::GetPreferredHero() const
{
    return LocalProfile ? LocalProfile->PreferredHero : 0;
}

float UAlphaGameInstance::GetAudioVolume() const
{
    return LocalProfile ? LocalProfile->AudioVolume : .75f;
}

float UAlphaGameInstance::GetCameraShake() const
{
    return LocalProfile ? LocalProfile->CameraShake : .35f;
}

bool UAlphaGameInstance::RecordChapterComplete(int32 Chapter)
{
    LoadLocalProfile();
    if (!AlphaProfileRules::ValidChapter(Chapter))
    {
        if (!bProfileReadOnly) ProfileStatusError = TEXT("章节编号无效，未保存。");
        return false;
    }
    const AlphaProfileRules::Data Current = LocalProfile ? LocalProfile->GetData() : AlphaProfileRules::Data{};
    return CommitProfile(AlphaProfileRules::AfterCompletion(Current, Chapter));
}

bool UAlphaGameInstance::SetPreferredChapter(int32 Chapter)
{
    LoadLocalProfile();
    AlphaProfileRules::Data Data = LocalProfile ? LocalProfile->GetData() : AlphaProfileRules::Data{};
    if (!AlphaProfileRules::CanSelectChapter(Data, Chapter))
    {
        if (!bProfileReadOnly) ProfileStatusError = TEXT("该章节尚未解锁，未保存章节选择。");
        return false;
    }
    Data.LastChapter = Chapter;
    return CommitProfile(Data);
}

bool UAlphaGameInstance::SetPreferredHero(int32 Hero)
{
    LoadLocalProfile();
    if (Hero < 0 || Hero > 2)
    {
        if (!bProfileReadOnly) ProfileStatusError = TEXT("角色编号无效，未保存。");
        return false;
    }
    AlphaProfileRules::Data Data = LocalProfile ? LocalProfile->GetData() : AlphaProfileRules::Data{};
    Data.Hero = Hero;
    return CommitProfile(Data);
}

bool UAlphaGameInstance::SetPresentationSettings(float Volume, float Shake)
{
    LoadLocalProfile();
    if (!AlphaProfileRules::ValidPresentation(Volume) || !AlphaProfileRules::ValidPresentation(Shake))
    {
        if (!bProfileReadOnly) ProfileStatusError = TEXT("音量和镜头震动必须在0到1之间，未保存。");
        return false;
    }
    AlphaProfileRules::Data Data = LocalProfile ? LocalProfile->GetData() : AlphaProfileRules::Data{};
    Data.Volume = Volume;
    Data.Shake = Shake;
    return CommitProfile(Data);
}

bool UAlphaGameInstance::ResetLocalProfile()
{
    return CommitProfile(AlphaProfileRules::Data{}, true);
}
