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
enum class ECraftingQuality : uint8
{
	DENSITY UMETA(DisplayName = "Density", Description = "Weight of the Item. A High quality of density means the item will weigh less"),
	GRIP    UMETA(DisplayName = "Grip", Description = "A high quality of grip makes the weapon easier to grip, reducing time between attacks"),
	SIGHTS  UMETA(DisplayName = "Sights", Description = "A high sight quality makes weapons easier to aim, making ranged weapon damage closer to it's true value"),
	BALANCE UMETA(DisplayName = "Balance", Description = "A high balance quality makes weapons easier to handle, making melee weapon damage closer to it's true value"),
	DURABILITY UMETA(DisplayName = "Durability", Description = "A high durability means the item will degrade slower, getting more use out of the item")
};

UENUM(BlueprintType)
enum class ECraftingSkill : uint8
{
	GENERAL		UMETA(DisplayName = "Generic", Description = "Trivial crafting such as campfires and bandages"),
	SHIPWRIGHT	UMETA(DisplayName = "Shipwright", Description = "Crafting starships and starship parts"),
	GUNSMITH	UMETA(DisplayName = "Gunsmithing", Description = "Weapons and gunparts in relation to combine, blast or percussive weapons"),
	ORDNANCE	UMETA(DisplayName = "Ordnance", Description = "Weapons and Devices that utilize explosives, such as grenades and breaching charges"),
	COOKING		UMETA(DisplayName = "Cooking", Description = "Grilling, baking, cooking, frying - Crafting food and edible items"),
	BREWING		UMETA(DisplayName = "Brewing", Description = "Creating drinks, such as beer, tea, coffee and cocktails"),
	ENGINEER	UMETA(DisplayName = "Engineering", Description = "Vehicles and their parts, such as land speeders and hovercrafts"),
	BLACKSMITH	UMETA(DisplayName = "Blacksmithing", Description = "Weapons and parts in relation to martial weapons, such as swords and daggers"),
	ENERGETICS	UMETA(DisplayName = "Energetics", Description = "Combining the galactic energies with items to create energized materials"),
	EXTRICATION	UMETA(DisplayName = "Extrication", Description = "The act of extracting valuable minerals from raw materials"),
	ARCHITECT	UMETA(DisplayName = "Architecture", Description = "Crafting buildings, houses, grand halls and more for housing needs"),
	TAILOR		UMETA(DisplayName = "Tailoring", Description = "Crafting cosmetic items, such as entertainer outfits, capes and everyday wear"),
	ARMORER		UMETA(DisplayName = "Armoring", Description = "Crafting equipment used to protect the wearer")
};

USTRUCT(BlueprintType)
struct FStCraftQuality : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName qualityName = "Durability";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float qualityValue = 0.5;
};

// FCrafterData
// Contains all data related to an item's data when crafted by a player
// If the item is NOT crafted, 'itemName' will be None.
USTRUCT(BlueprintType)
struct FStCrafterData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText itemName = FText::FromString("None");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText crafterName = FText::FromString("None");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FStCraftQuality> itemQuality;
};

USTRUCT(BlueprintType)
struct FStCraftRecycleData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName itemName = "None";

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int giveQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool spawnsInWorld = false;
};

USTRUCT(BlueprintType)
struct FStCraftingRecipe : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName itemName = "None";

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int quantityRequired = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<ECraftingQuality> affectedQualities;
	
};

USTRUCT(BlueprintType)
struct FStCraftingData
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText displayName = FText::FromString("Untitled");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int createsQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float failureChance = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int waitTime = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECraftingSkill craftingSkill = ECraftingSkill::GENERAL;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite)
	//	TArray<APrimaryWorkstation> allowedWorkstations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FStCraftingRecipe> craftingRecipe;

};


UCLASS()
class UCraftSystem : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable)
	static bool isSameCrafter(FStCrafterData craftOne, FStCrafterData craftTwo);
	
};