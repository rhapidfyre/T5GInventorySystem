
#include "lib/ItemData.h"

#include "lib/InventorySlot.h"


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

bool UPrimaryItemDataAsset::GetItemCanStack() const
{
	return GetItemMaxStackSize() > 1;
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

/* Returns a random rarity from the items possible rarity categories.
 * If the item has no rarities defined, it will return "Common"
 */
FGameplayTag UPrimaryItemDataAsset::GetItemRarity() const
{
	if (ItemRarity.IsValid())
	{
		TArray AllRarityTags = ItemRarity.GetGameplayTagArray();
		if (AllRarityTags.Num() > 0)
		{
			return AllRarityTags[ FMath::RandRange(0, ItemRarity.Num()-1) ];
		}
		return TAG_Item_Rarity_Common;
	}
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

bool UPrimaryItemDataAsset::GetItemHasCategory(const FGameplayTag& ChallengeTag) const
{
	return ItemCategories.HasTag(ChallengeTag);
}

bool UItemDataAsset::GetItemCanActivate() const
{
	FGameplayTagContainer TagOptions;
	GetItemTagOptions(TagOptions);
	return TagOptions.HasTag(TAG_Item_Activates);
}

/**
 * Returns a container of all tags applied to each child class & the parent
 * @param TagOptions All of the tags applied to this item
 */
void UItemDataAsset::GetItemTagOptions(FGameplayTagContainer& TagOptions) const
{
	TagOptions.AddTag(ItemActivation);
}

FGameplayTagContainer UPrimaryItemDataAsset::GetItemCategories() const
{
	return ItemCategories;
}

/* Returns an array of all possible rarities. If no rarities are specified,
 * this will return an array with just the "Common" rarity.
 */
FGameplayTagContainer UPrimaryItemDataAsset::GetItemRarities() const
{
	return ItemRarity.Num() > 0 ? ItemRarity : FGameplayTagContainer(TAG_Item_Rarity_Common);
}


FStItemData::FStItemData() :
	ItemQuantity(0),
	DurabilityNow(-1.f),
	Rarity(TAG_Item_Rarity_Common.GetTag())
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
	if (OldItem.GetIsValidItem())
	{
		if (IsValid(OldItem.Data))
		{
			if (OverrideQuantity > 0)	{ItemQuantity = OverrideQuantity;}
			else						{ItemQuantity = OldItem.ItemQuantity;}
			DurabilityNow	= OldItem.Data->GetItemMaxDurability();
			Rarity			= OldItem.Data->GetItemRarity();
			Data			= OldItem.Data->CopyAsset();
			PrimaryAssetId = Data->GetPrimaryAssetId();
			return;
		}
	}
}

FStItemData::~FStItemData()
{
	if (OnItemUpdated.IsBound()) { OnItemUpdated.Clear(); }
	if (OnItemActivation.IsBound()) { OnItemActivation.Clear(); }
	if (OnItemDurabilityChanged.IsBound()) { OnItemDurabilityChanged.Clear(); }
}

FGameplayTagContainer FStItemData::GetValidEquipmentSlots() const
{
	if (GetIsValidItem())
	{
		// Is an equipment tag
		if ( GetIsValidEquipmentItem() )
		{
			FGameplayTagContainer TagOptions;
			Data->GetItemTagOptions(TagOptions);
			return TagOptions;
		}
	}
	return {};
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
	// Clear all bindings
	if (OnItemUpdated.IsBound())    		{ OnItemUpdated.Clear(); }
	if (OnItemActivation.IsBound()) 		{ OnItemActivation.Clear(); }
	if (OnItemQuantityChanged.IsBound()) 	{ OnItemQuantityChanged.Clear(); }
	if (OnItemDurabilityChanged.IsBound()) 	{ OnItemDurabilityChanged.Clear(); }
	*this = FStItemData();
}

// Replaces this struct with the incoming struct
void FStItemData::ReplaceItem(const FStItemData& NewItem)
{
	// Clear all bindings
	if (OnItemUpdated.IsBound())    		{ OnItemUpdated.Clear(); }
	if (OnItemActivation.IsBound()) 		{ OnItemActivation.Clear(); }
	if (OnItemQuantityChanged.IsBound()) 	{ OnItemQuantityChanged.Clear(); }
	if (OnItemDurabilityChanged.IsBound()) 	{ OnItemDurabilityChanged.Clear(); }
	*this = FStItemData(NewItem);
}

float FStItemData::GetWeight() const
{
	if (!GetIsValidItem()) { return 0.f; }
	return Data->GetItemCarryWeight() * ItemQuantity;
}

bool FStItemData::GetCanActivate() const
{
	if (GetIsValidItem()) { return Data->GetItemCanActivate();}
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
		if (Data->GetItemCanActivate())
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

bool FStItemData::GetIsValidItem() const
{
	if (IsValid(Data))
	{
		const int iQuantity = ItemQuantity;
		return iQuantity > 0;
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

bool FStItemData::GetIsValidEquipmentItem() const
{
	if (GetIsValidItem())
	{
		return IsValid(Cast<UEquipmentDataAsset>(Data));
	}
	return false;
}

bool FStItemData::GetIsValidFitToSlot(const FGameplayTag& SlotTag) const
{
	if (GetIsValidEquipmentItem())
	{
		const UEquipmentDataAsset* EquipmentData = Cast<UEquipmentDataAsset>( Data );
		if (IsValid(EquipmentData))
		{
			return EquipmentData->EquippableSlots.HasTag(SlotTag);
		}
	}
	return false;
}

const UEquipmentDataAsset* FStItemData::GetItemDataAsEquipment() const
{
	if (GetIsValidEquipmentItem())
	{
		return Cast<UEquipmentDataAsset>(Data);
	}
	return nullptr;
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
