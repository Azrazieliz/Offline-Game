#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OGWorldModeInterfaces.generated.h"

UINTERFACE(BlueprintType)
class OFFLINEGAME_API UOGWorldInteractable : public UInterface
{
    GENERATED_BODY()
};

class OFFLINEGAME_API IOGWorldInteractable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|World|Interaction")
    bool CanInteract(AActor* Interactor) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|World|Interaction")
    FText GetInteractionPrompt(AActor* Interactor) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|World|Interaction")
    void Interact(AActor* Interactor);
};

UINTERFACE(BlueprintType)
class OFFLINEGAME_API UOGWorldTargetable : public UInterface
{
    GENERATED_BODY()
};

class OFFLINEGAME_API IOGWorldTargetable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|World|Targeting")
    bool CanBeTargeted(AActor* Requester) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|World|Targeting")
    FVector GetTargetPoint(AActor* Requester) const;
};

/**
 * Map-authored playable-boundary contract. Implementations own the shape and
 * may intentionally allow arbitrarily deep vertical traversal; Foundation only
 * asks whether an XY/world location is legal and whether it is a stable rollback
 * point.
 */
UINTERFACE(BlueprintType)
class OFFLINEGAME_API UOGPlayableBoundaryProvider : public UInterface
{
    GENERATED_BODY()
};

class OFFLINEGAME_API IOGPlayableBoundaryProvider
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|World|Safety")
    bool IsInsidePlayableBoundary(const FVector& WorldLocation) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="OfflineGame|World|Safety")
    bool IsInsideStableGroundRegion(const FVector& WorldLocation) const;
};
