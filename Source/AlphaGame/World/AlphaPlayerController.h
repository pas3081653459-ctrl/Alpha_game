#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AlphaPlayerController.generated.h"

class UAlphaLobbyWidget;

UCLASS()
class ALPHAGAME_API AAlphaPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AAlphaPlayerController();
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    UFUNCTION(Exec, BlueprintCallable)
    void HostGame();
    UFUNCTION(Exec, BlueprintCallable)
    void JoinGame(const FString& Address);
    UFUNCTION(Exec, BlueprintCallable)
    void RestartRound();
    UFUNCTION(Exec, BlueprintCallable)
    void StartRun();
    UFUNCTION(Exec, BlueprintCallable)
    void SelectHero(int32 HeroIndex);
    UFUNCTION(Exec)
    void CameraShakeStrength(float Strength);
    void SetLobbyVisible(bool bVisible);
private:
    static bool IsValidLANAddress(const FString& Address);
    void ToggleLobby();
    UFUNCTION(Server, Reliable)
    void ServerSelectHero(int32 HeroIndex);
    UPROPERTY(Transient)
    TObjectPtr<UAlphaLobbyWidget> LobbyWidget;
};
