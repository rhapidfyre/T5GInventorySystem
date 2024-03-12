
#include "lib/InventorySlot.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "lib/ItemData.h"
#include "Logging/StructuredLog.h"


UInventorySlot::UInventorySlot(const FGameplayTag& SlotTypeTag)
{
	if (   SlotTypeTag.MatchesTag(TAG_Equipment_Slot)
		|| SlotTypeTag.MatchesTag(TAG_Inventory_Slot) )
	{
		if (SlotTypeTag.MatchesTag(TAG_Equipment_Slot))
		{
			SlotTags.AddTag(TAG_Inventory_Slot_Equipment);
		}
		SlotTags.AddTag(SlotTypeTag);
	}
}

void UInventorySlot::OnRep_QuantityUpdated_Implementation(int OldQuantity)
{
	if (OnSlotUpdated.IsBound())
	{
		OnSlotUpdated.Broadcast(SlotNumber_);
	}
}

void UInventorySlot::OnRep_ItemUpdated_Implementation(const FItemStatics& ItemPreChanges)
{
	if (OnSlotUpdated.IsBound())
	{
		OnSlotUpdated.Broadcast(SlotNumber_);
	}
}

/**
 * Sets the quantity of the item in the slot. Does not work if the slot is empty.
 * @param OrderQuantity The new quantity. Values <= zero will clear the slot.
 * @return True on success, false otherwise
 */
bool UInventorySlot::SetQuantity(int OrderQuantity)
{
	if (!ContainsValidItem()) { return false; }
	const int NewQuantity = GetQuantity() + (OrderQuantity > 0 ? OrderQuantity : 0);
	if (NewQuantity < 1)
	{
		ResetAndEmptySlot();
		return true;
	}
	const int QuantityMax = DataAsset_->GetItemMaxStackSize();
	Quantity_ = NewQuantity > QuantityMax ? QuantityMax : NewQuantity;
	return (Quantity_ == NewQuantity || Quantity_ == QuantityMax);
}

/**
 * Performs an absolute value of the quantity, decreasing
 * the quantity of the item in the slot.
 * @param OrderQuantity The amount to decrease by
 * @return The new quantity of the item in this slot. Negative indicates failure.
 */
int UInventorySlot::DecreaseQuantity(int OrderQuantity)
{
	const int AdjustQuantity = abs(OrderQuantity);
	if (SetQuantity(GetQuantity() - AdjustQuantity))
	{
		return GetQuantity();
	}
	return -1;
}

/**
 * Performs an absolute value of the quantity, increasing
 * the quantity of the item in the slot.
 * @param OrderQuantity The amount to increase by
 * @return The new quantity of the item in this slot. Negative indicates failure.
 */
int UInventorySlot::IncreaseQuantity(int OrderQuantity)
{
	const int AdjustQuantity = abs(OrderQuantity);
	if (SetQuantity(GetQuantity() + AdjustQuantity))
	{
		return GetQuantity();
	}
	return -1;
}

bool UInventorySlot::GetIsEquipmentSlot() const
{
	if (!SlotTags.HasTag(TAG_Inventory_Slot_Equipment))
		{ return SlotTags.HasTag(TAG_Equipment_Slot); }
	return true;
}

bool UInventorySlot::SetDurability(float DurabilityValue)
{
	if (FMath::IsNearlyZero(DurabilityValue, 0.001))
		{ return false; }
	if (ContainsValidItem())
	{
		// Item is vulnerable/fragile
		if (DataAsset_->GetItemMaxDurability() > 0.f)
		{
			ItemStatics_.Durability = DurabilityValue;
			return true;
		}
	}
	return false;
}

float UInventorySlot::DamageDurability(float DamageAmount)
{
	const int AdjustQuantity = abs(DamageAmount);
	if (SetDurability(GetDurability() - AdjustQuantity))
	{
		return GetDurability();
	}
	return -1.f;
}

float UInventorySlot::RepairDurability(float RepairAmount)
{
	const int AdjustQuantity = abs(RepairAmount);
	if (SetDurability(GetDurability() + AdjustQuantity))
	{
		return GetDurability();
	}
	return -1.f;
}

float UInventorySlot::GetDurability() const
{
	return ContainsValidItem() ? ItemStatics_.Durability : -1.f;
}

int UInventorySlot::GetMaxStackAllowance() const
{
	return ContainsValidItem() ? DataAsset_->GetItemMaxStackSize() : 0;
}

/**
 * Called when a slot has been activated by the character, performing validity
 * checks of whether activation is allowed or not. If the activation is
 * successful, 'OnSlotActivated(SlotNumber, ItemDataAsset)' will be triggered.
 */
void UInventorySlot::Activate()
{
	if (!ContainsValidItem())
	{
		UE_LOGFMT(LogTemp, Display,
			"{iSlot}({nSlot}): Activation Failed - This slot does not contain any items.",
			GetNameSafe(this), SlotNumber_);
		return;
	}
	
	if (!DataAsset_->GetItemCanActivate())
	{
		UE_LOGFMT(LogTemp, Display,
			"{iSlot}({nSlot}): The item ({ItemName}) in this slot does not activate.",
			GetNameSafe(this), SlotNumber_, ItemStatics_.ItemName);
		return;
	}
	
	UE_LOGFMT(LogTemp, Display,
		"{iSlot}({nSlot}): Activating Item '{ItemName}'",
		GetNameSafe(this), SlotNumber_, ItemStatics_.ItemName);
		
	// Notify listeners
	OnSlotActivated.Broadcast(SlotNumber_, GetItemData());
}

/**
 * Checks if this slot contains the item given. If ItemStatics is given, the check makes sure that
 * the item in this slot is the exact same item, including durability, rarity and value.
 * Method is NOT a reliable way to check if the slot is empty.
 * @param DataAsset The data asset to check for.
 * @param ItemStatics (opt) If valid, checks that the item is an exact match
 * @return 
 */
bool UInventorySlot::ContainsItem(const UItemDataAsset* DataAsset, const FItemStatics& ItemStatics) const
{
	if (IsValid(DataAsset) && ContainsValidItem())
	{
		if (ItemStatics.ItemName.IsNone())
		{
			return true;
		}
		return (ItemStatics_ == ItemStatics);
	}
	return false;
}

/**
 * Adds a brand new item by the data asset name given, if the slot is empty or
 * if the slot contains the same item with the same generated statics.
 * Generates a result from all possible outcomes of the item.
 * To control exactly what is created, either copy an item using
 * 'CopyItemFromSlot', or use the overload function 'AddItem(FItemStatics, int)'
 * @param ItemName The Name of the DataAsset for the item to be added
 * @param NewQuantity The amount of the item to be added. (def: 1)
 */
void UInventorySlot::AddItem(const FName& ItemName, int NewQuantity)
{
	const FPrimaryAssetType itemType = UItemDataAsset::StaticClass()->GetFName();
	const FName itemName = ItemName;
	const FPrimaryAssetId newAssetId(itemType, itemName);

	if (newAssetId.IsValid())
	{
		AssetId_  = newAssetId;
		Quantity_ = NewQuantity;
		
		if (OnSlotItemChanged.IsBound())
		{
			OnSlotItemChanged.Broadcast(SlotNumber_, nullptr);
		}
	}
}

/**
 * Adds an existing or pre-determined item by the statics given, if the slot is empty or
 * if the slot contains the same item with the same generated statics.
 * To generate a new item using the items spawn logic, use 'AddItem(FName, int)'
 * @param ItemStatics The item statics for the item to be added to the slot
 * @param NewQuantity The amount of the item to be added. (def: 1)
 */
void UInventorySlot::AddItem(const FItemStatics& ItemStatics, int NewQuantity)
{
	const FPrimaryAssetType itemType = UItemDataAsset::StaticClass()->GetFName();
	const FName itemName = ItemStatics.ItemName;
	const FPrimaryAssetId newAssetId(itemType, itemName);

	if (newAssetId.IsValid())
	{
		// Set the item data values
		ItemStatics_	= FItemStatics(ItemStatics);
		Quantity_ 		= NewQuantity;

		// Load the data asset
		AssetId_  		= newAssetId;
		
		if (OnSlotItemChanged.IsBound())
		{
			OnSlotItemChanged.Broadcast(SlotNumber_, nullptr);
		}
	}
}

/**
 * Copies an item from a different inventory slot, making this slot match
 * @param SlotReference The inventory slot to copy the item from
 * @param NewQuantity If < 1, it will also copy the quantity
 */
void UInventorySlot::CopyItemFromSlot(const UInventorySlot* SlotReference, int NewQuantity)
{
	if (!IsSlotEmpty())
		{ return; }
	
	if (IsValid(SlotReference))
	{
		if (SlotReference->ContainsValidItem())
		{
			AddItem(SlotReference->GetItemStatics(),
				NewQuantity > 0 ? NewQuantity : SlotReference->GetQuantity());
		}
	}
}

bool UInventorySlot::ContainsValidItem() const
{ return IsValid(DataAsset_) && Quantity_ > 0; }

/**
 * Unbinds all listeners, destroys references, empties the slot,
 * and attempts to unload the asset through asset manager
 */
void UInventorySlot::ResetAndEmptySlot()
{
	// Reset Relevant Data
	Quantity_		= 0;

	// Resets the item data & asset reference on replication
	AssetId_ = FPrimaryAssetId();
}

bool UInventorySlot::IsSlotEmpty() const
{ return !IsValid(DataAsset_) || Quantity_ < 1; }

void UInventorySlot::AssetLoadedDelegate()
{
	const UAssetManager& AssetManager = UAssetManager::Get();
	DataAsset_ = Cast<UItemDataAsset>(AssetManager.GetPrimaryAssetObject(AssetId_));
}

void UInventorySlot::OnRep_AssetId_Implementation()
{
	UAssetManager& AssetManager = UAssetManager::Get();
	const UItemDataAsset* tempAsset = Cast<UItemDataAsset>(AssetManager.GetPrimaryAssetObject(AssetId_));

	// The asset is already loaded
	if (IsValid(tempAsset))
		{ DataAsset_ = tempAsset; }
	
	else
	{
		// If the AssetID is valid, and the object is not, load the asset
		if (AssetId_.IsValid())
		{
			const TArray<FName> AssetBundle = {};
			const FStreamableDelegate StreamDelegate =
				FStreamableDelegate::CreateUObject(this, &UInventorySlot::AssetLoadedDelegate);
			AssetManager.LoadPrimaryAsset(AssetId_, AssetBundle, StreamDelegate);
		}
		// If the assetID is not valid, this is an empty slot
		else
		{
			const UItemDataAsset* ItemReference = DataAsset_;
			ItemStatics_	= FItemStatics();
			DataAsset_		= nullptr;
			if (IsValid(ItemReference))
			{
				if (OnSlotItemChanged.IsBound())
				{
					OnSlotItemChanged.Broadcast(SlotNumber_, ItemReference);
				}
			}
		}
	}
}
