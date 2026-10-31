#pragma once
#include "CoreMinimal.h"
#include "Core/OGEntityId.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "OGNativeBattleWidget.generated.h"

class AOGWorldPrototypeCharacter;
class UOGNativeBattleWidget;
class UTextBlock;
class UProgressBar;
class UScrollBox;
class UWrapBox;
class UVerticalBox;
class UCanvasPanelSlot;

UCLASS()
class OFFLINEGAME_API UOGNativeBattleButton : public UButton
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<UOGNativeBattleWidget> Screen;
    FName Command;
    int32 TargetIndex = INDEX_NONE;
    UFUNCTION() void Invoke();
};

/** Touch-first native commands over the actual rendered World and canonical battle. */
UCLASS()
class OFFLINEGAME_API UOGNativeBattleWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void BindCharacter(AOGWorldPrototypeCharacter* InCharacter);
    void RefreshBattle() { Signature.Reset(); Refresh(); }
    bool IsForCharacter(const AOGWorldPrototypeCharacter* InCharacter) const;
    void Activate(FName Command,int32 TargetIndex);
protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
private:
    void Refresh();
    void AddButton(class UPanelWidget* Parent,const FString& Label,const FString& Hint,
        FName Command,int32 TargetIndex,bool Enabled,bool Selected,float Width);
    TWeakObjectPtr<AOGWorldPrototypeCharacter> Character;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Heading;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Resources;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> UltimateBar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Feedback;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> Targets;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> ActionScroll;
    UPROPERTY(Transient) TObjectPtr<UWrapBox> Commands;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> ActionPanelSlot;
    FOGEntityId DismissedResultReceipt;
    FString Signature;
    FString LastActionMessage;
    float RefreshElapsed=0;
    float ViewWidth=0;
    float ViewHeight=0;
    bool bActionAnchorsApplied=false;
    bool bActionPortraitLayout=false;
};
