
#include "lib/InventorySlot.h"
#include "lib/ItemData.h"
#include "Logging/StructuredLog.h"


UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory,					"Inventory");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot,				"Inventory.SlotType");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Uninit,		"Inventory.SlotType.Uninitialized");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Generic,		"Inventory.SlotType.Generic");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Equipment,	"Inventory.SlotType.Equipment");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Locked,		"Inventory.SlotType.Locked");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Mirrored, 	"Inventory.SlotType.Mirrored");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Hidden,		"Inventory.SlotType.Hidden");

UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment,					"Equipment");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot,				"Equipment.SlotType");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Uninit,		"Inventory.SlotType.Uninitialized");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Primary,		"Equipment.SlotType.Primary");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Secondary,	"Equipment.SlotType.Secondary");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Ranged,		"Equipment.SlotType.Ranged");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Ammunition,	"Equipment.SlotType.Ammunition");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Head,			"Equipment.SlotType.Head");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Face,			"Equipment.SlotType.Face");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Neck,			"Equipment.SlotType.Neck");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Torso,		"Equipment.SlotType.Torso");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Shoulders,	"Equipment.SlotType.Shoulders");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Arms,			"Equipment.SlotType.Arms");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Wrists,		"Equipment.SlotType.Wrist");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Ring,			"Equipment.SlotType.Ring");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Ring_Left,	"Equipment.SlotType.Ring.Left");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Ring_Right,	"Equipment.SlotType.Ring.Right");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Waist,		"Equipment.SlotType.Waist");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Legs,			"Equipment.SlotType.Legs");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Anklet,		"Equipment.SlotType.Anklet");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot_Feet,			"Equipment.SlotType.Feet");


// Generic Constructor for initializing an un-usable inventory slot
FStInventorySlot::FStInventorySlot()
{
	
}


FStInventorySlot::FStInventorySlot(const FGameplayTag& InvSlotType)
{
	SetAsEquipmentSlot(InvSlotType);
}

FStInventorySlot::FStInventorySlot(const FStItemData& ItemReference)
{
	if (ItemReference.GetIsValidItem())
	{
		SetNewItemInSlot(FStItemData(ItemReference));
	}
}

// For initializing with an item in it
FStInventorySlot::FStInventorySlot(const UItemDataAsset* ItemDataAsset, int OrderQuantity)
{
	if (IsValid(ItemDataAsset))
	{
		SetNewItemInSlot(FStItemData(ItemDataAsset), OrderQuantity);
	}
}

FStInventorySlot::~FStInventorySlot()
{
	if (OnSlotActivated.IsBound()) { OnSlotActivated.Clear(); }
	if (OnSlotUpdated.IsBound()) { OnSlotUpdated.Clear(); }
}


void FStInventorySlot::SetAsEquipmentSlot(const FGameplayTag& NewEquipmentTag)
{
	const FGameplayTag ParentTag = NewEquipmentTag.RequestDirectParent();
	if (ParentTag == TAG_Equipment_Slot.GetTag())
	{
		SlotTag			= TAG_Inventory_Slot_Equipment.GetTag();
		EquipmentTag	= NewEquipmentTag;
	}
	else if (ParentTag == TAG_Inventory_Slot.GetTag())
	{
		SlotTag			= NewEquipmentTag;
		EquipmentTag	= FGameplayTag::EmptyTag;
	}
	else
	{
		SlotTag			= TAG_Inventory_Slot_Generic.GetTag();
		EquipmentTag	= FGameplayTag::EmptyTag;
	}
}

void FStInventorySlot::OnItemUpdated() const
{
	if (OnSlotUpdated.IsBound()) {OnSlotUpdated.Broadcast(this->SlotNumber);}
}

void FStInventorySlot::OnItemActivated() const
{
	if (OnSlotActivated.IsBound()) {OnSlotActivated.Broadcast(this->SlotNumber);}
}

void FStInventorySlot::OnItemDurabilityChanged(const float OldDurability, const float NewDurability) const
{
	if (OnSlotUpdated.IsBound()) {OnSlotUpdated.Broadcast(this->SlotNumber);}
}

FGameplayTag FStInventorySlot::GetEquipmentTag() const
{
	return EquipmentTag;
}

void FStInventorySlot::DamageDurability(float DamageAmount)
{
	SlotItemData.DamageItem(DamageAmount);
}

void FStInventorySlot::RepairDurability(float RepairAmount)
{
	SlotItemData.RepairItem(RepairAmount);
}

bool FStInventorySlot::SetQuantity(int NewQuantity)
{
	return SlotItemData.SetItemQuantity(NewQuantity);
}


/**
 * Adds a specified amount of the item to this slot
 * @param OrderQuantity The amount to add to the slot
 * @return The number of items actually added
 */
int FStInventorySlot::IncreaseQuantity(int OrderQuantity)
{
	if (!ContainsValidItem()) { return SlotItemData.IncreaseQuantity(OrderQuantity); }
	return 0;
}

/**
 * Removes a specified amount of the item from this slot
 * @param OrderQuantity The amount to remove from the slot
 * @return The number of items actually removed
 */
int FStInventorySlot::DecreaseQuantity(int OrderQuantity)
{
	if (!ContainsValidItem()) { return SlotItemData.DecreaseQuantity(OrderQuantity); }
	return 0;
}

int FStInventorySlot::GetQuantity() const
{
	if (ContainsValidItem()) { return SlotItemData.ItemQuantity;}
	return 0;
}

bool FStInventorySlot::ContainsValidItem() const
{
	return SlotItemData.GetIsValidItem();
}

// Returns the data for the item in the slot
// If the item is empty, it will return default item data
FStItemData* FStInventorySlot::GetItemData()
{
	return &SlotItemData;
}

FStItemData FStInventorySlot::GetCopyOfItemData() const
{
	return SlotItemData;
}

/**
 * Overwrites the existing item in this slot by copying the reference item,
 * without compromising the slot settings (equipment tag, etc)
 * @param ReferenceItem		The item being referenced for the new item
 * @param OverrideQuantity	Optional quantity override.
 *							If <= 0, it will use the quantity from ReferenceItem
 */
void FStInventorySlot::SetNewItemInSlot(const FStItemData& ReferenceItem, const int OverrideQuantity)
{
	SlotItemData = FStItemData(ReferenceItem, OverrideQuantity);
	if (OnSlotUpdated.IsBound()) {OnSlotUpdated.Broadcast(this->SlotNumber);}
}

// Returns TRUE if the slot is empty (qty < 1 or invalid item)
bool FStInventorySlot::IsSlotEmpty() const
{
	return (!ContainsValidItem());
}

bool FStInventorySlot::IsSlotFull() const
{
	if (!ContainsValidItem()) { return false; }
	return (GetQuantity() >= GetMaxStackAllowance());
}

float FStInventorySlot::GetWeight() const
{
	if (!ContainsValidItem()) { return 0.f; }
	return SlotItemData.GetWeight();
}

/**
 * Returns whether or not this slot contains the same item.
 * @param ItemData The item we are referencing to check for
 * @param bExact If true, the items meta data must also be identical
 */
bool FStInventorySlot::ContainsItem(const FStItemData& ItemData, const bool bExact) const
{
	return bExact
		? SlotItemData.GetIsExactSameItem(ItemData)
		: SlotItemData.GetIsSameItem(ItemData);
}

// Sets the slot to empty without changing the slot meta such as equipment type
void FStInventorySlot::EmptyAndResetSlot()
{
	SlotItemData.DestroyItem();
	if (OnSlotUpdated.IsBound()) {OnSlotUpdated.Broadcast(this->SlotNumber);}
}

int FStInventorySlot::GetMaxStackAllowance() const
{
	if (!ContainsValidItem()) { return 0; }
	return SlotItemData.Data->GetItemMaxStackSize();
}

// Broadcasts OnSlotActivate and internally calls SlotItemData->Activate
void FStInventorySlot::Activate()
{
	if (!ContainsValidItem())	{ OnSlotActivated.Broadcast(SlotNumber); }
	else						{ SlotItemData.ActivateItem(false); };
}

