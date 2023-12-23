#pragma once

#include "CoreMinimal.h"
#include "ItemData.h"
#include "InventoryData.h"
#include "EquipmentData.h"

#include "InventorySlot.generated.h"


UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Generic);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Equipment);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Locked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Mirrored);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Inventory_Slot_Hidden);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Equipment_Slot);
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

	TMulticastDelegate<void(const FStInventorySlot&, int SlotNumber)> OnSlotActivated;
	TMulticastDelegate<void(const FStInventorySlot&, int SlotNumber)> OnSlotUpdated;
	
	FStInventorySlot();
	FStInventorySlot(const FGameplayTag& InvSlotType);
	FStInventorySlot(const UItemDataAsset* ItemDataAsset);

	~FStInventorySlot();

	void BindEventListeners();
	void SetAsEquipmentSlot(const FGameplayTag& NewEquipmentTag);

	void OnItemUpdated() const;
	void OnItemActivated() const;
	void OnItemDurabilityChanged(const float OldDurability, const float NewDurability) const;
	
	FStItemData& GetItemData();
	void		 SetNewItemInSlot(
					const FStItemData& ReferenceItem, const int NewQuantity = 1);
	void 		 DamageDurability(float DamageAmount = 0.f);
	void 		 RepairDurability(float RepairAmount = 0.f);
	bool 		 AddQuantity(int AddAmount = 1);
	bool 		 RemoveQuantity(int RemoveAmount = 1);
	bool 		 IsSlotEmpty() const;
	void 		 EmptyAndResetSlot();
	int	 		 GetMaxStackAllowance() const;
	void 		 Activate() const;
	bool 		 ContainsItem(
					const FStItemData& ItemData, const bool bExact = false) const;

	// The proper item information for the item that is "in" this slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FStItemData SlotItemData;
	
	// The quantity of the item in this slot. Zero means empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int SlotQuantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHideSlotIfEmpty = false;

	// The type of this inventory slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag SlotTag = TAG_Inventory_Slot_Generic;

	// The equipment relationship to this inventory slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag EquipmentTag = FGameplayTag::EmptyTag;

	int SlotNumber = -1;
	
};
