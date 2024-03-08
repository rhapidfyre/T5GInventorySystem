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
#include "GameplayTags/Public/GameplayTags.h"

#include "ItemData.generated.h"

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

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Generic);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Equipment);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Edible);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Drink);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Focus);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Trash);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Common);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Uncommon);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Rare);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Legendary);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Rarity_Divine);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Trigger); // Default behavior
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Equip);   // Activating equips it
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Drink);   // Activating drinks it
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Eat);     // Activating eats it
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Activation_Emplace); // Starts the placement system


USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FCraftingRecipe
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UItemDataAsset* ResultingItem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ChanceSuccess = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int QuantityOnSuccess = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag MinimumRarity = TAG_Item_Rarity_Rare.GetTag();

	// How many seconds it takes to complete this item
	// If you want ingredients to be consumed on completion, this time should
	// be divisible by 'tickConsume' (i.e. 300 ticks, 30 tick consume)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int ticksToComplete = 1;

	// How many ticks between ingredient consumption.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int tickConsume = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<UItemDataAsset*, int> Ingredients = {};
	
};


/**
 * Defines variables that all items share, as well as accessors/mutators.
 */
UCLASS(BlueprintType)
class T5GINVENTORYSYSTEM_API UPrimaryItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:

	UPrimaryItemDataAsset()
		: ItemType(TAG_Item_Generic.GetTag()), ItemRarity(TAG_Item_Rarity_Trash) {};
	
	UFUNCTION(BlueprintPure) int			GetItemPrice() const;
	UFUNCTION(BlueprintPure) int			GetItemMaxStackSize() const;
	UFUNCTION(BlueprintPure) bool			GetItemCanStack() const;
	UFUNCTION(BlueprintPure) float			GetItemCarryWeight() const;
	UFUNCTION(BlueprintPure) float			GetItemMaxDurability() const;
	UFUNCTION(BlueprintPure) FText			GetItemDisplayName() const;
	UFUNCTION(BlueprintPure) FString		GetItemDisplayNameAsString() const;
	UFUNCTION(BlueprintPure) FString		GetItemDescription() const;
	UFUNCTION(BlueprintPure) FGameplayTag	GetItemRarity() const;
	UFUNCTION(BlueprintPure) UTexture2D*	GetItemThumbnail() const;
	UFUNCTION(BlueprintPure) UStaticMesh*	GetItemStaticMesh() const;
	UFUNCTION(BlueprintPure) FTransform		GetItemOriginAdjustments() const;
	
	
	UFUNCTION(BlueprintPure) bool	GetIsItemFragile() const;
	UFUNCTION(BlueprintPure) bool	GetIsItemDroppable() const;
	UFUNCTION(BlueprintPure) bool	GetIsItemConsumedOnUse() const;
	UFUNCTION(BlueprintPure) bool	GetItemHasCategory(const FGameplayTag& ChallengeTag) const;
	
	UFUNCTION(BlueprintPure) FGameplayTagContainer GetItemCategories() const;
	UFUNCTION(BlueprintPure) FGameplayTagContainer GetItemRarities() const;

protected:
	
	// Name displayed for this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName = FText();
	
	// Name displayed for this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag ItemType;

	// UI Description of this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ItemDescription = "";
	
	// Rarities that this item can appear as. Defaults to Common only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FGameplayTagContainer ItemRarity;

	// Categories the item belongs to
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FGameplayTagContainer ItemCategories;

	// <= 1 means it does not stack
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	int MaxStackSize = 1;

	// How much each individual item weighs
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	float CarryWeight = 0.01f;

	// The visual used for UI presentation
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	UTexture2D* ItemThumbnail = nullptr;

	// The appearance of the item when spawned in the world as a prop
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
 * Data belonging to a specific item, such as a carrot or a sword. These are
 * items that are manipulated inside of an inventory. They can be equipment,
 * weapons, edibles, drinks, spell focus, etc. Anything that goes into the
 * inventory is a child of UItemDataAsset.
 */
UCLASS(BlueprintType)
class T5GINVENTORYSYSTEM_API UItemDataAsset : public UPrimaryItemDataAsset
{
	GENERATED_BODY()
	
public:

	UItemDataAsset() : ItemActivation(TAG_Item_Activation_Trigger) {};
	
	UFUNCTION(BlueprintPure) UItemDataAsset* CopyAsset() const;
	
	UFUNCTION(BlueprintPure) bool	GetCanItemActivate() const;
	
	virtual void	GetItemTagOptions(FGameplayTagContainer& TagOptions) const;

	// How the item activates
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FGameplayTag ItemActivation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FCraftingRecipe CraftingRecipe;
	
};


/**
 * Data belonging specifically to items used as starting items. Starting Items
 * are items that are spawned on NPCs as loot, vendor items for merchants,
 * or starting items for players.
 */
USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStartingItem
{
	GENERATED_BODY()
	
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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	UItemDataAsset* ItemReference = nullptr;
	
};


/** Used for tracking items in an inventory slot */
USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStItemData
{
	GENERATED_BODY()

	TMulticastDelegate<void()>	OnItemUpdated;
	TMulticastDelegate<void()>	OnItemActivation;
	
	TMulticastDelegate<void(const float, const float)>	OnItemQuantityChanged;
	TMulticastDelegate<void(const float, const float)>	OnItemDurabilityChanged;
	
	// Initializes a generic item
	FStItemData();
	FStItemData(const UItemDataAsset* NewData, const int OrderQuantity = 1);
	FStItemData(const FStItemData& OldItem, int OverrideQuantity = 0);

	virtual ~FStItemData();
	
	bool GetIsSameItem(const FStItemData& ItemStruct) const;
	bool GetIsExactSameItem(const FStItemData& ItemStruct) const;
	
	bool GetIsValidEquipmentItem() const;
	bool GetIsValidFitToSlot(const FGameplayTag& SlotTag) const;
	
	FGameplayTagContainer GetValidEquipmentSlots() const;

	FString ToString() const;
	float 	GetMaxDurability() const;
	float 	GetModifiedItemValue() const;
	float 	GetBaseItemValue() const;
	
	float 	RepairItem(const float AdditionValue);
	float 	DamageItem(const float DeductionValue);

	int 	GetMaxStackSize() const;
	int 	IncreaseQuantity(const int OrderQuantity = 1);
	int 	DecreaseQuantity(const int OrderQuantity = 1);

	void 	DestroyItem();
	void 	ReplaceItem(const FStItemData& NewItem);

	float	GetWeight() const;

	bool 	GetCanActivate() const;
	bool 	SetItemQuantity(int NewQuantity);
	bool 	GetIsEmpty() const;
	bool 	GetIsFull() const;
	bool 	ActivateItem(bool bForceConsume);
	bool 	GetIsValidItem() const;
	bool 	GetIsItemDamaged() const;
	bool 	GetIsItemDroppable() const;
	bool 	GetIsItemFragile() const;
	bool 	GetIsItemUnbreakable() const;
	bool 	GetIsItemBroken() const { return DurabilityNow == 0.f; }
	
	bool	bIsEquipped = false;

	// Custom comparison operator, since qualities, quantities and durability may be different
	// but the item might be the same exact item
	bool operator== (const FStItemData& ComparisonItem) const
	{
		if (IsValid(this->Data) && IsValid(ComparisonItem.Data))
		{
			if (this->Data == ComparisonItem.Data)
			{
				if (this->Rarity == ComparisonItem.Rarity)
				{
					return (this->DurabilityNow == ComparisonItem.DurabilityNow);
				}
			}
		}
		return false;
	}

	// Custom comparison operator, since qualities, quantities and durability may be different
	// but the item might be the same exact item
	bool operator != (const FStItemData& ComparisonItem) const
	{
		if (!IsValid(this->Data) || !IsValid(ComparisonItem.Data)) { return true; }
		if (this->Data != ComparisonItem.Data) { return true; }
		return (this->Rarity != ComparisonItem.Rarity);
	}

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) int ItemQuantity;

	// Current Durability (Data-Durability is the max durability of this item)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) float DurabilityNow;

	// Actual Item Rarity (Data->Rarity is for generating this item)
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FGameplayTag Rarity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	const UItemDataAsset* Data = nullptr; // References the data asset with static info
	
};