#pragma once

#include "CoreMinimal.h"
#include "ItemData.h"
#include "InventoryData.h"

#include "InventorySlot.generated.h"

class UInventoryComponent;

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Uninit);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Generic);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Equipment);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Locked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Mirrored);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Hidden);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Uninit);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Primary);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Secondary);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Ranged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Ammunition);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Head);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Face);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Neck);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Torso);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Shoulders);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Arms);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Wrists);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Wrists_Left);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Wrists_Right);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Ring);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Ring_Left);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Ring_Right);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Waist);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Legs);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Anklet);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot_Feet);


USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStInventorySlot
{
	GENERATED_BODY()

	TMulticastDelegate<void(int SlotNumber)> OnSlotActivated;
	TMulticastDelegate<void(int SlotNumber)> OnSlotUpdated;
	
	FStInventorySlot();
	FStInventorySlot(const FGameplayTag& InvSlotType);
	FStInventorySlot(const FStItemData& ItemReference);
	FStInventorySlot(const UItemDataAsset* ItemDataAsset, int OrderQuantity = 1);

	~FStInventorySlot();

	void SetAsEquipmentSlot(const FGameplayTag& NewEquipmentTag);

	void OnItemUpdated() const;
	void OnItemActivated() const;
	void OnItemDurabilityChanged(const float OldDurability, const float NewDurability) const;
	
	FGameplayTag	GetEquipmentTag() const;
	FStItemData*	GetItemData();
	FStItemData		GetCopyOfItemData() const;
	
	void		 SetNewItemInSlot(
					const FStItemData& ReferenceItem, const int OverrideQuantity = 0);

	bool		 GetIsEquipmentSlot() const { return SlotTag == TAG_Inventory_Slot_Equipment.GetTag(); }
	void 		 DamageDurability(float DamageAmount = 0.f);
	void 		 RepairDurability(float RepairAmount = 0.f);

	bool		 SetQuantity(int NewQuantity);
	int 		 IncreaseQuantity(int OrderQuantity = 1);
	int 		 DecreaseQuantity(int OrderQuantity = 1);
	int			 GetQuantity() const;

	bool		 ContainsValidItem() const;
	bool 		 IsSlotEmpty() const;
	bool		 IsSlotFull() const;

	float		 GetWeight() const;
	void 		 EmptyAndResetSlot();
	int	 		 GetMaxStackAllowance() const;
	void 		 Activate();
	bool 		 ContainsItem(
					const FStItemData& ItemData, const bool bExact = false) const;

	int64 LastUpdate = 0;

	bool operator==(const FStInventorySlot& ComparisonSlot) const
	{
		if (ParentInventory != nullptr)					{ return false; }
		if (ComparisonSlot.ParentInventory != nullptr)	{ return false; }
		if (ComparisonSlot.ParentInventory == ParentInventory)
		{
			if (ComparisonSlot.SlotNumber  == SlotNumber)
			{
				if (ComparisonSlot.SlotTag == SlotTag)
				{
					return (ComparisonSlot.SlotItemData == SlotItemData);
				}
			}
		}
		return false;
	}

	UPROPERTY() UInventoryComponent* ParentInventory = nullptr;
	
	// The proper item information for the item that is "in" this slot
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) FStItemData SlotItemData;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite) bool bHideSlotIfEmpty = false;

	// The type of this inventory slot
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite)
	FGameplayTag SlotTag = TAG_Inventory_Slot_Generic;

	// The equipment relationship to this inventory slot
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite)
	FGameplayTag EquipmentTag = FGameplayTag::EmptyTag;

	UPROPERTY(SaveGame) int SlotNumber = -1;
	
};