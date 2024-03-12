
#pragma once

#include "CoreMinimal.h"
#include "Data/InventoryTags.h"
#include "GameFramework/SaveGame.h"

#include "InventorySave.generated.h"


USTRUCT(Blueprintable)
struct T5GINVENTORYSYSTEM_API FItemStatics
{
	GENERATED_BODY();
	
	// The name of the Data Asset for this item
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FName			ItemName = FName();
	
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FGameplayTag	Rarity = TAG_Item_Rarity_Trash;
	
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) float			Durability = -1.f;
	
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FString		CrafterName = "";
	
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FDateTime		CraftTimestamp = FDateTime();
	
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) bool			bIsEquipped = false;

	
	bool operator==(const FItemStatics& InItemStatics) const
	{
		if (InItemStatics.ItemName != this->ItemName)
		{ return false; }
		if (!InItemStatics.Rarity.MatchesTagExact(this->Rarity))
		{ return false; }
		if (InItemStatics.Durability != this->Durability)
		{ return false; }
		return true;
	}
	
};

USTRUCT()
struct T5GINVENTORYSYSTEM_API FInventorySlotSaveData
{
	GENERATED_BODY();
	UPROPERTY(SaveGame) TArray<FItemStatics> SavedItemStatics;
};

USTRUCT()
struct T5GINVENTORYSYSTEM_API FInventorySaveData
{
	GENERATED_BODY();
	UPROPERTY(SaveGame)	int		Setting_NumberOfSlots		= 24;
	UPROPERTY(SaveGame)	bool	Setting_bReadOnlyInventory	= false;
};


/**
 * A child class of 'USaveGame' used as the save game for inventory systems
 */
UCLASS(Blueprintable, BlueprintType)
class T5GINVENTORYSYSTEM_API UInventorySave : public USaveGame
{
	GENERATED_BODY()
	
public:
	
	UInventorySave() {};

	UPROPERTY(SaveGame) FInventorySaveData InventoryData = {};
	UPROPERTY(SaveGame) TArray<FInventorySlotSaveData> InventorySlots = {};
	
};