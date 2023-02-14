// Fill out your copyright notice in the Description page of Project Settings.

// ReSharper disable CppUE4CodingStandardNamingViolationWarning
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "lib/ItemData.h"
#include "lib/EquipmentData.h"
#include "lib/InventorySlot.h"

#include "InventoryComponent.generated.h"

// Blueprints can only subscribe to dynamic delegates

/* Delegate that is called on both client and server when an inventory update has occurred
 * Used so that other blueprints/classes can bind to it, running their own function when the inventory updates.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryUpdated, int, slotNumber, bool, isEquipment);
/* Delegate that is called whenever a new notification is added to the item notification array.
 * Tells the client that there is an item to be processed.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNotificationAvailable);

UCLASS(BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
	
protected:
    virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
public:	//functions

    UInventoryComponent();

	/*
	 * Runs the initialization after UE has created the UObject system.
	 * Sets up the stuff for the inventory that can't be done before the game is running.
	 */
	void InitializeInventory();

	virtual void OnComponentCreated() override;

	//virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Event Dispatchers/Delegates
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
		FOnInventoryUpdated OnInventoryUpdated;
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
		FOnNotificationAvailable OnNotificationAvailable;
	
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

    /**
     * Gets a copy of the entire slot data struct from the requested slotNumber
     * @param slotNumber An int of the inventory slot requested
     * @return A FStSlotContents object copied from the data of the found slot.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
        FStInventorySlot getInventorySlotData(int slotNumber);

	/**
	 * Returns the item found in the given slot.
	 * @param slotNumber An int representing the inventory slot requested
	 * @param isEquipment True if this should check for an EQUIPMENT slot. False by default.
	 * @return Returns a copy of the found FStItemData or an invalid item
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
		FStItemData getItemInSlot(int slotNumber, bool isEquipment = false);

	/**
	 * Returns the equipment enum slot type of the given slot number. Returns NONE
	 * if the slot is invalid or the slot is not an equipment slot.
	 * @param slotNumber The slot number for the equipment slot inquiry
	 * @return The equip slot type enum, where NONE indicates failure.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment Accessors")
		EEquipmentSlotType getEquipmentSlotType(int slotNumber);
	
	/**
	 * Returns the inventory enum slot type of the given slot number. Returns NONE
	 * if the slot is invalid or the slot is not an inventory slot.
	 * @param slotNumber The slot number for the inventory slot inquiry
	 * @return The inventory slot type enum, where NONE indicates failure.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
		EInventorySlotType getInventorySlotType(int slotNumber);
	
    /**
     * Returns the total quantity of an item in the entire inventory, NOT including equipment slots.
     * @param itemName The itemName of the item we're searching for.
     * @return The total quantity of items found. Negative value indicates failure.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
        int getTotalQuantityInAllSlots(FName itemName);

    /**
     * Returns the slotNumber of the first empty slot found in the inventory.
     * @return The slotNumber of the first empty slot. Negative indicates full inventory.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
        int getFirstEmptySlot();

    /**
     * Returns the total weight of all items in the inventory.
     * @param incEquip If true, the total weight will also include all equipment slots. False by default.
     * @return Float containing the total weight of the inventory. Unitless.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
        float getTotalWeight(bool incEquip = false);

    /**
     * Gets the total weight of all items in the given slot.
     * @param slotNumber The int representing the inventory slot to check.
     * @return Float with the total weight of the given slot. Negative indicates failure. Zero indicates empty slot.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
        float getWeightInSlot(int slotNumber);

    /**
     * Returns a TArray of int ints where the item was found
     * @param itemName The item we're looking for
     * @return TArray of ints. Empty Array indicates no items found.
     */
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

    /**
     * Returns a copy of the slot data for the slot requested.
     * @param slotNumber An int representing the inventory slot
     * @return FStSlotContents of the found slot. Returns default struct if slot is empty or not found.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
        FStInventorySlot getInventorySlot(int slotNumber);

    /**
     * Gets the index (slot) of the equipment slot where equipment type was found.
     * @param equipEnum Enum of the slotType to find.
     * @return Int representing the slot number found. Negative indicates failure.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
        int getEquipmentSlotNumber(EEquipmentSlotType equipEnum);
    
    /**
     * Gets a copy of the equipment slot for the requested equipment type. On failure,
     * it will return a default FStEquipmentSlot struct. Check 'slotType == NONE' for validity.
     * 
     * @param equipEnum Enum of the slotType to find.
     * @return A copy of an FStEquipmentSlot struct that was found.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
        FStInventorySlot getEquipmentSlot(EEquipmentSlotType equipEnum);

    /**
     * Override which uses the direct slot number instead of the equip enum.
     * @param slotNumber Represents the slot of 'm_equipmentSlots' to be examined.
     * @return FStEquipmentSlot representing the slot found. Check 'FStEquipmentSlot.slotType = NONE' for failure
     */
    FStInventorySlot getEquipmentSlot(int slotNumber);
    
    /**
     * Returns the truth of whether the requested slot is a real inventory slot or not.
     * @param slotNumber An int int of the slot being requested
     * @return Boolean
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
		bool isValidInventorySlot(int slotNumber);
    
	/**
	 * Returns the truth of whether the requested slot is a real equipment slot or not.
	 * @param slotNumber The slot being requested
	 * @return Boolean
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
		bool isValidEquipmentSlot(int slotNumber);
    
	/**
	 * (overload) Returns the truth of whether the requested slot is a real equipment slot or not.
	 * Not preferred, as the int version of this function is O(1)
	 * @param equipSlot The equip slot enum to look for ( O(n) lookup )
	 * @return Truth of outcome
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
		bool isValidEquipmentSlotEnum(EEquipmentSlotType equipSlot);

	/**
	 * Returns the truth of whether the requested slot is empty.
	 * Performs isValidInventorySlot() internally. No need to call both functions.
	 * @param slotNumber The slot number to check.
	 * @param isEquipment If true, checks equipment slots. If false, checks inventory slots.
	 * @return If true, the slot is vacant.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
		bool isEmptySlot(int slotNumber, bool isEquipment = false);
	
	/**
	 * Returns the truth of whether the requested slot is empty.
	 * Performs isValidInventorySlot() internally. No need to call both functions.
	 * @param equipSlot The Equip enum for the slot we're checking.
	 * @return True if the equipment slot is unoccupied.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
		bool isEquipmentSlotEmpty(EEquipmentSlotType equipSlot);

    /**
     * Gets the name of the item that resides in the requested inventory slot.
     * @param slotNumber an int int representing the slot number requested
     * @param isEquipment True if looking for an equipment slot
     * @return FName the itemName of the item in slot. Returns 'invalidName' if empty or nonexistent.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
        FName getItemNameInSlot(int slotNumber, bool isEquipment = false);

    /**
     * Retrieves the quantity of the item in the given slot.
     * @param slotNumber The int int representing the slot number being requested
     * @param isEquipment True if the slot is an equipment slot.
     * @return Int of the slot number. Negative indicates failure.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
        int getQuantityInSlot(int slotNumber, bool isEquipment = false);
    
    /**
    * Returns true if the inventory system has fully initialized and is ready to be operated on.
    */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
        bool isInventoryReady() { return m_bIsInventoryReady; }

    /**
    * Performs a data table lookup for the item name provided, adding a completely new item
    * to the inventory. Once the item has been added, you can then use 'getItemInInventorySlot(return)'
    * to operate on the item directly (such as changing durability, etc).
    *
    * @param itemName item we are trying to add.
    * @param quantity How many to add. Defaults to 1.
    * @param overflowAdd If true, any items remaining after add will attempt to add to the next empty slot.
    * @param overflowDrop If true, the item will fall to the floor if it can't be added. True by default.
    * @param showNotify If true, add will show a notification. False by default.
    * @return The number of items actually added
    */
    UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
        int addItemByName(FName itemName, int quantity = 1,
        bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

	/**
	* Finds the item at the given location in the origination inventory, copies
	* it to an entirely new item. The item will be inserted at the first available slot,
	* either stacking with an identical item or being added to a new empty slot.
	* 
	* @param fromInventory The inventory that is donating the item.
	* @param fromSlot The slot where the item reference can be found.
	* @param quantity How many to add. Defaults to 1.
	* @param overflowAdd If true, remaining items after add will attempt to be added to the next empty slot.
	* @param overflowDrop If true, the item will fall to the floor if it can't be added. True by default.
	* @param showNotify If true, add will show a notification. False by default.
   * @return Slot number the item was added to. Negative value indicates failure.
	*/
	int addItemFromExisting(UInventoryComponent* fromInventory, int fromSlot, int quantity,
	bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

	/**
	* (overload) Finds the item at the given location in the origination inventory, copies
	* it to an entirely new item. The item will be inserted at the first available slot,
	* either stacking with an identical item or being added to a new empty slot.
	* 
	* @param fromInventory The inventory that is donating the item.
	* @param fromSlot The slot where the item reference can be found.
	* @param quantity How many to add. Defaults to 1.
	* @param remainder How many items could not be added.
	* @param overflowAdd If true, remaining items after add will attempt to be added to the next empty slot.
	* @param overflowDrop If true, the item will fall to the floor if it can't be added. True by default.
	* @param showNotify If true, add will show a notification. False by default.
   * @return Slot number the item was added to. Negative value indicates failure.
	*/
	int addItemFromExistingWithRemainder(UInventoryComponent* fromInventory, int fromSlot, int quantity, int& remainder,
		bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

	/**
	* @param newItem Pointer to the item being added. Must be a new object, not a const pointer.
	* @param quantity How many to add. Defaults to 1.
	* @param overflowAdd If true, any items that don't stack will be added to the next empty slot.
	* @param overflowDrop If true, any remaining quantity that does not fit into the inventory will fall to the ground.
	* @param showNotify If true, add will show a notification. False by default.
   * @return Slot number the item was added to. Negative value indicates failure.
	*/
	int addItemFromExisting(FStItemData newItem, int quantity,
	bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

	/**
	 * Acquires the existing item, creates a copy of it, and passes the new item object
	 * to the overload version of this function for addition to the inventory.
	 *
	 * @param fromInventory The inventory donating the item.
	 * @param fromSlot Which slot the item is coming from.
	 * @param toSlot Which slot to add the item to. -1 means to find the first empty slot.
	 * @param quantity How many to add. Defaults to 1.
	 * @param overflowAdd If true, any items that don't stack will be added to the next empty slot.
	 * @param overflowDrop If true, any remaining quantity that does not fit into the inventory will fall to the ground.
	 * @param showNotify If true, will show a notification. False by default.
	 * @return An integer representing how many items failed to add. Negative indicates failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
		int addItemFromExistingToSlot(UInventoryComponent* fromInventory,
				int fromSlot, int toSlot, int quantity = 1,
				bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

	/**
	 * (overload) Acquires the existing item, creates a copy of it, and passes the new item object
	 * to the overload version of this function for addition to the inventory.
	 *
	 * @param existingItem The existing item to add
	 * @param toSlot Which slot to add the item to. -1 means to find the first empty slot.
	 * @param quantity How many to add. Defaults to 1.
	 * @param overflowAdd If true, any items that don't stack will be added to the next empty slot.
	 * @param overflowDrop If true, any remaining quantity that does not fit into the inventory will fall to the ground.
	 * @param showNotify If true, will show a notification. False by default.
	 * @return An integer representing how many items failed to add. Negative indicates failure.
	 */
		int addItemFromExistingToSlot(FStItemData existingItem, int toSlot, int quantity = 1,
				bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

	/**
	 * (overload) Acquires the existing item, creates a copy of it, and passes the new item object
	 * to the overload version of this function for addition to the inventory.
	 *
	 * @param fromInventory The inventory donating the item.
	 * @param fromSlot Which slot the item is coming from.
	 * @param toSlot Which slot to add the item to. -1 means to find the first empty slot.
	 * @param quantity How many to add. Defaults to 1.
	 * @param remainder How many items could not be added.
	 * @param overflowAdd If true, any items that don't stack will be added to the next empty slot.
	 * @param overflowDrop If true, any remaining quantity that does not fit into the inventory will fall to the ground.
	 * @param showNotify If true, will show a notification. False by default.
	 * @return An integer representing how many items failed to add. Negative indicates failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
		int addItemFromExistingToSlotWithRemainder(UInventoryComponent* fromInventory,
				int fromSlot, int toSlot, int quantity, int& remainder,
				bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

	/**
	 * (overload) Adds a pre-existing item into the given slot. If the slot given is negative,
	 * it will add the item to the first empty slot it finds. Fails if the slot has a different item in it.
	 * If the given slot already has an identical item, it will stack them, accounting for
	 * durability, crafting differences, etc.
	 *
	 * @param newItem The new item to be added. New item, not a const pointer.
	 * @param toSlot Which slot to add the item to. -1 means to find the first empty slot.
	 * @param quantity How many to add. Defaults to 1.
	 * @param remainder How many items could not be added.
	 * @param overflowAdd If true, any items that don't stack will be added to the next empty slot.
	 * @param overflowDrop If true, any remaining quantity that does not fit into the inventory will fall to the ground.
	 * @param showNotify If true, will show a notification. False by default.
	 * @return An integer representing how many items were actually added. Values less than 1 indicate failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
		int addItemFinalWithRemainder(FStItemData newItem, int toSlot, int quantity, int& remainder,
		bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

	/**
	 * (overload) Adds a pre-existing item into the given slot. If the slot given is negative,
	 * it will add the item to the first empty slot it finds. Fails if the slot has a different item in it.
	 * If the given slot already has an identical item, it will stack them, accounting for
	 * durability, crafting differences, etc.
	 *
	 * @param newItem The new item to be added. New item, not a const pointer.
	 * @param toSlot Which slot to add the item to. -1 means to find the first empty slot.
	 * @param quantity How many to add. Defaults to 1.
	 * @param overflowAdd If true, any items that don't stack will be added to the next empty slot.
	 * @param overflowDrop If true, any remaining quantity that does not fit into the inventory will fall to the ground.
	 * @param showNotify If true, will show a notification. False by default.
	 * @return An integer representing how many items were actually added. Values less than 1 indicate failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
		int addItemFinal(FStItemData newItem, int toSlot, int quantity = 1,
				bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

    /**
    * Adds a brand new item to the inventory from nonexistence. If slot number is
    * negative, it will add the item to the first empty slot available. If an
    * item exists in this slot already, it will stack the item if it's exactly identical,
    * accounting for durability and crafting uniqueness.
    *
    * @param itemName The FName of the item we are trying to add.
    * @param slotNumber Which slot to add the item to. Negative = add to first empty slot.
	* @param quantity How many to add. Defaults to 1.
	* @param overflowAdd If true, any items that don't stack will be added to the next empty slot.
	* @param overflowDrop If true, any remaining quantity that does not fit into the inventory will fall to the ground.
    * @param showNotify If true, will show a notification. False by default.
    * @return Integer representing how many items were actually added. Negative indicates failure to add altogether.
    */
    UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
        int addItemByNameToSlot(FName itemName, int slotNumber = 0, int quantity = 1,
			bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

    /**
     * Attempts to read the item from the provided slot, and if it is a valid
     * target for equipping, it will don the equipment to the appropriate slot.
     *
     * @param fromInventory The inventory we are equipping the item from.
     * @param fromSlot The slot where the item reference can be found
     * @param toSlot (optional) If not NONE it will equip the item directly to this slot.
     * @return True if the item was added successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment Setters")
        bool donEquipment(UInventoryComponent* fromInventory, int fromSlot, EEquipmentSlotType toSlot = EEquipmentSlotType::NONE);
    
	/**
	* Unequip a piece of equipment. Provides an argument for destroying the item,
	* instead of returning it to the inventory (such as a sword being destroy). If
	* the item isn't destroyed, it gets moved to the requested slot.
	*
	* @param equipSlot The slot to remove the item from. 
	* @param slotNumber Which slot to put the doffed equipment into.
	* @param destroyResult If true, the item will be destroyed. If false, moved into the inventory.
	* @param toInventory (optional) The inventory to doff the equipment into.
	* @return True if the operation succeeded with the given parameters
	*/
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
		bool doffEquipment(EEquipmentSlotType equipSlot, int slotNumber = -1,
			UInventoryComponent* toInventory = nullptr, bool destroyResult = false);
	
    /**
    * Starting from the first slot where the item is found, removes the given quantity of
    * the given itemName until all of the item has been removed or the removal quantity
    * has been reached (inclusive).
    *
    * @param itemName The name of the item we are wanting to remove from the inventory
    * @param quantity How many to remove. Defaults to 1. If amount exceeds quantity, will set removeAll to true.
    * @param dropToGround If true, spawns a pickup for the item removed at the total quantity removed.
    * @param removeAll If true, removes ALL occurrence of the item in the entire inventory.
    * @return The total number of items successfully removed. Negative indicates failure.
    */
    UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
        int removeItemByQuantity(FName itemName, int quantity = 1, bool dropToGround = false, bool removeAll = false);

	/**
	* Removes a quantity of the item in the requested inventory slot. Verifications should
	* be performed before running this function. This function will remove whatever is in the slot
	* with the given parameters. It does not perform name/type verification.
	*
	* @param slotNumber Which slot we are taking an item away from
	* @param quantity How many to remove. Defaults to 1. If amount exceeds quantity, will set removeAll to true.
	* @param isEquipment True if the slot is an equipment slot.
	* @param showNotify True if the player should get a notification.
	* @param dropToGround If the remove is successful, the item will fall to the ground (spawns a pickup item)
	* @param removeAll If true, removes all of the item in the slot, making the slot empty.
	* @return The number of items actually removed. Negative indicates failure.
	*/
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	int removeItemFromSlot(int slotNumber = 0, int quantity = 1, bool isEquipment = false, bool showNotify = false, bool dropToGround = false, bool removeAll = false);

	/**
	* Performs a swap where the destination slot is another slot. If the items are the same, it will
	* stack items FROM the original spot on top of the items in the TO slot. For example, both slots
	* contain carrots. Two different FStItemData, but they are effectively the same item. These will stack instead of swap.
	* 
	* @param fromInventory A pointer to the UInventoryComponent* the item is being moved FROM. Nullptr uses this inventory.
	* @param fromSlotNum The slot number of the FROM inventory
	* @param toSlotNum The slot number of this inventory to swap with or stack into.
	* @param quantity The quantity to move. Values less tan 1 signify to move the entire slot's stack.
	* @param showNotify True if a notification should be shown to both inventory owners.
	* @param isEquipmentSlot True if the TO slot is an EQUIPMENT slot. False means INVENTORY slot.
	* @param fromEquipmentSlot True if the FROM slot is an EQUIPMENT slot. False means INVENTORY slot.
	*/
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
		bool swapOrStackSlots(UInventoryComponent* fromInventory,
		int fromSlotNum = 0, int toSlotNum = 0, int quantity = 1,
		bool showNotify = false, bool isEquipmentSlot = false, bool fromEquipmentSlot = false);

	/**
	* (overload) Performs a swap-or-stack where the destination slot is another slot. If the items are the same, it will
	* stack items FROM the original spot on top of the items in the TO slot. For example, both slots
	* contain carrots. Two different FStItemData, but they are effectively the same item. These will stack instead of swap.
	* 
	* @param fromInventory A pointer to the UInventoryComponent* the item is being moved FROM. Nullptr uses this inventory.
	* @param fromSlotNum The slot number of the FROM inventory. Fails if invalid slot.
	* @param toSlotNum The slot number of this inventory to swap with or stack into. Defaults to -1.
	* @param quantity The quantity to move. Values less tan 1 signify to move the entire slot's stack.
	* @param remainder The amount remaining after swapping/stacking.
	* @param showNotify True if a notification should be shown to both inventory owners.
	* @param isEquipmentSlot True if the TO slot is an EQUIPMENT slot. False means INVENTORY slot.
	* @param fromEquipmentSlot True if the FROM slot is an EQUIPMENT slot. False means INVENTORY slot.
	*/
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
		bool swapOrStackWithRemainder(UInventoryComponent* fromInventory,
		int fromSlotNum , int toSlotNum, int quantity, int& remainder,
		bool showNotify = false, bool isEquipmentSlot = false, bool fromEquipmentSlot = false);

    /**
    * Starting from the first slot where the item is found, removes the given quantity of
    * the given FStItemData until all of the item has been removed or the removal quantity
    * has been reached (inclusive).
    *
    * @param fromInventory The inventory of the stack being split.
    * @param fromSlotNum The slot number of where the split is originating FROM
    * @param splitQuantity The amount of split off from the original stack.
    * @param toSlotNum The slot number of the destination slot. Negative means use first available slot found.
	* @param showNotify If true, shows a notification. False by default.
    * @return True if the split was successful. Returns false on failure
                            or if firstEmptySlot is true with no slots open
    */
    UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
        bool splitStack(UInventoryComponent* fromInventory, int fromSlotNum = 0,
        	int splitQuantity = 1, int toSlotNum = 0, bool showNotify = false);

	/**
	* CLIENT event that retrieves and subsequently removes all inventory notifications.
	* @return A TArray of FStInventoryNotify
	*/
	UFUNCTION(BlueprintCallable, Category = "Inventory Getters")
	TArray<FStInventoryNotify> getNotifications();
	
private: //functions
    
    /**
    * Creates an entirely new item that did not previously exist, by looking up
    * the given itemName in the items database. If found, it will construct
    * a new item, inserting it into the inventory at the first eligible location.
    * If a slot number is given, this function will return how many are left after stack limit reached.
    * @param itemName The FName of the item to be added.
    * @param quantity The amount of the item to be added.
    * @param showNotify Whether a notification should appear. False by default.
    * @param slotNumber The slot to add the item to. Negative chooses first empty slot. Returns slot item was added into.
    * @return The total number of items actually added. Negative indicates failure.
    */
    int addItem(FName itemName, int quantity, bool showNotify, int& slotNumber);
    
    /**
    * Creates a copy of the given pointer const itemData, and adds it to the inventory.
    * If a slot number is given, this function will return how many are left after stack limit reached.
    * @param newItem The new item to be added.
    * @param quantity The amount to be added to the slot.
    * @param showNotify Whether a notification should appear. False by default.
    * @param slotNumber The slot to insert at. Negative means first empty slot. Returns slot item was added into.
    * @return The total number of items actually added. Negative indicates failure.
    */
    int addItem(FStItemData newItem, int quantity, bool showNotify, int& slotNumber);
	
	/**
	* Increases the amount of an existing item by the quantity given.
	* If the given slot is empty, it will do nothing and fail.
	*
	* @param slotNumber The slot number to be increased
	* @param quantity The quantity to increase the slot by, up to the maximum quantity
	* @param showNotify Show a notification when quantity increases. False by default.
	* @return The number of items actually added. Negative indicates failure.
	*/
	int increaseQuantityInSlot(int slotNumber, int quantity, bool showNotify = false);
	
    /**
    * Increases the amount of an existing item by the quantity given.
    * If the given slot is empty, it will do nothing and fail.
    *
    * @param slotNumber The slot number to be increased
    * @param quantity The quantity to increase the slot by, up to the maximum quantity
    * @param newQuantity The new amount after increase (override)
    * @param showNotify Show a notification when quantity increases. False by default.
	* @return The number of items actually added. Negative indicates failure.
    */
    int increaseQuantityInSlot(int slotNumber, int quantity, int& newQuantity, bool showNotify = false);

	/**
	* Increases the amount of an existing item by the quantity given.
	* If the given slot is empty, it will do nothing and fail.
	*
	* @param slotNumber The slot number to be decreased
	* @param quantity The quantity to decrease the slot by
	* @param isEquipment True if the slot is an equipment slot
    * @param showNotify Show a notification when quantity increases. False by default.
	* @return The number of items actually removed. Negative indicates failure.
	*/
	int decreaseQuantityInSlot(int slotNumber, int quantity, bool isEquipment = false, bool showNotify = false);

	/**
	* Increases the amount of an existing item by the quantity given.
	* If the given slot is empty, it will do nothing and fail.
	*
	* @param slotNumber The slot number to be decreased
	* @param quantity The quantity to decrease the slot by
	* @param remainder The remaining amount after decrease (override)
	* @param isEquipment True if the slot is an equipment slot
    * @param showNotify Show a notification when quantity increases. False by default.
	* @return The number of items actually removed. Negative indicates failure.
	*/
	int decreaseQuantityInSlot(int slotNumber, int quantity, int& remainder, bool isEquipment = false, bool showNotify = false);
	
	/**
	* Sends a notification that an item was modified. Since the item may or may not exist
	* at the time the notification is created, we just use a simple FName and the class defaults.
	* 
	* @param itemName The FName of the affected item
	* @param quantity The quantity affected
	* @param wasAdded True if the item was added, False if the item was removed
	*/
	UFUNCTION(Client, Unreliable)
	void sendNotification(FName itemName, int quantity, bool wasAdded = true);

    /**
    * Wipes the inventory slot, making it empty and completely destroying anything previously in the slot.
    * Mostly used for cases where we know we want the slot to absolutely be empty in all cases.
    * @param slotNumber The slot number to reset
    */
    void resetInventorySlot(int slotNumber = 0);

    /**
    * Performs network replication and keeps the inventory updated.
    * Should be called anytime a value in the inventory is changed.
    *
    * @param slotNumber The slot that was affected
    * @param isEquipment True if the slot is an equipment slot. False by default.
    * @param isAtomic If true, sends the entire inventory. Should not be used except for testing.
    */
    void InventoryUpdate(int slotNumber = 0, bool isEquipment = false, bool isAtomic = false);

    /**
    * Called when the server is reporting that a PLAYER wants to transfer items between
    * two inventories. This is NOT a delegate/event. It should be invoked by the delegate
    * that intercepted the net event request. This function transfers two items between inventories.
    * Validation is required if you want to keep game integrity.
    *
    * @param fromInventory The inventory losing the item
    * @param toInventory The inventory gaining the item. Can be the same inventory.
    * @param fromSlotNum The slot of origination. Required to be zero or greater. Must be a real item.
    * @param toSlotNum The slot of the destination. Negative indicates to use the first available slot.
    * @param moveQuantity The amount to be moved. Will default to 1.
    * @param overflowDrop If true, anything that overflows will spawn a pickup actor.
    * @param isFromEquipSlot If true, signifies the FROM slot is an equipment slot.
    * @param isToEquipSlot If true, signifies the TO slot is an equipment slot.
    * @return True if at least 1 of the item was successfully moved. False if the transfer failed entirely.
    */
    bool transferItemBetweenSlots(
        UInventoryComponent* fromInventory, UInventoryComponent* toInventory,
        int fromSlotNum, int toSlotNum, int moveQuantity = 1, bool overflowDrop = false, bool isFromEquipSlot = false, bool isToEquipSlot = false);

    /**
    * Called when an item is activated from the inventory. This function is
    * executed from the inventory of the player who activated the item.
    *
    * @param slotNumber The number of the inventory slot containing the item to be activated
    * @return True if the item was successfully activated
    */
    bool activateItemInSlot(int slotNumber);

    /**
     * Adds the item to the notification TMap, notifying the player about a modification to their inventory.
     * @param itemData The item data that is being notified
     * @param quantityAffected The quantity that was affected (how many the player gained/lost)
     * @return True if the notification was successfully added to the queue.
     */
	bool setNotifyItem(FStItemData itemData, int quantityAffected);
	
	UFUNCTION()
	void OnRep_InventorySlotUpdated();
	
	UFUNCTION()
	void OnRep_EquipmentSlotUpdated();

public: //variables

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Actor Settings")
        bool bShowNotifications = true;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Actor Settings")
        int NumberOfInvSlots = 6;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Actor Settings")
		TArray<EEquipmentSlotType> EligibleEquipmentSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Initialization")
		TArray<FStInventorySlot> StartingItems;
	
	// If TRUE, players cannot put items in this inventory, only take from it
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool m_WithdrawOnly = false;

private: //variables

	UPROPERTY(Replicated, ReplicatedUsing = OnRep_InventorySlotUpdated)
    TArray<FStInventorySlot> m_inventorySlots;
	
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_EquipmentSlotUpdated)
    TArray<FStInventorySlot> m_equipmentSlots;

	// Client Only.
    // Contains items that haven't been processed yet (notifications)
	UPROPERTY()
	TArray<FStInventoryNotify> m_notifications;
	
    bool m_bIsInventoryReady = false;

	

	//-------------------------
	//-------------------------          REPLICATION
	//-------------------------

public:

	/**
	 * Sends a request to the server to transfer items between inventories.
	 * Requires Validation. Called by the client. Executes from the player's inventory.
	 * @param fromInventory The inventory of the FROM item (the 'origination')
	 * @param toInventory The inventory of the TO item (the 'receiving')
	 * @param fromSlot The slot number of the origination inventory being affected
	 * @param toSlot The slot number of the receiving inventory (-1 means first available slot)
	 * @param moveQty The amount to move. Negative means move everything.
	 * @param isFromEquipSlot If TRUE, the origination slot will be treated as an equipment slot. False means inventory.
	 * @param isToEquipSlot If TRUE, the receiving slot will be treated as an equipment slot. If false, inventory.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_TransferItems(UInventoryComponent* fromInventory, UInventoryComponent* toInventory, int fromSlot, int toSlot, int moveQty, bool isFromEquipSlot = false, bool isToEquipSlot = false);
	//void Server_TransferItems_Implementation(UInventoryComponent* fromInventory, UInventoryComponent* toInventory, int fromSlot, int toSlot, int moveQty, bool isFromEquipSlot = false, bool isToEquipSlot = false);

	/**
	 * Sends a request to the server to take the item from the FROM inventory, and throw it on the ground as a pickup.
	 * @param fromInventory The inventory of the FROM item (the 'origination')
	 * @param fromSlot The slot number of the origination inventory being affected
	 * @param quantity The amount to toss out. Negative means everything.
	 * @param isFromEquipSlot If TRUE, the origination slot will be treated as an equipment slot. False means inventory.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_DropItemOnGround(UInventoryComponent* fromInventory, int fromSlot, int quantity, bool isFromEquipSlot);
	
	/**
	 * Sends a request to the server when the player opens something like a storage container, and needs the
	 * information of the inventory for their UI. Called by the client.
	 * @requestInventory The inventory component the player wishes to load in
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_RequestOtherInventory(UInventoryComponent* targetInventory);
	
	/**
	 * Called when the player has opened a new inventory (such as loot or
	 * a treasure chest) and needs the information for the first time.
	 */
	UFUNCTION(Client, Reliable, BlueprintCallable)
	void Server_StopUseOtherInventory(UInventoryComponent* otherInventory);
	
	
};
