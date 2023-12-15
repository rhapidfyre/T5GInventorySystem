#pragma once

#include "CoreMinimal.h"
#include "ItemData.h"
#include "InventoryData.h"
#include "EquipmentData.h"

#include "InventorySlot.generated.h"

USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStInventorySlot
{
	GENERATED_BODY()

	TMulticastDelegate<void(const int, EInventorySlotType)> OnSlotActivated;
	
	FStInventorySlot();
	FStInventorySlot(EInventorySlotType InvSlotType);
	FStInventorySlot(EEquipmentSlotType EquipSlotType);
	FStInventorySlot(FName InitItemName, int InitQuantity = 1);
	
	void DamageDurability(float DamageAmount);
	void RepairDurability(float DamageAmount);
	bool AddQuantity(int AddAmount = 1);
	bool RemoveQuantity(int RemoveAmount = 1);
	FStItemData GetItemData() const;
	bool IsSlotEmpty() const;
	bool ContainsItem(FName itemName = FName()) const;
	void EmptyAndResetSlot();
	int GetMaxStackAllowance() const;
	void Activate();

	// The proper game name of the item in this slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemName = FName();
	
	// The quantity of the item in this slot. Zero means empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int SlotQuantity = 0;
	
	// The quantity of the item in this slot. Zero means empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SlotDurability = 0.f;

	// If TRUE, this slot exists and can be used, but does not exist for all intents and purposes
	// Useful for things like not dropping the item on death
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIgnoreSlot = false;

	// The type of this inventory slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EInventorySlotType SlotType = EInventorySlotType::NONE;

	// The equipment relationship to this inventory slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEquipmentSlotType EquipType = EEquipmentSlotType::NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int SlotNumber = 0;
    
};
