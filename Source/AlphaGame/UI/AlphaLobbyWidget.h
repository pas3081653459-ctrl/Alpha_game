#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AlphaLobbyWidget.generated.h"

class UEditableTextBox;
class UTextBlock;
class UButton;
class UVerticalBox;
class UBorder;
class USizeBox;

/** Responsive startup and party UI; all progression stays on the authority. */
UCLASS()
class ALPHAGAME_API UAlphaLobbyWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void PrepareForDisplay();
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
private:
    UButton* AddButton(UVerticalBox* Panel, const FString& Label);
    UTextBlock* AddText(UVerticalBox* Panel, const FString& Label, int32 FontSize, FLinearColor Color);
    void UpdateResponsiveLayout(float Width);
    UFUNCTION() void Host();
    UFUNCTION() void Join();
    UFUNCTION() void Start();
    UFUNCTION() void Continue();
    UFUNCTION() void Restart();
    UFUNCTION() void ToggleLAN();
    UFUNCTION() void ToggleCompanions();
    UFUNCTION() void HeroBlade();
    UFUNCTION() void HeroWeaver();
    UFUNCTION() void HeroLantern();
    UFUNCTION() void Close();
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> AddressBox;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ChapterText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StoryText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ObjectiveText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ErrorText;
    UPROPERTY(Transient) TObjectPtr<UButton> StartButton;
    UPROPERTY(Transient) TObjectPtr<UButton> ContinueButton;
    UPROPERTY(Transient) TObjectPtr<UButton> ExploreButton;
    UPROPERTY(Transient) TObjectPtr<UButton> RestartButton;
    UPROPERTY(Transient) TObjectPtr<UButton> CompanionButton;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> HeroButtons;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> LANPanel;
    UPROPERTY(Transient) TObjectPtr<UBorder> Backdrop;
    UPROPERTY(Transient) TObjectPtr<UBorder> StoryCard;
    UPROPERTY(Transient) TObjectPtr<UBorder> PartyCard;
    UPROPERTY(Transient) TObjectPtr<USizeBox> LayoutFrame;
    FString LocalError;
    bool bLANVisible = false;
    bool bCompactLayout = false;
    bool bHasLayout = false;
    bool bObservedPreparation = false;
};
