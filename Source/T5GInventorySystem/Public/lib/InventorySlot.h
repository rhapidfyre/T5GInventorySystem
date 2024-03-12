#pragma once

#include "CoreMinimal.h"
#include "InventorySave.h"
#include "Delegates/Delegate.h"

#include "InventorySlot.generated.h"

class UInventoryComponent;
class UItemDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotUpdated, int, SlotNumber);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotItemChanged,
	int, SlotNumber, const UItemDataAsset*, OldItem);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotActivated,
	int, SlotNumber, const UItemDataAsset*, ActivatedItem);


UCLASS(Blueprintable, BlueprintType)
class T5GINVENTORYSYSTEM_API UInventorySlot : public UObject
{
	GENERATED_BODY()

public:

	// Called when the item in the slot has changed
	UPROPERTY(Blueprintable) FOnSlotItemChanged	OnSlotItemChanged;
	// Called when the item in the slot has activated
	UPROPERTY(Blueprintable) FOnSlotActivated	OnSlotActivated;
	// Called whenever the slot has updated, such as durability or quantity
	UPROPERTY(Blueprintable) FOnSlotUpdated		OnSlotUpdated;

	UInventorySlot() {SlotTags.AddTag(TAG_Inventory_Slot_Generic.GetTag());};
	
	UInventorySlot(const FGameplayTag& SlotTypeTag);
	
	UFUNCTION(NetMulticast, Reliable) void OnRep_AssetId(); // Loads thru Asset Manager
	UFUNCTION(NetMulticast, Reliable) void OnRep_ItemUpdated(const FItemStatics& ItemPreChanges);
	UFUNCTION(NetMulticast, Reliable) void OnRep_QuantityUpdated(int OldQuantity);

	UFUNCTION(BlueprintPure) FItemStatics GetItemStatics() const { return ItemStatics_; }
	
	UFUNCTION(BlueprintPure) const UItemDataAsset* GetItemData() const { return DataAsset_; }

	// Returns the quantity of items in slot, ensuring a return from 0-inf
	UFUNCTION(BlueprintCallable) bool SetQuantity(int OrderQuantity = 1);
	UFUNCTION(BlueprintCallable) int  DecreaseQuantity(int OrderQuantity = 1);
	UFUNCTION(BlueprintCallable) int  IncreaseQuantity(int OrderQuantity = 1);
	UFUNCTION(BlueprintPure)	 int  GetQuantity() const { return Quantity_ > 0 ? Quantity_ : 0;}
	
	bool GetIsEquipmentSlot() const;
	
	bool SetDurability(float DurabilityValue = 0.f);
	float DamageDurability(float DamageAmount = 0.f);
	float RepairDurability(float RepairAmount = 0.f);
	UFUNCTION(BlueprintPure) float GetDurability() const;
	
	int	 GetMaxStackAllowance() const;
	void Activate();
	bool ContainsItem(const UItemDataAsset* DataAsset,
		const FItemStatics& ItemStatics = FItemStatics()) const;
	
	// Sets the item that is in this slot
	void AddItem(const FName& ItemName, int NewQuantity = 1);

	void AddItem(const FItemStatics& ItemStatics, int NewQuantity = 1);

	// Makes a 1-for-1 copy of the item in an existing inventory slot
	void CopyItemFromSlot(const UInventorySlot* SlotReference, int NewQuantity = 0);

	bool ContainsValidItem() const;

	void ResetAndEmptySlot();

	bool IsSlotEmpty() const;
	
private:
	
	UPROPERTY() UInventoryComponent* ParentInventory = nullptr;
	
	UFUNCTION() void AssetLoadedDelegate(); // Called when asset loads
	
	UPROPERTY(ReplicatedUsing=OnRep_AssetId)		 FPrimaryAssetId AssetId_;
	UPROPERTY(ReplicatedUsing=OnRep_ItemUpdated)	 FItemStatics	 ItemStatics_;
	UPROPERTY(ReplicatedUsing=OnRep_QuantityUpdated) int			 Quantity_ = 0;

	// Set by OnRep_AssetId. If pointer is valid, slot contains a valid item.
	UPROPERTY() const UItemDataAsset* DataAsset_ = nullptr;
	
	UPROPERTY(Replicated) int SlotNumber_ = 0;

	FGameplayTagContainer SlotTags;
	
};