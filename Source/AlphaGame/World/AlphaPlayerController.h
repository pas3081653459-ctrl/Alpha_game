#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AlphaPlayerController.generated.h"

class UAlphaLobbyWidget;
class UAlphaDialogueWidget;

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
    void ContinueChapter();
    UFUNCTION(Exec, BlueprintCallable)
    void InteractStory();
    void FinishStoryDialogue(bool bAdvance);
    static bool IsValidLANAddress(const FString& Address);
    UFUNCTION(Exec, BlueprintCallable)
    void SelectHero(int32 HeroIndex);
    UFUNCTION(Exec, BlueprintCallable)
    void SetBotFill(int32 Enabled);
    /** Development-only rescue exercise; damage is calculated on the authority. */
    UFUNCTION(Exec)
    void TestDownSelf();
    UFUNCTION(Exec)
    void CameraShakeStrength(float Strength);
    void SetLobbyVisible(bool bVisible);
private:
    void ToggleLobby();
    void UpdateUIInputMode();
    void HideStoryDialogue();
    UFUNCTION(Server, Reliable)
    void ServerInteractStory();
    UFUNCTION(Server, Reliable)
    void ServerFinishStoryDialogue(int32 Chapter, bool bAdvance);
    UFUNCTION(Client, Reliable)
    void ClientOpenStoryDialogue(int32 Chapter, bool bDebrief, int32 Revision);
    UFUNCTION(Server, Reliable)
    void ServerSelectHero(int32 HeroIndex);
    UFUNCTION(Server, Reliable)
    void ServerTestDownSelf();
    UPROPERTY(Transient)
    TObjectPtr<UAlphaLobbyWidget> LobbyWidget;
    UPROPERTY(Transient)
    TObjectPtr<UAlphaDialogueWidget> DialogueWidget;
    int32 VisibleStoryChapter = -1;
    int32 ServerStoryChapter = -1;
    uint8 ServerStoryStage = 0;
    int32 ServerStoryRevision = -1;
    double LastStoryRequestAt = -1000.;
};
