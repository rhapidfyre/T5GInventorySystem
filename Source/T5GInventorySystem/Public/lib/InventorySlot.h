#pragma once

#include "CoreMinimal.h"
#include "Data/ItemStatics.h"
#include "Delegates/Delegate.h"

#include "InventorySlot.generated.h"

struct FInventorySlotSaveData;

class UInventoryComponent;
class UItemDataAsset;
class UEquipmentDataAsset;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotUpdated,
											int, SlotNumber);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotItemChanged,
											 int, SlotNumber,
											 const UItemDataAsset*, OldItem);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotActivated,
											 int, SlotNumber,
											 const UItemDataAsset*, ActivatedItem);


UCLASS(Blueprintable, BlueprintType)
class T5GINVENTORYSYSTEM_API UInventorySlot : public UObject
{
	GENERATED_BODY()


public:

	UFUNCTION(BlueprintPure) bool HasAuthority() const;

	// Called when the item in the slot has changed
	UPROPERTY(Blueprintable)
	FOnSlotItemChanged OnSlotItemChanged;

	// Called when the item in the slot has activated
	UPROPERTY(Blueprintable)
	FOnSlotActivated OnSlotActivated;

	// Called whenever the slot has updated, such as durability or quantity
	UPROPERTY(Blueprintable)
	FOnSlotUpdated OnSlotUpdated;
	
	UInventorySlot() {};

	bool TryLoadAsset(const FPrimaryAssetId& AssetId);

	void RestoreInventorySlot(const FInventorySlotSaveData& SaveData);

	void AddInventorySlotTags(const FGameplayTagContainer& NewTags);
	
	void AddInventorySlotTag(const FGameplayTag& NewTag);

	// Once initialize slot is called, the slot will need to be reinitialized in
	// order to change the slot tags, or the parent inventory reference.
	void InitializeSlot(const UInventoryComponent* ParentInventory, int SlotNumber);

	UFUNCTION(BlueprintPure) int GetSlotNumber() const { return SlotNumber_; }
	
	UFUNCTION(NetMulticast, Reliable)
	void OnRep_AssetId(); // Loads thru Asset Manager
	UFUNCTION(NetMulticast, Reliable)
	void OnRep_ItemUpdated(const FItemStatics& ItemPreChanges);

	UFUNCTION(NetMulticast, Reliable)
	void OnRep_QuantityUpdated(int OldQuantity);

	UFUNCTION(BlueprintPure)
	const FItemStatics& GetItemStatics() const
	{
		return ItemStatics_;
	}

	UFUNCTION(BlueprintPure)
	const FName& GetItemName() const { return ItemStatics_.ItemName;}

	UFUNCTION(BlueprintPure)
	const UItemDataAsset* GetItemData() const
	{
		return DataAsset_;
	}
	
	UFUNCTION(BlueprintPure)
	const UEquipmentDataAsset* GetItemDataAsEquipment() const;


	// Returns the quantity of items in slot, ensuring a return from 0-inf
	UFUNCTION(BlueprintCallable)
	bool SetQuantity(int OrderQuantity = 1);

	UFUNCTION(BlueprintCallable)
	int DecreaseQuantity(int OrderQuantity = 1);

	UFUNCTION(BlueprintCallable)
	int IncreaseQuantity(int OrderQuantity = 1);

	UFUNCTION(BlueprintPure)
	int GetQuantity() const
	{
		return Quantity_ > 0 ? Quantity_ : 0;
	}


	bool GetIsEquipmentSlot() const;

	bool SetDurability(float DurabilityValue = 0.f);

	float DamageDurability(float DamageAmount = 0.f);

	float RepairDurability(float RepairAmount = 0.f);

	UFUNCTION(BlueprintPure)
	float GetDurability() const;

	int GetMaxStackAllowance() const;

	void ActivateSlot();

	bool ContainsItem(const UItemDataAsset* DataAsset, const FItemStatics& ItemStatics = FItemStatics()) const;

	bool ContainsItem(const FName& ItemName, const FItemStatics& ItemStatics = FItemStatics()) const;

	float GetCarryWeight() const;

	// Sets the item that is in this slot
	int AddItem(const FName& ItemName, int NewQuantity = 1);

	int AddItem(const FItemStatics& ItemStatics, int NewQuantity = 1);

	bool SetItem(const FItemStatics& ItemStatics, int NewQuantity = 1);

	// Makes a 1-for-1 copy of the item in an existing inventory slot
	void CopyItemFromSlot(const UInventorySlot* SlotReference, int NewQuantity = 0);

	bool ContainsValidItem() const;

	void ResetAndEmptySlot();

	bool IsEmpty() const;

	bool IsFull() const;

	const UInventoryComponent* GetParentInventory() const { return ParentInventory_; }

	const FGameplayTagContainer& GetSlotTags() const
	{
		return SlotTags_;
	}

	bool ContainsTag(const FGameplayTag& SearchTag) const;

	bool ContainsTag(const FGameplayTagContainer& SearchTags) const;

private:

	UPROPERTY()
	const UInventoryComponent* ParentInventory_ = nullptr;

	UFUNCTION()
	void AssetLoadedDelegate(); // Called when asset loads

	UPROPERTY(ReplicatedUsing=OnRep_AssetId)
	FPrimaryAssetId AssetId_;

	UPROPERTY(ReplicatedUsing=OnRep_ItemUpdated)
	FItemStatics ItemStatics_;

	UPROPERTY(ReplicatedUsing=OnRep_QuantityUpdated)
	int Quantity_ = 0;

	// Set by OnRep_AssetId. If pointer is valid, slot contains a valid item.
	UPROPERTY()
	const UItemDataAsset* DataAsset_ = nullptr;

	UPROPERTY(Replicated)
	int SlotNumber_ = 0;

	FGameplayTagContainer SlotTags_;
};
