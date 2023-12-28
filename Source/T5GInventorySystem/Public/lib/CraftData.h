/*
*	Crafting Data Function Library
*
*	Contains Structs, Enums, and public Classes regarding crafting and items
*   containing crafting data.
*
*	CHANGELOG
*		version 1.0		Melanie Harris		09DEC2022		Created file and contents
*/
#pragma once

#include "CoreMinimal.h"
#include "ItemData.h"
#include "Engine/DataTable.h"

#include "CraftData.generated.h"

UENUM(BlueprintType)
enum class ECraftingType : uint8
{
	NONE		UMETA(DisplayName = "No Crafting"),
	PLAYER		UMETA(DisplayName = "Player Inventory"),
	CAMPFIRE	UMETA(DisplayName = "Campfire"),
	FORGE		UMETA(DisplayName = "Forge"),
	GRILLE		UMETA(DisplayName = "Grill / Stove"),
	LOOM		UMETA(DisplayName = "Loom / Tailoring"),
	ANVIL		UMETA(DisplayName = "Blacksmith Anvil"),
	WORKBENCH	UMETA(DisplayName = "Workbench"),
	ADVWORK		UMETA(DisplayName = "Advanced Workbench")
};

// FCrafterData
// Contains all data related to an item's data when crafted by a player
// If the item is NOT crafted, 'ItemName' will be None.
USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStCrafterData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText itemName		= FText::FromString("None");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText crafterName	= FText::FromString("None");
};

USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStCraftRecycleData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName itemName = "None";

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int giveQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool spawnsInWorld = false;
};

// Simple struct for replication
USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStCraftQueueData
{
	GENERATED_BODY()

	FStCraftQueueData() : ticksCompleted(0) {};
	FStCraftQueueData(const UCraftingItemData* NewData)
		: ItemAsset(NewData), ticksCompleted(0) {};
	
	// The item name being crafted (also the row name for the data tables)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) const UCraftingItemData* ItemAsset = nullptr;
	
	// How many crafting ticks have passed
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int   ticksCompleted;
	
};


UCLASS()
class T5GINVENTORYSYSTEM_API UCraftSystem : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
};