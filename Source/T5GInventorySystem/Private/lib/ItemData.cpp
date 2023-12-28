
#include "lib/ItemData.h"

#include "lib/InventorySlot.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Equipment, 	"Item.Category.Equipment");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_QuestItem, 	"Item.Category.QuestItem");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Drinkable, 	"Item.Category.Drinkable");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Edible, 		"Item.Category.Edible");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_MeleeWeapon, 	"Item.Category.MeleeWeapon");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_RangedWeapon, 	"Item.Category.RangedWeapon");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Placeable, 	"Item.Category.Placeable");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Currency, 		"Item.Category.Currency");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Fuel, 			"Item.Category.Fuel");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Utility, 		"Item.Category.Utility");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Ingredient, 	"Item.Category.Ingredient");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Category_Component, 	"Item.Category.Component");

UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Trash,		"Item.Rarity.Trash");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Common,		"Item.Rarity.Common");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Uncommon,	"Item.Rarity.Uncommon");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Rare,		"Item.Rarity.Rare");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Legendary,	"Item.Rarity.Legendary");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Rarity_Divine,		"Item.Rarity.Divine");

UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Trigger,	"Item.Rarity.Trigger");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Equip,	"Item.Rarity.Equip");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Drink,	"Item.Rarity.Drink");
UE_DEFINE_GAMEPLAY_TAG(TAG_Item_Activation_Eat,		"Item.Rarity.Eat");


UItemDataAsset* UItemDataAsset::CopyAsset() const
{
	return DuplicateObject(this, GetOuter());
}

int UPrimaryItemDataAsset::GetItemPrice() const
{
	return 1;
}

int UPrimaryItemDataAsset::GetItemMaxStackSize() const
{
	return MaxStackSize > 0 ? MaxStackSize : 1;
}

float UPrimaryItemDataAsset::GetItemCarryWeight() const
{
	return CarryWeight >= 0.f ? CarryWeight : 0.0;
}

float UPrimaryItemDataAsset::GetItemMaxDurability() const
{
	return MaxDurability;
}

FText UPrimaryItemDataAsset::GetItemDisplayName() const
{
	return DisplayName;
}

FString UPrimaryItemDataAsset::GetItemDisplayNameAsString() const
{
	return (DisplayName).ToString();
}

FString UPrimaryItemDataAsset::GetItemDescription() const
{
	return ItemDescription;
}

FGameplayTag UPrimaryItemDataAsset::GetItemRarity() const
{
	if (ItemRarity.IsValid()) { return ItemRarity; }
	return TAG_Item_Rarity_Trash.GetTag();
}

UTexture2D* UPrimaryItemDataAsset::GetItemThumbnail() const
{
	return ItemThumbnail;
}

UStaticMesh* UPrimaryItemDataAsset::GetItemStaticMesh() const
{
	return Mesh;
}

FTransform UPrimaryItemDataAsset::GetItemOriginAdjustments() const
{
	FTransform NewTransform(OriginAdjustment);
	NewTransform.SetRotation(OriginRotate.Quaternion());
	NewTransform.SetScale3D(OriginScale);
	return NewTransform;
}

bool UPrimaryItemDataAsset::GetIsItemFragile() const
{
	return bFragile;
}

bool UPrimaryItemDataAsset::GetIsItemDroppable() const
{
	return bNoDrop;
}

bool UPrimaryItemDataAsset::GetIsItemConsumedOnUse() const
{
	return bConsumeOnUse;
}

bool UPrimaryItemDataAsset::GetCanEquipInSlot(const FGameplayTag& EquipmentSlotTag) const
{
	return EquippableSlots.HasTag(EquipmentSlotTag);
}

bool UPrimaryItemDataAsset::GetItemHasCategory(const FGameplayTag& ChallengeTag) const
{
	return ItemCategories.HasTag(ChallengeTag);
}

bool UPrimaryItemDataAsset::GetCanItemActivate() const
{
	if (ItemActivation.IsValid()) { return true; }
	return false;
}

FGameplayTagContainer UPrimaryItemDataAsset::GetItemCategories() const
{
	return ItemCategories;
}

FGameplayTagContainer UPrimaryItemDataAsset::GetItemEquippableSlots() const
{
	return EquippableSlots;
}


FStItemData::FStItemData() :
	ItemQuantity(0),
	DurabilityNow(-1.f),
	Rarity(TAG_Item_Rarity_Common.GetTag()),
	Data(nullptr)
{}

/**
 * Creates a NEW item with the data found at the given Data Asset
 * @param NewData			The item that is being initialized
 * @param OrderQuantity		The quantity requested to start with
 */
FStItemData::FStItemData(const UItemDataAsset* NewData, const int OrderQuantity)
{
	Data = NewData->CopyAsset();
	if (IsValid(Data))
	{
		const int NewQuantity = OrderQuantity > 0 ? OrderQuantity : 1;
		const int MaxStackSize = Data->GetItemMaxStackSize();
		
		ItemQuantity	= NewQuantity > MaxStackSize ? MaxStackSize : NewQuantity;
		DurabilityNow	= Data->GetItemMaxDurability();
		Rarity			= Data->GetItemRarity();
	}
}

/**
 * Creates a copy of the item passed into the constructor
 * @param OldItem			The old item, for reference
 * @param OverrideQuantity		An optional quantity override.
 *							If <= 0, the OldItem quantity will be used
 */
FStItemData::FStItemData(const FStItemData& OldItem, int OverrideQuantity)
{
	if (IsValid(OldItem.Data))
	{
		if (OverrideQuantity > 0)	{ItemQuantity = OverrideQuantity;}
		else						{ItemQuantity = OldItem.ItemQuantity;}
		DurabilityNow	= OldItem.Data->GetItemMaxDurability();
		Rarity			= OldItem.Data->GetItemRarity();
		Data			= OldItem.Data->CopyAsset();
	}
}

FStItemData::~FStItemData()
{
	if (OnItemUpdated.IsBound()) { OnItemUpdated.Clear(); }
	if (OnItemActivation.IsBound()) { OnItemActivation.Clear(); }
	if (OnItemDurabilityChanged.IsBound()) { OnItemDurabilityChanged.Clear(); }
}

FString FStItemData::ToString() const
{
	if (GetIsValidItem()) { return Data->GetItemDisplayName().ToString(); }
	return "Invalid Item";
}

float FStItemData::GetMaxDurability() const
{
	if (GetIsValidItem()) { return -1.f; }
	return Data->GetItemMaxDurability();
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
	if		(Rarity == TAG_Item_Rarity_Trash)		{ RarityModifier = 0.40; }
	else if (Rarity == TAG_Item_Rarity_Common)		{ RarityModifier = 0.85; }
	else if (Rarity == TAG_Item_Rarity_Uncommon)	{ RarityModifier = 1.08; }
	else if (Rarity == TAG_Item_Rarity_Rare)		{ RarityModifier = 1.25; }
	else if (Rarity == TAG_Item_Rarity_Legendary)	{ RarityModifier = 2.21; }
	else if (Rarity == TAG_Item_Rarity_Divine)		{ RarityModifier = 4.19; }
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
 * Increases the quantity of this item, performing logic internally.
 * @param OrderQuantity The amount to increase by
 * @return The number of items actually added
 */
int FStItemData::IncreaseQuantity(const int OrderQuantity)
{
	if (!GetIsValidItem() || OrderQuantity == 0) { return 0; }
	const int MaxStackSize = Data->GetItemMaxStackSize();
	if (GetIsFull()) { return 0; }
	const int StartQuantity = ItemQuantity;
	const int NewQuantity   = ItemQuantity + abs(StartQuantity);
	ItemQuantity = NewQuantity > MaxStackSize ? MaxStackSize : NewQuantity;
	return ItemQuantity - StartQuantity;
}

/**
 * Decreases the quantity of this item, performing logic internally. If the
 * quantity falls below 1, the slot will clear itself and reset.
 * @param OrderQuantity The amount to decrease by
 * @return The number of items actually removed.
 */
int FStItemData::DecreaseQuantity(const int OrderQuantity)
{
	if (!GetIsValidItem() || OrderQuantity == 0) { return 0; }
	const int StartQuantity  = ItemQuantity;
	const int NewQuantity    = ItemQuantity - abs(OrderQuantity);
	ItemQuantity = NewQuantity > 0 ? NewQuantity : 0;
	if (GetIsEmpty()) { DestroyItem(); }
	return StartQuantity - ItemQuantity;
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

/**
 * Damages the item in this slot and ensures the value remains clamped.
 * Issues a broadcast with 'OnItemDurabilityChanged' if the value changed.
 * Inventory Managers must handle the outcome if durability reaches zero.
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

int FStItemData::GetMaxStackSize() const
{
	if (GetIsValidItem()) { return Data->GetItemMaxStackSize(); }
	return 0;
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

float FStItemData::GetWeight() const
{
	if (!GetIsValidItem()) { return 0.f; }
	return Data->GetItemCarryWeight() * ItemQuantity;
}

bool FStItemData::GetCanActivate() const
{
	if (GetIsValidItem()) { return Data->GetCanItemActivate();}
	return false;
}

/**
 * Sets the new quantity, clamping between zero and MaxStackSize.
 * @param NewQuantity The new quantity to be set.
 * @return True if the setter worked.
 */
bool FStItemData::SetItemQuantity(int NewQuantity)
{
	if (!GetIsValidItem())
	{
		const int FinalQuantity = FMath::Clamp(NewQuantity, 0, GetMaxStackSize());
		if (FinalQuantity != ItemQuantity)
		{
			const int OldQuantity = ItemQuantity;
			ItemQuantity = FinalQuantity;
			OnItemQuantityChanged.Broadcast(OldQuantity, FinalQuantity);
		}
	}
	return false;
}

bool FStItemData::GetIsEmpty() const
{
	return (ItemQuantity < 1);
}

bool FStItemData::GetIsFull() const
{
	if (!GetIsValidItem()) { return true; }
	return (ItemQuantity > Data->GetItemMaxStackSize());
}

/**
 * Activates an item, AKA issues broadcast with OnItemActivation.
 * @return				True if item exists and was activated. False otherwise.
 */
bool FStItemData::ActivateItem(bool bForceConsume)
{
	if (GetIsValidItem())
	{
		if (Data->GetCanItemActivate())
		{
			if (bForceConsume)
			{
				DecreaseQuantity(1);
			}
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

FGameplayTagContainer FStItemData::GetValidEquipmentSlots() const
{
	if (GetIsValidItem()) {return Data->GetItemEquippableSlots();}
	return FGameplayTagContainer();
}

bool FStItemData::GetIsValidFitToSlot(const FGameplayTag& SlotTag) const
{
	if (GetIsValidItem())
	{
		// Is an equipment tag
		if (SlotTag.RequestDirectParent() == TAG_Equipment_Slot.GetTag())
		{
			const FGameplayTagContainer EquipSlotTags = GetValidEquipmentSlots();
			return EquipSlotTags.HasTag(SlotTag);
		}
		
		// If we match any of the valid tags, it's a valid non-equipment slot
		FGameplayTagContainer ValidSlotTags;
		ValidSlotTags.AddTag( TAG_Inventory_Slot_Equipment.GetTag() );
		ValidSlotTags.AddTag( TAG_Inventory_Slot_Generic.GetTag() );
		return !SlotTag.MatchesAny(ValidSlotTags);
	}
	return false;
}
