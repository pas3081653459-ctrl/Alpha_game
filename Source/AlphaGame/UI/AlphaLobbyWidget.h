#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AlphaLobbyWidget.generated.h"

class UEditableTextBox;
class UTextBlock;
class UButton;
class UVerticalBox;

/** Asset-free prototype lobby. Network authority stays in the player controller. */
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
    UFUNCTION() void Host();
    UFUNCTION() void Join();
    UFUNCTION() void Start();
    UFUNCTION() void Restart();
    UFUNCTION() void HeroBlade();
    UFUNCTION() void HeroWeaver();
    UFUNCTION() void HeroLantern();
    UFUNCTION() void Close();
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> AddressBox;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(Transient) TObjectPtr<UButton> StartButton;
    UPROPERTY(Transient) TObjectPtr<UButton> RestartButton;
    bool bObservedPreparation = false;
};
