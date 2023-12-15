
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryUpdated, int, slotNumber, bool, bIsEquipmentSlot);

/* Delegate that is called whenever an item is activated. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemActivated, FName, itemName, int, quantityConsumed);

/* Delegate that is called whenever the inventory is 'opened' or 'closed'. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryInUse,
		const AActor*, InUseActor, const bool, IsInUse);



UCLASS(BlueprintType, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class T5GINVENTORYSYSTEM_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
	
protected:
    virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
public:	//functions

    UInventoryComponent();

	// Simple HasAuthority() implementation (UObjects don't have this)
	bool HasAuthority() const;

	TMulticastDelegate<void(bool)> OnInventorySaved;
	TMulticastDelegate<void(bool)> OnInventoryRestored;

	void ReinitializeInventory();

	UFUNCTION(BlueprintCallable)
	FString SaveInventory(FString& responseStr, bool isAsync = false);
	
	UFUNCTION(BlueprintCallable)
	bool LoadInventory(
		FString& responseStr, FString SaveSlotName, bool isAsync = false);

	virtual void OnComponentCreated() override;

	// Event Dispatchers/Delegates
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnInventoryUpdated OnInventoryUpdated;
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnNotificationAvailable OnNotificationAvailable;
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnItemActivated OnItemActivated;
	
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnInventoryInUse OnInventoryInUse;
	
	UFUNCTION(BlueprintPure)
    int GetNumberOfSlots(bool isEquipment = false) const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Equipment Accessors")
	EEquipmentSlotType GetSlotTypeEquipment(int slotNumber) const;
	
	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	EInventorySlotType GetSlotTypeInventory(int slotNumber) const;
	
    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	int GetQuantityOfItem(FName itemName, bool checkEquipment = false) const;

    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	int GetFirstEmptySlotNumber(bool checkEquipment = false) const;

    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	float GetWeightTotal(bool incEquip = false) const;

    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	float getWeightInSlot(int slotNumber, bool isEquipmentSlot = false) const;

    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	TArray<int> GetSlotsWithItem(FName itemName) const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Equipment Accessors")
	TArray<FStInventorySlot> GetCopyOfAllSlots(bool getEquipmentSlots = false) const;

    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	int GetSlotNumberFromEquipmentType(EEquipmentSlotType equipEnum) const;
    
    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	bool IsValidSlot(int slotNumber, bool IsEquipmentSlot = false) const;
    
	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	bool IsValidEquipmentSlotEnum(EEquipmentSlotType equipSlot) const;

	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	bool IsSlotEmpty(int slotNumber, bool isEquipment = false) const;
	
	UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	bool IsSlotEmptyByEnum(EEquipmentSlotType equipSlot) const;

    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	FName GetNameOfItemInSlot(int slotNumber, bool isEquipment = false) const;

    UFUNCTION(BlueprintPure, BlueprintPure, Category = "Inventory Accessors")
	int GetSlotQuantity(int slotNumber, bool isEquipment = false) const;
    
    UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	bool GetIsInventoryReady() const { return bIsInventoryReady; }
	
	UFUNCTION(BlueprintCallable, Category = "Item Management")
	int AddItemFromExistingSlot(
		const FStInventorySlot& NewItem, int quantity, int SlotNumber = -1,
		bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);

    UFUNCTION(BlueprintCallable, Category = "Item Management")
	int AddItemFromDataTable(
		FName ItemName, int quantity, int SlotNumber = -1,
		bool overflowAdd = true, bool overflowDrop = true, bool showNotify = false);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
	FStInventorySlot GetCopyOfSlot(int slotNumber, bool IsEquipment = false) const;

    UFUNCTION(BlueprintCallable, Category = "Equipment Setters")
	bool donEquipment(
		UInventoryComponent* fromInventory, int fromSlot,
		EEquipmentSlotType toSlot = EEquipmentSlotType::NONE);
    
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	bool doffEquipment(
		EEquipmentSlotType equipSlot, int slotNumber = -1,
		UInventoryComponent* toInventory = nullptr, bool destroyResult = false);
	
    UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	int RemoveItemByQuantity(
		FName itemName, int quantity = 1, bool isEquipment = false,
		bool dropToGround = false, bool removeAll = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	int RemoveItemFromSlot(
		int slotNumber = 0, int quantity = 1, bool isEquipment = false,
		bool showNotify = false, bool dropToGround = false, bool removeAll = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory Getters")
	TArray<FStInventoryNotify> GetNotifications();

	UFUNCTION(BlueprintPure)
	bool GetIsInventoryInUse(TArray<AActor*>& InUseActor) const;
	
	UFUNCTION(BlueprintCallable)
	void SetInventoryInUse(AActor* UseActor, bool isInUse = true);
	
	UFUNCTION(BlueprintCallable)
	void IssueStartingItems();
	
protected:
	
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	bool SwapOrStackSlots(
		UInventoryComponent* fromInventory,
		int fromSlotNum = 0, int toSlotNum = 0, int quantity = 1,
		bool showNotify = false, bool isEquipmentSlot = false, bool fromEquipmentSlot = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	bool SwapOrStackSlotsWithRemainder(
		UInventoryComponent* fromInventory,
		int fromSlotNum , int toSlotNum, int quantity, int& remainder,
		bool showNotify = false, bool isEquipmentSlot = false, bool fromEquipmentSlot = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
		bool SplitStack(
			UInventoryComponent* fromInventory, int fromSlotNum = 0,
			int splitQuantity = 1, int toSlotNum = 0, bool showNotify = false);

	UFUNCTION(Server, Reliable)
	void Server_RequestItemActivation(
		UInventoryComponent* inventoryUsed,
		int slotNumber = 0, bool isEquipment = false);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_TransferItems(
		UInventoryComponent* fromInventory, UInventoryComponent* toInventory,
		int fromSlot, int toSlot, int moveQty, bool isFromEquipSlot = false, bool isToEquipSlot = false);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_DropItemOnGround(
		UInventoryComponent* fromInventory,
		int fromSlot, int quantity, bool isFromEquipSlot);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_RequestOtherInventory(UInventoryComponent* targetInventory);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_StopUseOtherInventory(UInventoryComponent* otherInventory);

	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_RestoreSavedInventory(
		const TArray<FStInventorySlot>& RestoredInventory,
		const TArray<FStInventorySlot>& RestoredEquipment);
	
	void RestoreInventory(
		const TArray<FStInventorySlot>& RestoredInventory,
		const TArray<FStInventorySlot>& RestoredEquipment);
	
private: //functions
    
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Accessors")
		FStInventorySlot& GetSlot(int slotNumber, bool IsEquipment = false);

	void Helper_SaveInventory(USaveGame*& SaveData) const;

	int IncreaseSlotQuantity(int slotNumber, int quantity, bool showNotify = false);
	
    int IncreaseSlotQuantity(int slotNumber, int quantity, int& newQuantity, bool showNotify = false);

	int DecreaseSlotQuantity(int slotNumber, int quantity, bool isEquipment = false, bool showNotify = false);

	int DecreaseSlotQuantity(int slotNumber, int quantity, int& remainder, bool isEquipment = false, bool showNotify = false);
	
	void SendNotification(FName itemName, int quantity, bool wasAdded = true);

	UFUNCTION(BlueprintCallable)
    void ResetSlot(int slotNumber = 0, bool isEquipment = false);

	UFUNCTION(BlueprintCallable)	
    bool TransferItemBetweenSlots(
        UInventoryComponent* fromInventory, UInventoryComponent* toInventory,
        int fromSlotNum, int toSlotNum, int moveQuantity = 1, bool overflowDrop = false, bool isFromEquipSlot = false, bool isToEquipSlot = false);

	UFUNCTION(BlueprintCallable)
    bool ActivateSlot(int SlotOfActivation, bool isEquippedActivation = false, bool forceConsume = false);
	
	UFUNCTION()
	void LoadDataDelegate(const FString& SaveSlotName, int32 UserIndex, USaveGame* SaveData);

	UFUNCTION()
	void SaveInventoryDelegate(const FString& SaveSlotName, int32 UserIndex, USaveGame* SaveData);
	
public: //variables

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCanPickUpItems = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Settings")
	bool bShowNotifications = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Settings")
	int NumberOfInvSlots = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Settings")
	TArray<EEquipmentSlotType> EligibleEquipmentSlots;

	// If true, saves will only work on the server.
	// If false, saves will work on the client and server, then send the save data to the server.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Settings")
	bool bSavesOnServer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Settings")
	TArray<FStStartingItem> StartingItems;
	
	// If TRUE, players cannot put items in this inventory, only take from it
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool WithdrawOnly = false;

private: //variables

	UFUNCTION(NetMulticast, Reliable)
	void OnRep_InventorySlotUpdated(const TArray<FStInventorySlot>& OldSlot);
	
	UFUNCTION(NetMulticast, Reliable)
	void OnRep_NewNotification();
	
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_InventorySlotUpdated)
    TArray<FStInventorySlot> InventorySlots_;
	
	UFUNCTION(NetMulticast, Reliable)
	void OnRep_EquipmentSlotUpdated(const TArray<FStInventorySlot>& OldSlot);
	
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_EquipmentSlotUpdated)
    TArray<FStInventorySlot> EquipmentSlots_;

	UPROPERTY(ReplicatedUsing = OnRep_NewNotification)
	TArray<FStInventoryNotify> Notifications_;

    bool bIsInventoryReady = false;

	// Used to prevent clients from cheating their inventory
	bool bInventoryRestored = false;

	UFUNCTION(NetMulticast, Reliable)
	void OnRep_InUseByActors(const TArray<AActor*>& PreviousUseActors);
	
	UPROPERTY(ReplicatedUsing = OnRep_InUseByActors)
	TArray<AActor*> InUseByActors_ = {};

	FString SaveSlotName_ = "";
	
	int32 SaveUserIndex_ = 0;
	
#ifdef UE_BUILD_DEBUG
	bool bShowDebug = true;
#else
	bool bShowDebug = false;
#endif
};
