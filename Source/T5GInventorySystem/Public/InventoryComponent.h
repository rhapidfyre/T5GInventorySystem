
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "lib/InventorySlot.h"
#include "GameFramework/SaveGame.h"

#include "InventoryComponent.generated.h"


// Blueprints can only subscribe to dynamic delegates

/* Delegate that is called whenever a new notification is added to the item notification array.
 * Tells the client that there is an item to be processed.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNotificationAvailable);

/* Delegate that is called on both client and server when an inventory update has occurred
 * Used so that other blueprints/classes can bind to it, running their own function when the inventory updates.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryUpdated, int, slotNumber);

/* Delegate that is called whenever an item is activated. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemActivated, FName, itemName, int, QuantityConsumed);

/* Delegate that is called whenever the inventory is 'opened' or 'closed'. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryInUse,
		const AActor*, InUseActor, const bool, IsInUse);



UCLASS(BlueprintType, Blueprintable, ClassGroup = (InventorySystem), meta = (BlueprintSpawnableComponent))
class T5GINVENTORYSYSTEM_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
	
public:	//functions
	
	/**
	 * CONSTRUCTOR(S)
	 */
    UInventoryComponent();
	

    /**
     * EVENTS / DELEGATES
     */
    UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnInventoryUpdated OnInventoryUpdated;
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnNotificationAvailable OnNotificationAvailable;
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnItemActivated OnItemActivated;
	
	TMulticastDelegate<void(bool)> OnInventorySaved;
	TMulticastDelegate<void(bool)> OnInventoryRestored;
	
	/**
	 * ACCESSORS, MUTATORS & HELPERS
	 */

	bool HasAuthority() const; // Simple GetOwner()->HasAuthority() Check

	void ReinitializeInventory();

	UFUNCTION(BlueprintCallable)
	FString SaveInventory(FString& responseStr, bool isAsync = true);

	UFUNCTION(BlueprintPure)
	FString GetInventorySaveName() const;

	// If the restore boolean is set, this inventory has an associated save
	UFUNCTION(BlueprintPure)
	bool DoesInventorySaveExist() const { return bInventoryRestored; }
	
	UFUNCTION(BlueprintCallable)
	bool LoadInventory(
		FString& responseStr, FString SaveSlotName, bool isAsync = false);

	virtual void OnComponentCreated() override;
	
	UFUNCTION(BlueprintPure)
	int GetNumberOfTotalSlots() const;
	
	UFUNCTION(BlueprintPure)
	int GetNumberOfInventorySlots() const;
	
	UFUNCTION(BlueprintPure)
	int GetNumberOfEquipmentSlots() const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	FGameplayTag GetSlotInventoryTag(int slotNumber) const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Equipment Accessors")
	FGameplayTag GetSlotEquipmentTag(int slotNumber) const;
	
    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	int GetTotalQuantityByItem(const UItemDataAsset* ItemData) const;

    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	int GetFirstEmptySlotNumber() const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	float GetTotalWeightOfInventorySlots() const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	float GetTotalWeightOfEquipmentSlots() const;

    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	float GetWeightOfItemsInSlot(int slotNumber) const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	TArray<int> GetInventorySlotNumbersContainingItem(const UItemDataAsset* ItemData) const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	TArray<int> GetEquipmentSlotNumbersContainingItem(const UItemDataAsset* ItemData) const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	TArray<FStInventorySlot> GetCopyOfAllSlots() const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	TArray<FStInventorySlot> GetCopyOfAllInventorySlots() const;
	
	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Equipment Accessors")
	TArray<FStInventorySlot> GetCopyOfAllEquipmentSlots() const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	int GetSlotNumberByInventoryTag(const FGameplayTag& InventoryTag) const;

    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Equipment Accessors")
	int GetSlotNumberByEquipmentTag(const FGameplayTag& EquipmentTag) const;
    
	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Slot Validation")
	bool IsValidSlotNumber(int slotNumber) const;
    
	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Slot Validation")
	bool IsValidInventorySlot(int slotNumber) const;
    
	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Slot Validation")
	bool IsValidEquipmentSlot(int slotNumber) const;
    
	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Slot Validation")
	bool IsValidSlotByEquipmentTag(const FGameplayTag& EquipmentTag) const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Slot Validation")
	bool IsSlotNumberEmpty(int slotNumber) const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Slot Validation")
	bool IsSlotEmptyByTag(const FGameplayTag& EquipmentTag) const;

    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	FStItemData GetCopyOfItemInSlot(int slotNumber) const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	int GetQuantityOfItemsInSlotNumber(int slotNumber) const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Equipment Accessors")
	int GetQuantityOfItemsInSlotByTag(const FGameplayTag& EquipmentTag) const;
    
    UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	bool GetIsInventorySystemReady() const { return bInventoryReady; }
	
	UFUNCTION(BlueprintCallable, Category = "Item Management")
	int AddItemFromExistingSlot(
		const FStInventorySlot& NewItem, int OrderQuantity, int SlotNumber = -1,
		bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

    UFUNCTION(BlueprintCallable, Category = "Item Management")
	int AddItemFromDataAsset(
		const UItemDataAsset* ItemData, int OrderQuantity = 1, int SlotNumber = -1,
		bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	FStInventorySlot GetCopyOfSlotNumber(int slotNumber) const;

	// Execute on the inventory losing the item
    UFUNCTION(BlueprintCallable, Category = "Equipment Mutators")
	bool DonDoffEquipment(UInventoryComponent* TargetInventory,
		const int OriginSlotNumber, const int TargetSlotNumber = -1);

	// Execute on the inventory losing the item
    UFUNCTION(BlueprintCallable, Category = "Inventory Mutators")
	int RemoveItemByQuantity(
		const UItemDataAsset* ItemData, int OrderQuantity = 1,
		bool dropToGround = false, bool removeAll = false);

	// Execute on the inventory losing the item
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	int RemoveItemFromSlot(
		int SlotNumber, int OrderQuantity = 1,
		bool showNotify = false, bool dropToGround = false, bool removeAll = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory Getters")
	TArray<FStInventoryNotify> GetNotifications();
	
	UFUNCTION(BlueprintCallable) void IssueStartingItems();
	
	UFUNCTION(BlueprintPure)
	bool GetCanPickUpItems() const { return bCanPickUpItems; }
	
	UFUNCTION(BlueprintPure)
	bool GetIsWithdrawOnly() const { return bWithdrawOnly; }

	UFUNCTION(BlueprintCallable)
	bool SetInventoryDebugMode(const bool bEnable = true) { return (bShowDebug = bEnable); };
	
protected:

	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	int SwapOrStackSlots(
		FStInventorySlot& OriginSlot, FStInventorySlot& TargetSlot,
		int OrderQuantity = 1, bool showNotify = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	bool SplitStack(
		FStInventorySlot& OriginSlot, int TargetSlotNumber = 0, bool bNotify = false);

	UFUNCTION(Server, Reliable)
	void Server_RequestItemActivation(
		UInventoryComponent* OriginInventory, int SlotNumber = 0);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_TransferItems(
		UInventoryComponent* OriginInventory, UInventoryComponent* TargetInventory,
		int OriginSlotNumber, int TargetSlotNumber, int OrderQuantity = 1);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_DropItemOnGround(
		UInventoryComponent* OriginInventory, int SlotNumber, int OrderQuantity = 1);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_RequestOtherInventory(UInventoryComponent* TargetInventory);

	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_RestoreSavedInventory(
		const TArray<FStInventorySlot>& RestoredInventory);
	
	void RestoreInventory(
		const TArray<FStInventorySlot>& RestoredInventory);
	
private: //functions
    
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory References")
	FStInventorySlot& GetSlotReference(int SlotNumber);

	void Helper_SaveInventory(USaveGame*& SaveData) const;

	UFUNCTION(BlueprintCallable, BlueprintCallable, Category = "Inventory Mutators")
	int IncreaseSlotQuantity(FStInventorySlot& InventorySlot, int OrderQuantity = 1, bool bNotify = false);
	
	UFUNCTION(BlueprintCallable, BlueprintCallable, Category = "Inventory Mutators")
    int DecreaseSlotQuantity(FStInventorySlot& InventorySlot, int OrderQuantity = 1, bool bNotify = false);
	
	UFUNCTION(BlueprintCallable, BlueprintCallable, Category = "Slot Mutators")
	void SendNotification(const UItemDataAsset* ItemData, int OrderQuantity = 1);

	UFUNCTION(BlueprintCallable) void ResetSlot(FStInventorySlot& InventorySlot);

	UFUNCTION(BlueprintCallable)	
    bool TransferItemBetweenSlots(
        FStInventorySlot& OriginSlot, FStInventorySlot& TargetSlot,
        int OrderQuantity = 1, bool bDropOverflow = false);

	UFUNCTION(BlueprintCallable)
    bool ActivateItemInSlot(FStInventorySlot& InventorySlot, bool forceConsume = false);
	
	UFUNCTION()
	void LoadDataDelegate(const FString& SaveSlotName, int32 UserIndex, USaveGame* SaveData);

	UFUNCTION()
	void SaveInventoryDelegate(const FString& SaveSlotName, int32 UserIndex, USaveGame* SaveData);
	
public: //variables

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UInventoryDataAsset* InventoryDataAsset = nullptr; 

	// If true, saves will only work on the server.
	// If false, saves will work on the client and server, then send the save data to the server.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Settings")
	bool bSavesOnServer = false;

protected: //variables

	UPROPERTY(ReplicatedUsing = OnRep_NewNotification)
	TArray<FStInventoryNotify> Notifications_;

	// Local map used to remember which equipment slots are bound to which slots
	TMap<FGameplayTag, int> TagsMappedToSlots_;
	
	bool bCanPickUpItems = true;
	
	bool bWithdrawOnly = false;

	FString SaveSlotName_ = "";
	
	int32 SaveUserIndex_ = 0;
	
private: //variables

	UFUNCTION(NetMulticast, Reliable)
	void OnRep_InventorySlotUpdated(const TArray<FStInventorySlot>& OldSlot);
	
	UFUNCTION(NetMulticast, Reliable)
	void OnRep_NewNotification();

	UPROPERTY(ReplicatedUsing = OnRep_InventorySlotUpdated)
	TArray<FStInventorySlot> InventorySlots_;
	
	bool bInventoryReady = false;

	// Used to prevent clients from cheating their inventory
	bool bInventoryRestored = false;

	bool bShowDebug = false;
};
