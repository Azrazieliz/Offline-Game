#pragma once

#include "Components/ActorComponent.h"
#include "Runtime/OGFoundationCharacterDiagnostics.h"
#include "Runtime/OGFoundationStrategicRuntime.h"
#include "OGFoundationDiagnosticMenu.generated.h"

class UOGGameCoreSubsystem;
class UOGDiagnosticCombatComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOGFoundationCanonicalCharacterChanged, const FOGFoundationCharacterContext&);

struct OFFLINEGAME_API FOGFoundationDiagnosticMenuRow
{
    FName Id;
    FString Label;
    bool bEnabled = false;
    FString Reason;
};

/** Disposable authored controls; all commands use the ordinary canonical services. */
UCLASS(ClassGroup=(OfflineGame))
class OFFLINEGAME_API UOGFoundationDiagnosticMenu : public UActorComponent
{
    GENERATED_BODY()
public:
    UOGFoundationDiagnosticMenu();
    FOGFoundationCanonicalCharacterChanged OnCanonicalCharacterChanged;
    bool Initialize();
    TArray<FOGFoundationDiagnosticMenuRow> GetRows();
    bool ActivateRow(FName Id);
    FString GetStatus() const { return Status; }
private:
    enum class ECategory : uint8 { Home, Strategy, Character, Inventory, Knowledge };
    bool Resolve(UOGGameCoreSubsystem*& Core, UOGDiagnosticCombatComponent*& Combat,
        FOGFoundationCharacterContext& Character, int64& Tick, FString& Error) const;
    bool EnsureStrategy(UOGGameCoreSubsystem& Core, UOGDiagnosticCombatComponent& Combat,
        int64 Tick, FString& Error);
    TArray<FOGFoundationCharacterDiagnosticMenuEntry> CharacterEntries(
        const FOGFoundationCharacterContext& Character, IOGWorldStore& Store, FString& Error) const;
    ECategory Category = ECategory::Home;
    int32 Page = 0;
    FOGFoundationStrategicContext StrategyContext;
    FString Status;
};
