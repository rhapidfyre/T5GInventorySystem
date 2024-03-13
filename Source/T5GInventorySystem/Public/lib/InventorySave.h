
#pragma once

#include "CoreMinimal.h"
#include "Data/ItemStatics.h"
#include "GameFramework/SaveGame.h"

#include "InventorySave.generated.h"


USTRUCT()
struct T5GINVENTORYSYSTEM_API FInventorySlotSaveData
{
	GENERATED_BODY();
	FInventorySlotSaveData() : Quantity(0), SavedAssetId({}) {}

	UPROPERTY(SaveGame)	int Quantity;
	UPROPERTY(SaveGame) FItemStatics			SavedItemStatics;
	UPROPERTY(SaveGame) FPrimaryAssetId			SavedAssetId;
	UPROPERTY(SaveGame) FGameplayTagContainer	SavedSlotTags;
};


USTRUCT()
struct T5GINVENTORYSYSTEM_API FInventorySaveData
{
	GENERATED_BODY();
	FInventorySaveData() :
		Setting_NumberOfSlots(24), Setting_bReadOnlyInventory(false) {}

	UPROPERTY(SaveGame)	int		Setting_NumberOfSlots;
	UPROPERTY(SaveGame)	bool	Setting_bReadOnlyInventory;
};


/**
 * A child class of 'USaveGame' used as the save game for inventory systems
 */
UCLASS(Blueprintable, BlueprintType)
class T5GINVENTORYSYSTEM_API UInventorySave : public USaveGame
{
	GENERATED_BODY()

public:

	UInventorySave() : SavedInventoryData({}), SavedInventorySlots({}) {};

	UPROPERTY(SaveGame) FInventorySaveData SavedInventoryData = {};
	UPROPERTY(SaveGame) TArray<FInventorySlotSaveData> SavedInventorySlots = {};
	
};