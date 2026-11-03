#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AlphaHUD.generated.h"

UCLASS()
class ALPHAGAME_API AAlphaHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
