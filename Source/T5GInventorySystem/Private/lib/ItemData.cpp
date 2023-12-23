
#include "lib/ItemData.h"

#include "lib/InventorySlot.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Equipment, 	"Item.Category.Equipment");
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_QuestItem, 	"Item.Category.QuestItem");
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Drinkable, 	"Item.Category.Drinkable");
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Edible, 		"Item.Category.Edible");
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_MeleeWeapon, 	"Item.Category.MeleeWeapon");
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_RangedWeapon, 	"Item.Category.RangedWeapon");
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Placeable, 	"Item.Category.Placeable");
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Currency, 		"Item.Category.Currency");
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Fuel, 			"Item.Category.Fuel");
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Utility, 		"Item.Category.Utility");
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Ingredient, 	"Item.Category.Ingredient");
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Category_Component, 	"Item.Category.Component");

UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Common,		"Item.Rarity.Common");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Uncommon,	"Item.Rarity.Uncommon");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Rare,		"Item.Rarity.Rare");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Masterwork,	"Item.Rarity.Masterwork");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Legendary,	"Item.Rarity.Legendary");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Divine,		"Item.Rarity.Divine");

UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Trigger,	"Item.Rarity.Trigger");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Equip,	"Item.Rarity.Equip");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Drink,	"Item.Rarity.Drink");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Eat,		"Item.Rarity.Eat");


UPrimaryItemDataAsset* UPrimaryItemDataAsset::CopyAsset() const
{
	return DuplicateObject(this, GetOuter());
}

int UPrimaryItemDataAsset::GetItemPrice() const
{
	return 1;
}


FStItemData::FStItemData() :
	DurabilityNow(-1.f), Rarity(TAG_Item_Rarity_Common.GetTag()), Data(nullptr)
{}

/**
 * Creates a NEW item with the data found at the given Data Asset
 * @param NewData			The item that is being initialized 
 */
FStItemData::FStItemData(const UPrimaryItemDataAsset* NewData) :
	DurabilityNow(NewData->GetItemMaxDurability()),
	Rarity(NewData->GetItemRarity()),
	Data(NewData->CopyAsset())
{}

/**
 * Creates a copy of the item passed into the constructor
 * @param OldItem			The old item, for reference 
 */
FStItemData::FStItemData(const FStItemData& OldItem) :
	DurabilityNow(OldItem.DurabilityNow), Rarity(OldItem.Rarity), Data(OldItem.Data->CopyAsset())
{}

FStItemData::~FStItemData()
{
	if (OnItemUpdated.IsBound()) { OnItemUpdated.Clear(); }
	if (OnItemActivation.IsBound()) { OnItemActivation.Clear(); }
	if (OnItemDurabilityChanged.IsBound()) { OnItemDurabilityChanged.Clear(); }
}

/**
 * Returns the unmodified base price for this item.
 * @return The base price for this item. Zero on failure.
 */
float FStItemData::GetBaseItemValue() const
{
	if (!GetIsValidItem()) { return 0.f; }
	return Data->GetItemPrice();
}

/**
 * Returns the value of the item, modifying it based on meta data
 * values such as the rarity and durability.
 * @return					The value of the item after modifiers
 */
float FStItemData::GetModifiedItemValue() const
{
	float TotalItemValue = GetBaseItemValue();
	float RarityModifier = 1.f;
	
	// Modify the value by rarity first
	switch(Rarity)
	{
	case TAG_Item_Rarity_Common.GetTag():		RarityModifier	= 0.85; break;
	case TAG_Item_Rarity_Uncommon.GetTag(): 	RarityModifier	= 1.08;	break;
	case TAG_Item_Rarity_Rare.GetTag(): 		RarityModifier	= 1.25; break;
	case TAG_Item_Rarity_Masterwork.GetTag(): 	RarityModifier	= 1.73; break;
	case TAG_Item_Rarity_Legendary.GetTag(): 	RarityModifier	= 2.21; break;
	case TAG_Item_Rarity_Divine.GetTag(): 		RarityModifier	= 4.19; break;
	default: break;
	}
	TotalItemValue *= RarityModifier;
	
	// Apply damage modifier before returning the final price
	if (!GetIsItemUnbreakable())
	{
		// Modifies the price of the item when damaged where
		//   0% durability is 10% value and 100% durability is 100% value
		//											 y = 1.05 - e^(-3x)
		const float DamageModifier = (1.05) * pow(EULERS_NUMBER, (-3)*DurabilityNow);
		TotalItemValue *= FMath::Clamp(DamageModifier, 0.f, 1.f);
	}
	
	return TotalItemValue;
}

/**
 * Damages the item in this slot and ensures the value remains clamped.
 * Issues a broadcast with OnItemDurabilityChanged if the value changed.
 * @param DeductionValue	The amount of damage to apply
 * @return					The new durability. Negative on failure.
 */
float FStItemData::DamageItem(const float DeductionValue)
{
	if (!GetIsValidItem()) { return -1.f; }
	const float DamageValue = abs(DeductionValue);
	if (DamageValue > 0.f && DurabilityNow > 0.f)
	{
		const float OldDurability = DurabilityNow;
		const float NewDurability = DurabilityNow - DamageValue;
		DurabilityNow = NewDurability >= 0.f ? NewDurability : 0.f;
		if (OnItemDurabilityChanged.IsBound()) {OnItemDurabilityChanged.Broadcast(OldDurability, DurabilityNow);}
	}
	return DurabilityNow;
}

/**
 * Repairs the item in this slot and ensures the value remains clamped.
 * Issues a broadcast with OnItemDurabilityChanged if the value changed.
 * @param AdditionValue		The amount of damage to repair
 * @return					The new durability. Negative on failure.
 */
float FStItemData::RepairItem(const float AdditionValue)
{
	if (!GetIsValidItem()) { return -1.f; }
	const float RepairValue = abs(AdditionValue);
	if (RepairValue > 0.f && DurabilityNow < Data->GetItemMaxDurability())
	{
		const float OldDurability = DurabilityNow;
		const float NewDurability = DurabilityNow + RepairValue;
		const float MaxDurability = Data->GetItemMaxDurability();
		DurabilityNow = NewDurability > MaxDurability ? MaxDurability : NewDurability;
		if (OnItemDurabilityChanged.IsBound()) {OnItemDurabilityChanged.Broadcast(OldDurability, DurabilityNow);}
	}
	return DurabilityNow;
}

//Resets the item to a default, invalid item struct.
void FStItemData::DestroyItem()
{
	*this = FStItemData();
}

// Replaces this struct with the incoming struct
void FStItemData::ReplaceItem(const FStItemData& NewItem)
{
	*this = FStItemData(NewItem);
}

/**
 * Activates an item, AKA issues broadcast with OnItemActivation.
 * @return				True if item exists and was activated. False otherwise.
 */
bool FStItemData::ActivateItem() const
{
	if (GetIsValidItem())
	{
		if (Data->GetCanItemActivate())
		{
			OnItemActivation.Broadcast();
			return true;
		}
	}
	return false;
}

/**
 * Checks if the reference item and this item have the same data asset.
 * Does not account for meta data, such as durability. Use IsExactSameItem().
 * @param ItemStruct	The item referenced that we are comparing this item to
 * @return				True on Data Asset match.
 */
bool FStItemData::GetIsSameItem(const FStItemData& ItemStruct) const
{
	// If either item has an invalid Data Asset, it is not a real item.
	if (!IsValid(ItemStruct.Data) || !GetIsValidItem()) {return false;}
	 return ItemStruct.Data == Data; // If the Data Assets match, we're good to go.
}

/**
 * Ensures the reference item and this item have the same meta data, such as
 * durability and rarity. Otherwise, they are different items.
 * @param ItemStruct	The item referenced that we are comparing this item to.
 * @return				True on exact match
 */
bool FStItemData::GetIsExactSameItem(const FStItemData& ItemStruct) const
{
	if (GetIsSameItem(ItemStruct))
	{
		if (ItemStruct.DurabilityNow != DurabilityNow)	{return false;}
		if (ItemStruct.Rarity        != Rarity)			{return false;}
		return ItemStruct.Data == Data; // Referencing the same Data Asset
	}
	return false;
}

bool FStItemData::GetIsItemDamaged() const
{
	if (GetIsValidItem())
	{
		if (DurabilityNow < 0.f) {return false;}
		return Data->GetItemMaxDurability() > DurabilityNow;
	}
	return false;
}

bool FStItemData::GetIsItemDroppable() const
{
	if (GetIsValidItem()) {return Data->GetIsItemDroppable();}
	return false;
}

bool FStItemData::GetIsItemFragile() const
{
	if (GetIsValidItem()) {return Data->GetIsItemFragile();}
	return false;
}

bool FStItemData::GetIsItemUnbreakable() const
{
	if (GetIsValidItem()) {return Data->GetItemMaxDurability() < 0.f;}
	return false;
}
