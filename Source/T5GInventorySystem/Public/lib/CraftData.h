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

	FStCraftQueueData() {};
	FStCraftQueueData(FName iName, UTexture2D* iTexture)
	{
		itemName = iName;
		itemTexture = iTexture;
	}
	
	// The item name being crafted (also the row name for the data tables)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName itemName			= FName();
	
	// The image to show in the crafting panel
	UPROPERTY(EditAnywhere, BlueprintReadWrite) UTexture2D* itemTexture	= nullptr;
	
	// How many crafting ticks have passed
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int   ticksCompleted	= 0;
	
};

USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStCraftingRecipe : public FTableRowBase
{
	GENERATED_BODY()

	// What item is created when complete
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName itemName = FName();

	// How many of this item is created upon success
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int createsQuantity = 1;

	// How many seconds it takes to complete this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int ticksToComplete = 1;

	// How many ticks between ingredient consumption
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int tickConsume = 1;

	// The items required to create this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, int> craftingRecipe;

	// The type of workstation required to see this recipe
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<ECraftingType> craftingTypes;
	
};


UCLASS()
class T5GINVENTORYSYSTEM_API UCraftSystem : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	// Returns a pointer to the item data table for manual operations
	UFUNCTION(BlueprintPure, Category = "Recipe Data System Globals")
		static UDataTable* getRecipeDataTable();
	
	// Gets the recipe data structure from the given item name ('food_carrot')
	// Performs an O(1) lookup, but retrieves data, which is slower than direct access.
	UFUNCTION(BlueprintPure, Category = "Recipe Data System Globals")
		static FStCraftingRecipe getRecipeFromItemName(FName itemName);
	
	UFUNCTION(BlueprintPure, Category = "Recipe Data System Globals")
		static bool getItemHasRecipe(FName itemName);

	UFUNCTION(BlueprintPure, Category = "Recipe Data System Globals")
		static bool getRecipeIsValid(FStCraftingRecipe recipeData);
};