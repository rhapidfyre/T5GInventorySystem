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
#include "NativeGameplayTags.h"
#include "Engine/DataTable.h"
#include "Delegates/Delegate.h"

#include "ItemData.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogInventory, Log, Error);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Equipment);	// Used as equipment
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_QuestItem);	// Used in a quest
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Drinkable);	// Can be drank
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Edible);		// Can be eaten
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_MeleeWeapon);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_RangedWeapon);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Placeable);	// Can be placed like furniture
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Currency);		// Used as a type of currency
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Fuel);			// Used as fuel for a forge or fire
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Utility);		// An item that provide a function when in possession
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Ingredient);	// Used as an ingredient to a crafting recipe
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Component);	// Used as part of a final product, like a battery

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Common);		// Base chance of ~65%
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Uncommon);	// Base chance of ~20%
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Rare);		// Base chance of ~11%
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Masterwork); // Base chance of ~6%
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Legendary);	// Base chance of ~1%
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Divine)		// Base chance of ~0.18%

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Trigger); // Default behavior
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Equip);   // Activating equips it
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Drink);   // Activating drinks it
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Eat);     // Activating eats it
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Emplace); // Starts the placement system


// Obsolete?
UCLASS(BlueprintType)
class T5GINVENTORYSYSTEM_API UItemSystem : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	
};


/**
 * Defines variables that all items share, as well as accessors/mutators
 */
UCLASS(BlueprintType)
class T5GINVENTORYSYSTEM_API UPrimaryItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:

	UPrimaryItemDataAsset()
		: ItemRarities(TAG_Item_Rarity_Common),
		  ItemActivation(TAG_Item_Activation_Trigger)
		{};

	UFUNCTION(BlueprintPure) UPrimaryItemDataAsset* CopyAsset() const;
	
	UFUNCTION(BlueprintPure) int			GetItemPrice() const;
	UFUNCTION(BlueprintPure) int			GetItemMaxStackSize() const;
	UFUNCTION(BlueprintPure) float			GetItemCarryWeight() const;
	UFUNCTION(BlueprintPure) float			GetItemMaxDurability() const;
	UFUNCTION(BlueprintPure) FText			GetItemDisplayName() const;
	UFUNCTION(BlueprintPure) FString		GetItemDisplayNameAsString() const;
	UFUNCTION(BlueprintPure) FString		GetItemDescription() const;
	UFUNCTION(BlueprintPure) FGameplayTag	GetItemRarity() const;
	UFUNCTION(BlueprintPure) UTexture2D		GetItemThumbnail() const;
	UFUNCTION(BlueprintPure) UStaticMesh*	GetItemStaticMesh() const;
	UFUNCTION(BlueprintPure) FTransform		GetItemOriginAdjustments() const;
	
	UFUNCTION(BlueprintPure) bool	GetIsItemFragile() const;
	UFUNCTION(BlueprintPure) bool	GetIsItemDroppable() const;
	UFUNCTION(BlueprintPure) bool	GetIsItemConsumedOnUse() const;
	UFUNCTION(BlueprintPure) bool	GetCanEquipInSlot(const FGameplayTag& EquipmentSlotTag) const;
	UFUNCTION(BlueprintPure) bool	GetItemHasCategory(const FGameplayTag& ChallengeTag) const;
	UFUNCTION(BlueprintPure) bool	GetCanItemActivate() const;
	
	UFUNCTION(BlueprintPure) FGameplayTagContainer GetItemCategories() const;
	UFUNCTION(BlueprintPure) FGameplayTagContainer GetItemEquippableSlots() const;

protected:
	
	// Name displayed for this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName = FText();

	// UI Description of this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ItemDescription = "";
	
	// Rarities that this item can appear as. Defaults to Common only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FGameplayTagContainer ItemRarities;

	// Categories the item belongs to
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FGameplayTagContainer ItemCategories;

	// How the item activates
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FGameplayTag ItemActivation;

	// If item can be equipped, this is a list of all eligible slots this item fits in
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FGameplayTagContainer EquippableSlots;

	// <= 1 means it does not stack
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	int MaxStackSize = 1;

	// How much each individual item weighs
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	float CarryWeight = 0.01f;

	// The visual used for UI presentation
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	UTexture2D* ItemThumbnail = nullptr;

	// The appearance of the item when spawned
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	UStaticMesh* Mesh = nullptr;

	// XYZ location adjustment from attach point
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FVector OriginAdjustment;

	// XYZ rotation adjustment from attach point
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FRotator OriginRotate;

	// XYZ scale factor from the parent mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FVector OriginScale;

	// Any durability of < 0.0 means the item is indestructible (0.0 means broken)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	float MaxDurability = -1.0f;

	// Base value of the item without modification or scaling
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	int ItemPrice = 1;
	
	// Fragile items will be destroyed if dropped out of the inventory
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFragile = false;
	
	// If true, the item can not be willfully dropped by the player.
	// It must exist in an inventory, or destroyed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bNoDrop = false;

	// If the item should subtract 1 quantity when consumed/activated
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bConsumeOnUse = false;
	
};


/**
 * Data belonging to a specific item, such as a carrot or a sword.
 */
UCLASS(BlueprintType)
class T5GINVENTORYSYSTEM_API UItemDataAsset : public UPrimaryItemDataAsset
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite) int Quantity = 1; 
};


/**
 * Data belonging specifically to items used as starting items. Starting Items
 * are items that are spawned on NPCs as loot, vendor items for merchants,
 * or starting items for players.
 */
UCLASS(BlueprintType)
class T5GINVENTORYSYSTEM_API UStartingItemData : public UPrimaryItemDataAsset
{
	GENERATED_BODY()

public:

	// The minimum chance, in percentage (0.0 - 1.0) this item will be chosen
	// 1.0 will result in always being chosen
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float	ChanceMinimum = 1.f;

	// The maximum chance, in percentage (0.0 - 1.0) this item will be chosen
	// Ignored if ChanceMinimum is >= 1.0
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float	ChanceMaximum = 1.f;
	

	// If true, a random quantity of the item will be given. If false, the
	// QuantityMaximum will be used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	bool    bRandomQuantity = false;

	// If true, the item will spawn in the equipment slots, if available
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	bool    bEquipOnStart = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	int		QuantityMinimum = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	int		QuantityMaximum = 1;
	
};


/** Used for tracking items in an inventory slot */
USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStItemData
{
	GENERATED_BODY()

	TMulticastDelegate<void()>	OnItemUpdated;
	TMulticastDelegate<void()>	OnItemActivation;
	
	TMulticastDelegate<void(const float, const float)>	OnItemDurabilityChanged;
	
	// Initializes a generic item
	FStItemData();
	FStItemData(const UPrimaryItemDataAsset* NewData);
	FStItemData(const FStItemData& OldItem);

	~FStItemData();

	float GetModifiedItemValue() const;
	float GetBaseItemValue() const;
	float DamageItem(const float DeductionValue);
	float RepairItem(const float AdditionValue);

	void DestroyItem();
	void ReplaceItem(const FStItemData& NewItem);
	
	bool ActivateItem() const;
	bool GetIsValidItem() const { return IsValid(Data); }
	bool GetIsSameItem(const FStItemData& ItemStruct) const;
	bool GetIsExactSameItem(const FStItemData& ItemStruct) const;
	bool GetIsItemDamaged() const;
	bool GetIsItemDroppable() const;
	bool GetIsItemFragile() const;

	bool GetIsItemUnbreakable() const;
	bool GetIsItemBroken() const { return DurabilityNow == 0.f; }

	// Current Durability (Data-Durability is the max durability of this item)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) float DurabilityNow;

	// Actual Item Rarity (Data->Rarity is for generating this item)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FGameplayTag Rarity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UPrimaryItemDataAsset* Data; // References the data asset with static info
	
};