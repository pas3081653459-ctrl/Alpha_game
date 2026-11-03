#include "World/AlphaGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UAlphaGameInstance::Init()
{
    Super::Init();
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
