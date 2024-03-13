#include "lib/InventorySlot.h"

#include "InventoryComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "lib/InventorySave.h"
#include "lib/ItemData.h"
#include "Logging/StructuredLog.h"


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


bool UInventorySlot::HasAuthority() const
{
	const UInventoryComponent* parentInventory = GetParentInventory();
	if (IsValid(parentInventory)) { return parentInventory->HasAuthority(); }
	return false;
}


bool UInventorySlot::TryLoadAsset(const FPrimaryAssetId& AssetId)
{
	UAssetManager&        AssetManager = UAssetManager::Get();
	const UItemDataAsset* tempAsset    = Cast<UItemDataAsset>(AssetManager.GetPrimaryAssetObject(AssetId));

	// The asset is already loaded
	if (IsValid(tempAsset)) { return true; }
	
	// If the AssetID is valid, and the object is not, load the asset
	if (AssetId.IsValid())
	{
		const TArray<FName> AssetBundle = {};
		const FStreamableDelegate StreamDelegate = FStreamableDelegate::CreateUObject(
			this, &UInventorySlot::AssetLoadedDelegate);
		AssetManager.LoadPrimaryAsset(AssetId, AssetBundle, StreamDelegate);
		return true;
	}
	return false;
}

/**
 * Attempts to create an inventory slot, modeled after the save data 
 * @param SaveData The save data to restore the slot from
 */
void UInventorySlot::RestoreInventorySlot(const FInventorySlotSaveData& SaveData)
{
	Quantity_    = SaveData.Quantity;
	ItemStatics_ = SaveData.SavedItemStatics;
	AssetId_     = SaveData.SavedAssetId;
	SlotTags_	 = SaveData.SavedSlotTags;
}


void UInventorySlot::AddInventorySlotTags(const FGameplayTagContainer& NewTags)
{
	for (auto NewTag : NewTags.GetGameplayTagArray())
	{
		AddInventorySlotTag(NewTag);
	}
}


void UInventorySlot::AddInventorySlotTag(const FGameplayTag& NewTag)
{
	if (NewTag.MatchesTag(TAG_Inventory) || NewTag.MatchesTag(TAG_Equipment))
	{
		SlotTags_.AddTag(NewTag);
	}
}


void UInventorySlot::InitializeSlot(const UInventoryComponent* ParentInventory, int SlotNumber)
{
	ParentInventory_ = ParentInventory;
	SlotNumber_      = SlotNumber;
}


const UEquipmentDataAsset* UInventorySlot::GetItemDataAsEquipment() const
{
	return Cast<UEquipmentDataAsset>(DataAsset_);
}

/**
 * Sets the quantity of the item in the slot. Does not work if the slot is empty.
 * @param OrderQuantity The new quantity. Values <= zero will clear the slot.
 * @return True on success, false otherwise
 */
bool                       UInventorySlot::SetQuantity(int OrderQuantity)
{
	if (!ContainsValidItem())
	{
		return false;
	}

	const int NewQuantity = GetQuantity() + (OrderQuantity > 0 ? OrderQuantity : 0);
	if (NewQuantity < 1)
	{
		ResetAndEmptySlot();
		return true;
	}

	const int QuantityMax = DataAsset_->GetItemMaxStackSize();
	Quantity_             = NewQuantity > QuantityMax ? QuantityMax : NewQuantity;

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
	if (!SlotTags_.HasTag(TAG_Inventory_Slot_Equipment))
	{
		return SlotTags_.HasTag(TAG_Equipment_Slot);
	}
	return true;
}


bool UInventorySlot::SetDurability(float DurabilityValue)
{
	if (FMath::IsNearlyZero(DurabilityValue, 0.001))
	{
		return false;
	}
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
void UInventorySlot::ActivateSlot()
{
	if (!ContainsValidItem())
	{
		UE_LOGFMT(LogTemp, Display, "{iSlot}({nSlot}): Activation Failed - This slot does not contain any items.",
				  GetNameSafe(this), SlotNumber_);
		return;
	}

	if (!DataAsset_->GetItemCanActivate())
	{
		UE_LOGFMT(LogTemp, Display, "{iSlot}({nSlot}): The item ({ItemName}) in this slot does not activate.",
				  GetNameSafe(this), SlotNumber_, ItemStatics_.ItemName);
		return;
	}

	UE_LOGFMT(LogTemp, Display, "{iSlot}({nSlot}): Activating Item '{ItemName}'", GetNameSafe(this), SlotNumber_,
			  ItemStatics_.ItemName);

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
 * Checks if this slot contains the item given. If ItemStatics is given, the check makes sure that
 * the item in this slot is the exact same item, including durability, rarity and value.
 * Method is NOT a reliable way to check if the slot is empty.
 * @param ItemName The item name to check for.
 * @param ItemStatics (opt) If valid, checks that the item is an exact match
 * @return 
 */
bool UInventorySlot::ContainsItem(const FName& ItemName, const FItemStatics& ItemStatics) const
{
	const FPrimaryAssetType itemType = UItemDataAsset::StaticClass()->GetFName();
	const FPrimaryAssetId   newAssetId(itemType, ItemName);
	if (!ItemStatics_.ItemName.IsNone())
	{
		if (newAssetId == AssetId_)
		{
			return ItemStatics == ItemStatics_;
		}
	}
	return (newAssetId == AssetId_);
}


float UInventorySlot::GetCarryWeight() const
{
	if (ContainsValidItem())
	{
		return GetQuantity() * DataAsset_->GetItemCarryWeight();
	}
	return ITEM_WEIGHT_EMPTY;
}


/**
 * Adds a brand new item by the FName given, if the slot is empty or
 * if the slot contains the same item with the same generated statics.
 * Generates a result from all possible outcomes of the item.
 * To control exactly what is created, either copy an item using
 * 'CopyItemFromSlot', or use the overload function 'AddItem(FItemStatics, int)'
 * @param ItemName The Name of the DataAsset for the item to be added
 * @param NewQuantity The amount of the item to be added. (def: 1)
 * @return The number of items successfully added
 */
int UInventorySlot::AddItem(const FName& ItemName, int NewQuantity)
{
	const FPrimaryAssetType itemType = UItemDataAsset::StaticClass()->GetFName();
	const FName             itemName = ItemName;
	const FPrimaryAssetId   newAssetId(itemType, itemName);

	if (newAssetId.IsValid())
	{
		// if server, load the asset and make sure the item can be added
		if (HasAuthority())
		{
			if (!TryLoadAsset(newAssetId)) { return 0; }
		}
		
		AssetId_  = newAssetId;
		Quantity_ = NewQuantity;
	}
	return -1;
}


/**
 * Adds an existing or pre-determined item by the statics given, if the slot is empty or
 * if the slot contains the same item with the same generated statics.
 * To generate a new item using the items spawn logic, use 'AddItem(FName, int)'
 * @param ItemStatics The item statics for the item to be added to the slot
 * @param NewQuantity The amount of the item to be added. (def: 1)
 * @return The number of items successfully added
 */
int UInventorySlot::AddItem(const FItemStatics& ItemStatics, int NewQuantity)
{
	const FPrimaryAssetType itemType = UItemDataAsset::StaticClass()->GetFName();
	const FName             itemName = ItemStatics.ItemName;
	const FPrimaryAssetId   newAssetId(itemType, itemName);

	if (newAssetId.IsValid())
	{
		// Checks if the slot already contains an item
		if (ContainsValidItem())
		{
			if (ContainsItem(ItemStatics.ItemName, ItemStatics))
			{
				// Stack the items
			}
			return 0;
		}
		
		// Set the item data values
		ItemStatics_ = FItemStatics(ItemStatics);
		Quantity_    = NewQuantity;

		// Load the data asset
		AssetId_ = newAssetId;
	}
	return -1;
}


bool UInventorySlot::SetItem(const FItemStatics& ItemStatics, int NewQuantity)
{
	const FPrimaryAssetType itemType = UItemDataAsset::StaticClass()->GetFName();
	const FName             itemName = ItemStatics.ItemName;
	const FPrimaryAssetId   newAssetId(itemType, itemName);
	if (newAssetId.IsValid())
	{
		// Checks if the slot already contains an item
		if (ContainsValidItem())
		{
			if (ContainsItem(ItemStatics.ItemName, ItemStatics))
			{
				// Stack the items
			}
			return false;
		}
		
		// Set the item data values
		ItemStatics_ = FItemStatics(ItemStatics);
		Quantity_    = NewQuantity;

		// Load the data asset
		AssetId_ = newAssetId;
		return true;
	}
	return false;
}


/**
 * Copies an item from a different inventory slot, making this slot match
 * @param SlotReference The inventory slot to copy the item from
 * @param NewQuantity If < 1, it will also copy the quantity
 */
void UInventorySlot::CopyItemFromSlot(const UInventorySlot* SlotReference, int NewQuantity)
{
	if (!IsEmpty())
	{
		return;
	}

	if (IsValid(SlotReference))
	{
		if (SlotReference->ContainsValidItem())
		{
			AddItem(SlotReference->GetItemStatics(), NewQuantity > 0 ? NewQuantity : SlotReference->GetQuantity());
		}
	}
}


bool UInventorySlot::ContainsValidItem() const
{
	return IsValid(DataAsset_) && Quantity_ > 0;
}


/**
 * Unbinds all listeners, destroys references, empties the slot,
 * and attempts to unload the asset through asset manager
 */
void UInventorySlot::ResetAndEmptySlot()
{
	// Reset Relevant Data
	Quantity_ = 0;

	// Resets the item data & asset reference on replication
	AssetId_ = FPrimaryAssetId();
}


bool UInventorySlot::IsEmpty() const
{
	return !IsValid(DataAsset_) || Quantity_ < 1;
}


bool UInventorySlot::IsFull() const
{
	if (ContainsValidItem())
	{
		return GetQuantity() >= GetMaxStackAllowance();
	}
	return false;
}


bool UInventorySlot::ContainsTag(const FGameplayTag& SearchTag) const
{
	return SlotTags_.HasTag(SearchTag);
}


bool UInventorySlot::ContainsTag(const FGameplayTagContainer& SearchTags) const
{
	for (auto SearchTag : SearchTags)
	{
		if (SlotTags_.HasTag(SearchTag))
		{
			return true;
		}
	}
	return false;
}


void UInventorySlot::AssetLoadedDelegate()
{
	const UAssetManager& AssetManager = UAssetManager::Get();
	DataAsset_                        = Cast<UItemDataAsset>(AssetManager.GetPrimaryAssetObject(AssetId_));

	// Validate the pre-load data
	if (IsValid(DataAsset_))
	{
		int maxStack = DataAsset_->GetItemMaxStackSize();
		if (GetQuantity() > maxStack) { SetQuantity(maxStack); }
	}
	
}


void UInventorySlot::OnRep_AssetId_Implementation()
{
	UAssetManager&        AssetManager = UAssetManager::Get();
	const UItemDataAsset* tempAsset    = Cast<UItemDataAsset>(AssetManager.GetPrimaryAssetObject(AssetId_));

	// The asset is already loaded
	if (IsValid(tempAsset))
	{
		DataAsset_ = tempAsset;
	}

	else
	{
		// If the AssetID is valid, and the object is not, load the asset
		if (AssetId_.IsValid())
		{
			const TArray<FName>       AssetBundle    = {};
			const FStreamableDelegate StreamDelegate = FStreamableDelegate::CreateUObject(
				this, &UInventorySlot::AssetLoadedDelegate);
			AssetManager.LoadPrimaryAsset(AssetId_, AssetBundle, StreamDelegate);
		}
		// If the assetID is not valid, this is an empty slot
		else
		{
			const UItemDataAsset* ItemReference = DataAsset_;
			ItemStatics_                        = FItemStatics();
			DataAsset_                          = nullptr;
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
