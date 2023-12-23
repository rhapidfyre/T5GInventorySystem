
#include "lib/InventorySlot.h"
#include "lib/ItemData.h"
#include "Logging/StructuredLog.h"


UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_,					"Inventory");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot,				"Inventory.SlotType");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Generic,		"Inventory.SlotType.Generic");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Equipment,	"Inventory.SlotType.Equipment");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Locked,		"Inventory.SlotType.Locked");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Mirrored, 	"Inventory.SlotType.Mirrored");
UE_DEFINE_GAMEPLAY_TAG(TAG_Inventory_Slot_Hidden,		"Inventory.SlotType.Hidden");

UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment,					"Equipment");
UE_DEFINE_GAMEPLAY_TAG(TAG_Equipment_Slot,				"Equipment.SlotType");
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

// For initializing with an item in it
FStInventorySlot::FStInventorySlot(const UItemDataAsset* ItemDataAsset)
{
	if (IsValid(ItemDataAsset))
	{
		
	}
}

FStInventorySlot::~FStInventorySlot()
{
	if (OnSlotActivated.IsBound()) { OnSlotActivated.Clear(); }
	if (OnSlotUpdated.IsBound()) { OnSlotUpdated.Clear(); }
}

void FStInventorySlot::BindEventListeners()
{
	if (SlotItemData.GetIsValidItem())
	{
		SlotItemData.OnItemUpdated.AddUObject(this, &FStInventorySlot::OnItemUpdated);
		SlotItemData.OnItemActivation.AddUObject(this, &FStInventorySlot::OnItemActivated);
		SlotItemData.OnItemDurabilityChanged.AddUObject(this, &FStInventorySlot::OnItemDurabilityChanged);
	}
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
	if (OnSlotUpdated.IsBound()) {OnSlotUpdated.Broadcast(*this, SlotNumber);}
}

void FStInventorySlot::OnItemActivated() const
{
	if (OnSlotActivated.IsBound()) {OnSlotActivated.Broadcast(*this, SlotNumber);}
}

void FStInventorySlot::OnItemDurabilityChanged(const float OldDurability, const float NewDurability) const
{
	if (OnSlotUpdated.IsBound()) {OnSlotUpdated.Broadcast(*this, SlotNumber);}
}

void FStInventorySlot::DamageDurability(float DamageAmount)
{
	SlotItemData.DamageItem(DamageAmount);
}

void FStInventorySlot::RepairDurability(float RepairAmount)
{
	SlotItemData.RepairItem(RepairAmount);
}

bool FStInventorySlot::AddQuantity(int AddAmount)
{
	if (!IsSlotEmpty())
	{
		// Item is stackable, and there is room for more
		const int maxStack = SlotItemData.Data->GetItemMaxStackSize();
		if ( (maxStack > 1) && (SlotQuantity < maxStack ) ) // Stackable
		{
			const int newQuantity = SlotQuantity + abs(AddAmount);
			SlotQuantity = newQuantity > maxStack ? maxStack : newQuantity;
			if (OnSlotUpdated.IsBound()) {OnSlotUpdated.Broadcast(*this);}
		}
	}
	return false;
}

bool FStInventorySlot::RemoveQuantity(int RemoveAmount)
{
	if (!IsSlotEmpty())
	{
		// If the quantity changed and isn't empty when finished
		const int NewQuantity = SlotQuantity - abs(RemoveAmount);
		if (NewQuantity < SlotQuantity)
		{
			if (NewQuantity <= 0)	{ EmptyAndResetSlot(); }
			else					{ SlotQuantity = NewQuantity; }
			if (OnSlotUpdated.IsBound()) {OnSlotUpdated.Broadcast(*this);}
			return true;
		}
	}
	return false;
}

// Returns the data for the item in the slot
// If the item is empty, it will return default item data
FStItemData& FStInventorySlot::GetItemData()
{
	return SlotItemData;
}

/**
 * Overwrites the existing item in this slot by copying the reference item,
 * without compromising the slot settings (equipment tag, etc)
 * @param ReferenceItem The item being referenced for the new item
 * @param NewQuantity The quantity of this slot, defaults to 1
 */
void FStInventorySlot::SetNewItemInSlot(const FStItemData& ReferenceItem, const int NewQuantity)
{
	SlotItemData = FStItemData(ReferenceItem);
	SlotQuantity = NewQuantity;
}

// Returns TRUE if the slot is empty (qty < 1 or invalid item)
bool FStInventorySlot::IsSlotEmpty() const
{
	return (SlotQuantity < 1 || !SlotItemData.GetIsValidItem());
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
	SlotQuantity = 0;
	SlotItemData.DestroyItem();
}

int FStInventorySlot::GetMaxStackAllowance() const
{
	if (SlotItemData.GetIsValidItem())
	{
		return SlotItemData.Data->GetItemMaxStackSize();
	}
	return 0;
}

// Broadcasts OnSlotActivate and internally calls SlotItemData->Activate
void FStInventorySlot::Activate() const
{
	if (!IsSlotEmpty())
	{
		// Activates the item in this slot.
		// ReSharper disable once CppExpressionWithoutSideEffects
		SlotItemData.ActivateItem();
	}
	// Activation of an empty slot?
	else { OnSlotActivated.Broadcast(*this); }
}

