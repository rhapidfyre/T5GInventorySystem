// Fill out your copyright notice in the Description page of Project Settings.

// ReSharper disable CppUE4CodingStandardNamingViolationWarning
// ReSharper disable CppUEBlueprintCallableFunctionUnused
// ReSharper disable CppUEBlueprintCallableFunctionUnused
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "lib/ItemData.h"
#include "lib/EquipmentData.h"
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

/* Delegate that is called on both client and server when an equipment update has occurred
 * Used so that other blueprints/classes can bind to it, rudddwnning their own function when the equipment slot updates.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentUpdated, EEquipmentSlotType, SlotEnum);

/* Delegate that is called whenever an item is activated. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemActivated, FName, itemName);



UCLASS(BlueprintType, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class T5GINVENTORYSYSTEM_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
	
protected:
    virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
public:	//functions

    UInventoryComponent();

	TMulticastDelegate<void(bool)> OnInventorySaved;
	TMulticastDelegate<void(bool)> OnInventoryRestored;

	void ReinitializeInventory();

	UFUNCTION(BlueprintCallable)
	FString SaveInventory(FString& responseStr, bool isAsync = false);
	
	UFUNCTION(BlueprintCallable)
	bool LoadInventory(FString& responseStr, FString SaveSlotName, bool isAsync = false);

	virtual void OnComponentCreated() override;

	// Event Dispatchers/Delegates
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnInventoryUpdated OnInventoryUpdated;
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnEquipmentUpdated OnEquipmentUpdated;
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnNotificationAvailable OnNotificationAvailable;
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnItemActivated OnItemActivated;
	
	/**
	 * Returns the size of the inventory, 1-indexed
	 * @return int - The number of inventory slots.
	 */
    int getNumInventorySlots() const { return m_inventorySlots.Num(); }

    /**
     * Returns the size of the equipment inventory, 1-indexed
     * @return int - The number of equipment slots.
     */
    int getNumEquipmentSlots() const { return m_equipmentSlots.Num(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	FStItemData getItemInSlot(int slotNumber, bool isEquipment = false);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment Accessors")
	EEquipmentSlotType getEquipmentSlotType(int slotNumber);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	EInventorySlotType getInventorySlotType(int slotNumber);
	
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	int getTotalQuantityInAllSlots(FName itemName);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	int GetFirstEmptySlot();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	float getTotalWeight(bool incEquip = false);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	float getWeightInSlot(int slotNumber);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	TArray<int> getSlotsContainingItem(FName itemName);

	/**
	 * Returns a TArray of FStInventorySlots containing a copy of all
	 * inventory slots in the order they were built. This function should only be used
	 * for testing, as this will use a large chunk of data. Result should never be networked.
	 * @return TArray of FStInventorySlots copies
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	TArray<FStInventorySlot> getInventorySlots() { return m_inventorySlots; };

	/**
	 * Returns a TArray of FStEquipmentSlots containing a copy of all
	 * inventory slots in the order they were built. This function should only be used
	 * for testing, as this will use a large chunk of data. Result should never be networked.
	 * @return TArray of FStEquipmentSlots copies
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment Accessors")
	TArray<FStInventorySlot> getEquipmentSlots() { return m_equipmentSlots; };

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	int getEquipmentSlotNumber(EEquipmentSlotType equipEnum);
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	FStInventorySlot getEquipmentSlot(EEquipmentSlotType equipEnum);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	FStInventorySlot getEquipmentSlotByNumber(int slotNumber);
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	bool IsValidSlot(int slotNumber, bool IsEquipmentSlot = false);
    
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	bool isValidEquipmentSlot(int slotNumber);
    
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	bool isValidEquipmentSlotEnum(EEquipmentSlotType equipSlot);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	bool isEmptySlot(int slotNumber, bool isEquipment = false);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	bool isEquipmentSlotEmpty(EEquipmentSlotType equipSlot);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	FName getItemNameInSlot(int slotNumber, bool isEquipment = false);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	int getQuantityInSlot(int slotNumber, bool isEquipment = false);
    
    /**
    * Returns true if the inventory system has fully initialized and is ready to be operated on.
    */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	bool isInventoryReady() { return bIsInventoryReady; }
	
	UFUNCTION(BlueprintCallable, Category = "Item Management")
	int AddItemFromExistingSlot(const FStInventorySlot& NewItem, int quantity, int SlotNumber = -1,
	bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

    UFUNCTION(BlueprintCallable, Category = "Item Management")
	int AddItemFromDataTable(FName ItemName, int quantity, int SlotNumber = -1,
	bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	FStInventorySlot GetCopyOfSlot(int slotNumber, bool IsEquipment = false);

    UFUNCTION(BlueprintCallable, Category = "Equipment Setters")
	bool donEquipment(UInventoryComponent* fromInventory, int fromSlot, EEquipmentSlotType toSlot = EEquipmentSlotType::NONE);
    
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	bool doffEquipment(EEquipmentSlotType equipSlot, int slotNumber = -1,
			UInventoryComponent* toInventory = nullptr, bool destroyResult = false);
	
    UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	int removeItemByQuantity(FName itemName, int quantity = 1, bool isEquipment = false, bool dropToGround = false, bool removeAll = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	int removeItemFromSlot(int slotNumber = 0, int quantity = 1, bool isEquipment = false, bool showNotify = false, bool dropToGround = false, bool removeAll = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	bool swapOrStackSlots(UInventoryComponent* fromInventory,
		int fromSlotNum = 0, int toSlotNum = 0, int quantity = 1,
		bool showNotify = false, bool isEquipmentSlot = false, bool fromEquipmentSlot = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	bool swapOrStackWithRemainder(UInventoryComponent* fromInventory,
		int fromSlotNum , int toSlotNum, int quantity, int& remainder,
		bool showNotify = false, bool isEquipmentSlot = false, bool fromEquipmentSlot = false);

    UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	bool splitStack(UInventoryComponent* fromInventory, int fromSlotNum = 0,
        	int splitQuantity = 1, int toSlotNum = 0, bool showNotify = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory Getters")
	TArray<FStInventoryNotify> getNotifications();

	UFUNCTION(BlueprintPure)
	TArray<FStInventorySlot> CopyInventorySlots() const { return m_inventorySlots; }
	
	UFUNCTION(BlueprintPure)
	TArray<FStInventorySlot> CopyEquipmentSlots() const { return m_equipmentSlots; }
	
private: //functions
	
	void RestoreInventory(
		const TArray<FStInventorySlot>& RestoredInventory,
		const TArray<FStInventorySlot>& RestoredEquipment);
    
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
		FStInventorySlot& GetInventorySlot(int slotNumber, bool IsEquipment = false);

	void Helper_SaveInventory(USaveGame*& SaveData) const;

	int increaseQuantityInSlot(int slotNumber, int quantity, bool showNotify = false);
	
    int increaseQuantityInSlot(int slotNumber, int quantity, int& newQuantity, bool showNotify = false);

	int decreaseQuantityInSlot(int slotNumber, int quantity, bool isEquipment = false, bool showNotify = false);

	int decreaseQuantityInSlot(int slotNumber, int quantity, int& remainder, bool isEquipment = false, bool showNotify = false);
	
	UFUNCTION(Client, Unreliable)
	void sendNotification(FName itemName, int quantity, bool wasAdded = true);

    void resetInventorySlot(int slotNumber = 0);
	
    bool transferItemBetweenSlots(
        UInventoryComponent* fromInventory, UInventoryComponent* toInventory,
        int fromSlotNum, int toSlotNum, int moveQuantity = 1, bool overflowDrop = false, bool isFromEquipSlot = false, bool isToEquipSlot = false);

	UFUNCTION(BlueprintCallable)
    bool activateItemInSlot(int slotNumber, bool isEquipment = false, bool consumeItem = false);

	bool setNotifyItem(FName itemName, int quantityAffected);
	
	UFUNCTION()
	void LoadDataDelegate(const FString& SaveSlotName, int32 UserIndex, USaveGame* SaveData);

	UFUNCTION()
	void SaveInventoryDelegate(const FString& SaveSlotName, int32 UserIndex, USaveGame* SaveData);
	
public: //variables

	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPickupItems = true;
	
	// If true, "LogTemp" will display information regarding failures and more. Is true if Verbose is true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bShowDebug = false;
	
	// If true, "LogTemp" will display basically everything the inventory does.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bVerboseOutput = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Settings")
	bool bShowNotifications = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Settings")
	int NumberOfInvSlots = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Settings")
	TArray<EEquipmentSlotType> EligibleEquipmentSlots;

	// If true, saves will only work on the server.
	// If false, saves will work on the client and server, then send the save data to the server.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Settings")
	bool bSavesOnServerOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Settings")
	TArray<FStStartingItem> StartingItems;
	
	// If TRUE, players cannot put items in this inventory, only take from it
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool m_WithdrawOnly = false;

private: //variables

	UFUNCTION() void OnRep_InventorySlotUpdated(TArray<FStInventorySlot> OldInventory);
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_InventorySlotUpdated)
    TArray<FStInventorySlot> m_inventorySlots;
	
	UFUNCTION() void OnRep_EquipmentSlotUpdated(TArray<FStInventorySlot> OldEquipment);
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_EquipmentSlotUpdated)
    TArray<FStInventorySlot> m_equipmentSlots;

	// Client Only.
    // Contains items that haven't been processed yet (notifications)
	UPROPERTY() TArray<FStInventoryNotify> m_notifications;

    bool bIsInventoryReady = false;

	// Used to prevent clients from cheating their inventory
	bool bInventoryRestored = false;

	// Basic Empty Slot for Referencing
	UPROPERTY() FStInventorySlot m_EmptySlot = FStInventorySlot();

	FString InventorySaveSlotName_ = "";
	int32 InventorySaveUserIndex = 0;

public:

	UFUNCTION(Server, Reliable)
	void Server_RequestItemActivation(UInventoryComponent* inventoryUsed,
		int slotNumber = 0, bool isEquipment = false);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_TransferItems(UInventoryComponent* fromInventory, UInventoryComponent* toInventory, int fromSlot, int toSlot, int moveQty, bool isFromEquipSlot = false, bool isToEquipSlot = false);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_DropItemOnGround(UInventoryComponent* fromInventory, int fromSlot, int quantity, bool isFromEquipSlot);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_RequestOtherInventory(UInventoryComponent* targetInventory);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_StopUseOtherInventory(UInventoryComponent* otherInventory);

	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_InitSavedInventory(const TArray<FStStartingItem>& StartingItemsList);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_RestoreSavedInventory(
		const TArray<FStInventorySlot>& RestoredInventory,
		const TArray<FStInventorySlot>& RestoredEquipment);
	
	
};
