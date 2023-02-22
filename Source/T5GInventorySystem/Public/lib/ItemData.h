/*
*	Item Data Function Library
*
*	Contains Structs, Enums, and public Classes regarding item data;
*
*	CHANGELOG
*		version 1.0		Melanie Harris		30NOV2022		Created file and contents
*/
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EquipmentData.h"

#include "ItemData.generated.h"

UENUM(BlueprintType)
enum class EItemActivation : uint8
{
	NONE UMETA(DisplayName = "Does Not Activate"),
	EQUIP UMETA(DisplayName = "Equippable", Description = "Equips when Activated"),
	EAT UMETA(DisplayName = "Edible", Description = "Item is eaten or otherwise consumed"),
	DRINK UMETA(DisplayName = "Drinkable", Description = "Item is consumed by drinking"),
	PLACE UMETA(DisplayName = "Placeable", Description = "Item activated the placement system when activated"),
	MISC UMETA(DisplayName = "Activated", Description = "An item that performs an action when activated")
};

UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	NONE,	CRAFT,	QUEST,	FOOD,	DRINK,	WEAPON,	EQUIPMENT,
	PLACEABLE,	CURRENCY,	COMPONENT,	FUEL,	UTILITY
};

UENUM(BlueprintType)
enum class EItemSubcategory : uint8
{
	NONE, RENEWABLE, BATTERY, COOLING
};

UENUM(BlueprintType)
enum class EItemRarity : uint8
{
	COMMON, UNCOMMON, MASTERWORK, RARE, LEGENDARY, MYTHICAL, RELIC, DEV
};


USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStItemData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName properName = "None";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText displayName = FText::FromString("None");

	UPROPERTY(EditAnywhere, BlueprintReadWrite);
	FString itemDetails = "Invalid Template Item";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EItemRarity itemRareness = EItemRarity::COMMON;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EItemCategory itemCategory = EItemCategory::NONE;

	// Optional subcategory
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EItemCategory itemSubcategory = EItemCategory::NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EItemActivation itemActivation = EItemActivation::NONE;

	// If item can be equipped, this is a list of all eligible slots this item fits in
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EEquipmentSlotType> equipSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int maxStackSize = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float itemWeight = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* itemThumbnail = nullptr; //UTexture2D::CreateTransient(32,32);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* staticMesh = nullptr; //NewObject<UStaticMesh>();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USkeletalMesh* skeletalMesh = nullptr; //NewObject<USkeletalMesh>();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform originAdjust;

	// A durability of 0.0 means the item is indestructible
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float currentDurability = -1.0f;

	// Value of the item, in base credits
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int basePrice = 1;
};

UCLASS()
class T5GINVENTORYSYSTEM_API UItemSystem : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	// Gets the current default name for an invalid item.
	// This should be used instead of accessing the name directly to avoid deprecation issues
    UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
        static FName getInvalidName() { return "None"; } 

	// Alternative accessor to getInvalidName().
    UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static FText getInvalidText() { return FText::FromName("None");}

	// Preferred accessor, in case the variable name is ever changed.
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static FName getItemName(FStItemData itemData) { return itemData.properName; };
	
	// Checks if the given item data is a stackable item
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static bool isItemStackable(FStItemData itemData);

	// Checks if the item name given is stackable. This uses the data table lookup.
	// To check an existing item data struct with O(1) lookup, use 'isItemStackable'
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static bool isStackable(FName itemName);

	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static float getItemWeight(FStItemData itemData) { return itemData.itemWeight; };

	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static float getWeight(FName itemName);

	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static TArray<EEquipmentSlotType> getItemEquipSlots(FStItemData itemData) { return itemData.equipSlots; };

	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static TArray<EEquipmentSlotType> getEquipmentSlots(FName itemName);
	
	// Checks if the item data is an activated item
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static bool isItemActivated(FStItemData itemData);

	// Checks if the item name given is activated. This uses the data table lookup.
	// To check an existing item data struct with O(1) lookup, use 'isItemActivated'
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static bool isActivated(FName itemName);

	// Gets the max durability of the given item. Negative means indestructible.
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static float getItemDurability(FStItemData itemData);

	// Gets the max durability for the item given. Uses data table lookup.
	// To check an existing item data struct with O(1) lookup, use 'getItemMaxDurability'
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static float getDurability(FName itemName);
	
	// Returns a pointer to the item data table for manual operations
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static UDataTable* getItemDataTable();

	// Gets the item data structure from the given item name ('food_carrot')
	// Performs an O(1) lookup, but retrieves data, which is slower than direct access.
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static FStItemData getItemDataFromItemName(FName itemName);

	/** Checks if the given name is valid. If valid, retrieves the item data from the data table
	 *
	 * @param itemName An FName text that represents the row name we're looking for (row name = item name)
	 * @param performLookup If true, will also check the item is in the data table.
	 * @return True if the item is valid, false if it is not.
	 */
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static bool getItemNameIsValid(FName itemName, bool performLookup = false);

	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static bool getItemDataIsValid(FStItemData itemData);
	
	// Takes an existing itemData and returns its stack size.
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static int getItemMaxStackSize(FStItemData itemData) { return itemData.maxStackSize; };
	
	/** Gets the maximum value of identical items that can stack together.
	 * @param itemName An FName with the text of the item (row name) to look for
	 * @return An integer of the maximum stack size. Return guarantees a value > zero.
	 */
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static int getMaximumStackSize(FName itemName);

	/** Checks if two item names are the same
	 * @return A boolean where TRUE means the two items are the same, and FALSE means different or invalid item
	 */
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static bool isSameItemName(FName itemOne, FName itemTwo);

	// Returns true if both item structs are identical and valid
	UFUNCTION(BlueprintPure, Category = "Item Data System Globals")
		static bool isSameItemData(FStItemData itemOne, FStItemData itemTwo);

};