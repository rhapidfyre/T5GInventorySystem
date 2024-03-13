
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "lib/InventorySlot.h"
#include "GameFramework/SaveGame.h"

#include "InventoryComponent.generated.h"

// Blueprints can only subscribe to dynamic delegates

struct FInventorySlotSaveData;
class UInventoryDataAsset;
/* Delegate that is called whenever a new notification is added to the item notification array.
 * Tells the client that there is an item to be processed.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNotificationAvailable);

/* Delegate that is called on both client and server when an inventory update has occurred
 * Used so that other blueprints/classes can bind to it, running their own function when the inventory updates.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryUpdated, int, slotNumber);

/* Delegate that is called whenever an item is activated. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemActivated,
		const FStItemData&, itemName, int, QuantityConsumed);

/* Delegate that is called whenever the inventory is 'opened' or 'closed'. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryInUse,
const AActor*, InUseActor, const bool, IsInUse);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryRestored,
											bool, bWasSuccessful);



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
	
	UPROPERTY(Blueprintable)
	FOnInventoryRestored OnInventoryRestored;
	
	/**
	 * ACCESSORS, MUTATORS & HELPERS
	 */

	bool HasAuthority() const; // Simple GetOwner()->HasAuthority() Check

	void ReinitializeInventory();

	UFUNCTION(BlueprintCallable)
	FString SaveInventory(FString& responseStr, bool isAsync = true);

	UFUNCTION(BlueprintPure)
	FString GetInventorySaveName() const;

	UFUNCTION(BlueprintPure)
	bool CheckIfSameSlot(const UInventorySlot* ComparisonSlot, const int SlotNumber) const;

	// If the restore boolean is set, this inventory has an associated save
	UFUNCTION(BlueprintPure)
	bool DoesInventorySaveExist() const { return bInventoryRestored; }
	
	UFUNCTION(BlueprintCallable)
	bool LoadInventory(
		FString& responseStr, FString SaveSlotName, int32 SaveUserIndex = 0, bool isAsync = false);

	virtual void OnComponentCreated() override;
	
	UFUNCTION(BlueprintPure)
	int GetNumberOfTotalSlots() const;
	
	UFUNCTION(BlueprintPure)
	int GetNumberOfInventorySlots() const;
	
	UFUNCTION(BlueprintPure)
	int GetNumberOfEquipmentSlots() const;

	UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	FGameplayTagContainer GetSlotInventoryTags(int slotNumber) const;

	UFUNCTION(BlueprintPure, Category = "Equipment Accessors")
	FGameplayTag GetSlotEquipmentTag(int slotNumber) const;
	
    UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	int GetTotalQuantityByItem(const FName& ItemName) const;

    UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	int GetFirstEmptySlotNumber() const;

	UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	float GetTotalWeightOfInventorySlots() const;

	UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	float GetTotalWeightOfEquipmentSlots() const;

    UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	float GetWeightOfSlotNumber(int SlotNumber) const;

	UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	TArray<int> GetInventorySlotNumbersContainingItem(
		const FName& ItemName, const FItemStatics& ItemStatics = FItemStatics()) const;

	UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	TArray<int> GetEquipmentSlotNumbersContainingItem(
		const FName& ItemName, const FItemStatics& ItemStatics = FItemStatics()) const;

	UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	int GetSlotNumberByTag(const FGameplayTag& SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	FItemStatics GetSlotNumberItem(int SlotNumber) const;
    
	UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	const UItemDataAsset* GetSlotNumberItemData(int SlotNumber) const;
	
	UFUNCTION(BlueprintPure, Category = "Inventory Slot Validation")
	bool IsValidItemInSlot(int SlotNumber) const;
	
	UFUNCTION(BlueprintPure, Category = "Inventory Slot Validation")
	bool IsValidSlotNumber(int SlotNumber) const;
    
	UFUNCTION(BlueprintPure, Category = "Inventory Slot Validation")
	bool IsValidEquipmentSlot(int SlotNumber) const;
    
	UFUNCTION(BlueprintPure, Category = "Inventory Slot Validation")
	bool IsValidSlotByEquipmentTag(const FGameplayTag& SearchTag) const;

	UFUNCTION(BlueprintPure, Category = "Inventory Slot Validation")
	bool IsSlotEmpty(int slotNumber) const;

	UFUNCTION(BlueprintPure, Category = "Inventory Slot Validation")
	bool IsSlotEmptyByTag(const FGameplayTag& EquipmentTag);

	UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	int GetQuantityInSlotNumber(int SlotNumber) const;
    
    UFUNCTION(BlueprintPure, Category = "Inventory Accessors")
	bool GetIsInventorySystemReady() const { return bInventoryReady; }
	
	UFUNCTION(BlueprintCallable, Category = "Item Management")
	int AddItem(
		const UInventorySlot* NewItem, int OrderQuantity, int SlotNumber = -1,
		bool bAddOverflow = true, bool bDropOverflow = true, bool bNotify = false);
	
    int AddItemFromDataAsset(
    	const UItemDataAsset* ItemDataAsset, int OrderQuantity, int SlotNumber,
    	bool bAddOverflow, bool bDropOverflow, bool bNotify);

	const UInventorySlot* GetPrimaryEquipmentSlot();

	const UInventorySlot* GetSecondaryEquipmentSlot();

	const UInventorySlot* GetRangedEquipmentSlot();

	const UInventorySlot* GetAmmunitionEquipmentSlot();

	// Execute on the inventory losing the item
    UFUNCTION(BlueprintCallable, Category = "Inventory Mutators")
	int RemoveItemByQuantity(
		const FItemStatics& ItemReference, int OrderQuantity = 1,
		bool bRemoveEquipment = false, bool bDropOnGround = false, bool bRemoveAll = false);

	// Execute on the inventory losing the item
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	int RemoveItemFromSlot(
		int OriginSlotNumber, int OrderQuantity = 1, bool bDropOnGround = false,
		bool bRemoveAllItems = false, bool bNotify = false);

	UFUNCTION(BlueprintCallable, Category = "Inventory Getters")
	TArray<FStInventoryNotify> GetNotifications();
	
	UFUNCTION(BlueprintCallable) void IssueStartingItems();
	
	UFUNCTION(BlueprintPure)
	bool GetCanPickUpItems() const;
	
	UFUNCTION(BlueprintPure)
	bool GetIsWithdrawOnly() const { return bWithdrawOnly; }

	UFUNCTION(BlueprintCallable)
	bool SetInventoryDebugMode(const bool bEnable = true) { return (bShowDebug = bEnable); };

	UFUNCTION(BlueprintCallable)
	bool ActivateSlot(int SlotNumber, bool bForceConsume = false);
	
	
protected:

	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	int SwapOrStackSlots(
		UInventorySlot* OriginSlot, UInventorySlot* TargetSlot, int& RemainingQuantity);

	UFUNCTION(BlueprintCallable, Category = "Inventory Setters")
	int SplitStack(
		UInventorySlot* OriginSlot, UInventorySlot* TargetSlot,
		int OrderQuantity, bool bNotify = false);

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

	UFUNCTION(Client, Reliable)	void Client_InventoryRestored();
	
	UFUNCTION(Server, Reliable)
	void Server_RestoreSavedInventory(
		const TArray<FInventorySlotSaveData>& RestoredInventory);
	
	void RestoreInventory(const TArray<FInventorySlotSaveData>& RestoredInventory);
    
	UInventorySlot* GetInventorySlot(int SlotNumber) const;

	int GetSlotNumber(const UInventorySlot* SlotReference) const;
    
	TArray<UInventorySlot*> GetAllSlots() const;
	
	TArray<UInventorySlot*> GetAllInventorySlots() const;
	
	TArray<UInventorySlot*> GetAllEquipmentSlots() const;

	UFUNCTION(BlueprintCallable, BlueprintCallable, Category = "Inventory Mutators")
	int IncreaseSlotQuantity(int SlotNumber, int OrderQuantity = 1, bool bNotify = false);
	
	UFUNCTION(BlueprintCallable, BlueprintCallable, Category = "Inventory Mutators")
	int DecreaseSlotQuantity(int SlotNumber, int OrderQuantity = 1, bool bNotify = false);

private: //functions

	UFUNCTION() virtual void NotifySlotUpdated(const int SlotNumber);

	void Helper_SaveInventory(USaveGame*& SaveData) const;

	bool Helper_CreateItem(const FPrimaryAssetId& AssetId);
	
	UFUNCTION(BlueprintCallable, BlueprintCallable, Category = "Slot Mutators")
	void SendNotification(const FName& ItemName, int OrderQuantity = 1);

	UFUNCTION(BlueprintCallable) void ResetSlot(UInventorySlot* InventorySlot);

	UFUNCTION(BlueprintCallable)	
    bool TransferItemBetweenSlots(
        UInventorySlot* OriginSlot, UInventorySlot* TargetSlot,
        int OrderQuantity = 1, bool bDropOverflow = false);
	
	UFUNCTION()
	void LoadDataDelegate(const FString& SaveSlotName, int32 UserIndex, USaveGame* SaveData);

	UFUNCTION()
	void SaveInventoryDelegate(const FString& SaveSlotName, int32 UserIndex, const bool bSuccess);

public: //variables

	// Used for reinitializing/resetting the inventory and defining starting items / loot
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UInventoryDataAsset* InventoryDataAsset = nullptr; 

	// If true, saves will only work on the server.
	// If false, saves will work on the client and server, then send the save data to the server.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Settings")
	bool bSavesOnServer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SaveFolder = "";

	
protected: //variables

	UPROPERTY(ReplicatedUsing = OnRep_NewNotification)
	TArray<FStInventoryNotify> Notifications_;

	// Local map used to remember which equipment slots are bound to which slots
	TMap<FGameplayTag, int> TagsMappedToSlots_;
	
	bool bCanPickUpItems = true;
	
	bool bWithdrawOnly = false;

	FString SaveSlotName_ = "";
	
	int32 SaveUserIndex_ = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MaxInventoryReach_ = 1024.f;

	const FString GetFullSavePath() const { return SaveFolder + SaveSlotName_; }

	const int32 GetSaveUserIndex() const { return SaveUserIndex_; }
	
private: //variables

	/**
	 * SLT_ReadOnly: We are only reading from shared memory
	 * SLT_Write:    We are writing to shared memory
	 */
	FRWLock InventoryMutex;

	void MapEquipmentSlot(const FGameplayTag& EquipmentTag, int SlotNumber);
	
	UFUNCTION(NetMulticast, Reliable)
	void OnRep_InventorySlotUpdated(const TArray<UInventorySlot*>& OldSlots);
	
	UFUNCTION(NetMulticast, Reliable)
	void OnRep_NewNotification();

	// TODO - Replace replication with a custom replication method, so that we save network resources
	UPROPERTY(ReplicatedUsing = OnRep_InventorySlotUpdated)
	TArray<UInventorySlot*> InventorySlots_;
	
	bool bInventoryReady = false;

	bool bInventoryFull = false;

	int PrimarySlot_		= -1;
	
	int SecondarySlot_	= -1;
	
	int RangedSlot_		= -1;
	
	int AmmunitionSlot_		= -1;

	// Used to prevent clients from cheating their inventory
	bool bInventoryRestored = false;

#ifdef UE_BUILD_DEBUG
	bool bShowDebug = true;
#endif
	
};
