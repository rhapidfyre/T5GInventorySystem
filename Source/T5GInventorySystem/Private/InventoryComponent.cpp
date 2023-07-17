
// ReSharper disable CppUE4CodingStandardNamingViolationWarning
#include "InventoryComponent.h"
#include "PickupActorBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h" // Used for replication


void UInventoryComponent::BeginPlay()
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): BeginPlay()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    Super::BeginPlay();
    
    const AActor* ownerActor = GetOwner();
    if (!IsValid(ownerActor))
    {
        UE_LOG(LogTemp, Error, TEXT("Inventory has no owner. Removed."));
        this->DestroyComponent(); // Kills itself, it failed validation
    }
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): Play Started. %d Slots and %d Equipment Slots"),
        GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), getNumInventorySlots(), getNumEquipmentSlots());
    }

    if (!IsValid(UItemSystem::getItemDataTable()))
    {
        UE_LOG(LogEngine, Warning, TEXT("%s(%s): Data Table Not Found"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    else
    {
        UE_LOG(LogEngine, Display, TEXT("%s(%s): Data Table Was Found"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    m_bIsInventoryReady = true;
}

UInventoryComponent::UInventoryComponent()
{
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: Constructor"));
    // Inventory Component doesn't need to tick
    // It will be updated as needed
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    // ReSharper disable once CppVirtualFunctionCallInsideCtor
    SetAutoActivate(true);
}

void UInventoryComponent::InitializeInventory()
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): InitializeInventory()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: InitializeInventory"));
    // Ensure the number of inventory slots is valid. If we added an inventory system,
    // it should at least have *one* slot at minimum. Otherwise, wtf is the point?
    if (NumberOfInvSlots < 1) NumberOfInvSlots = 6;
    
    m_inventorySlots.Empty();
    m_equipmentSlots.Empty();
    m_notifications.Empty();
    
    // Setup Inventory Slots
    for (int i = 0; i < NumberOfInvSlots; i++)
    {
        FStInventorySlot newSlot;
        newSlot.SlotType = EInventorySlotType::GENERAL;

        m_inventorySlots.Add(newSlot);
    }

    // Setup Equipment Slots
    for (int i = 0; i < EligibleEquipmentSlots.Num(); i++)
    {
        if (EligibleEquipmentSlots.IsValidIndex(i))
        {
            if (EligibleEquipmentSlots[i] != EEquipmentSlotType::NONE)
            {
                // Ignore slots already added - There can be only one..
                if (getEquipmentSlotNumber(EligibleEquipmentSlots[i]) < 0)
                {
                    FStInventorySlot newSlot;
                    newSlot.SlotType  = EInventorySlotType::EQUIP;
                    newSlot.EquipType = EligibleEquipmentSlots[i];
                    //UE_LOG(LogTemp, Display, TEXT("Adding new equipment slot @ %d"), i);
                    m_equipmentSlots.Add(newSlot);
                }
            }
        }
    }
    
    if (IsValid(GetOwner()))
    {
        if (GetOwner()->HasAuthority())
        {
            while (!StartingItems.IsEmpty())
            {
                if (StartingItems.IsValidIndex(0))
                {
                    // Determine the starting item by item name given
                    const FName itemName = StartingItems[0].startingItem;
                    int slotNum          = -1;
                    if (StartingItems[0].equipType != EEquipmentSlotType::NONE)
                    {
                        if (slotNum < 0)
                        {
                            slotNum = getEquipmentSlotNumber(StartingItems[0].equipType);
                        }
                        if (slotNum >= 0)
                        {
                            m_equipmentSlots[slotNum].ItemName = itemName;
                            m_equipmentSlots[slotNum].SlotQuantity = StartingItems[0].quantity;
                        }
                    }
                    else
                    {
                        const int itemsAdded = AddItemFromDataTable(itemName, StartingItems[0].quantity,
                                            slotNum, false, false, false);
                        if (itemsAdded < 1)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("%s(%s): Item(s) failed to add (StartingItem = %s)"),
                                *GetName(), GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI"), *itemName.ToString());
                        }
                    }

                    StartingItems.RemoveAt(0);
                }
            }
        }
    }

}

void UInventoryComponent::OnComponentCreated()
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): OnComponentCreated()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: OnComponentCreated"));
    Super::OnComponentCreated();
    RegisterComponent();
    InitializeInventory();
    if (bVerboseOutput) bShowDebug = true;
}

FStItemData UInventoryComponent::getItemInSlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getItemInSlot(%d, %s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: getItemInSlot"));
    if (!isEmptySlot(slotNumber, isEquipment))
    {
        const FStInventorySlot& ReferenceSlot = isEquipment ? m_equipmentSlots[slotNumber] : m_inventorySlots[slotNumber];
        return ReferenceSlot.GetItemData();
    }
    return FStItemData();
}

EInventorySlotType UInventoryComponent::getInventorySlotType(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getInventorySlotType(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: getInventorySlotType"));
    if (IsValidSlot(slotNumber))
        return m_inventorySlots[slotNumber].SlotType;
    return EInventorySlotType::NONE;
}

EEquipmentSlotType UInventoryComponent::getEquipmentSlotType(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getEquipmentSlotType(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (isValidEquipmentSlot(slotNumber))
        return m_equipmentSlots[slotNumber].EquipType;
    return EEquipmentSlotType::NONE;
}

int UInventoryComponent::getTotalQuantityInAllSlots(FName itemName)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getTotalQuantityInAllSlots(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *itemName.ToString());
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: GetTotalQuantityInAllSlots"));
    int total = 0;
    for (int i = 0; i < getNumInventorySlots(); i++)
    {
        if (IsValidSlot(i))
        {
            if (getItemNameInSlot(i) == itemName)
                total += m_inventorySlots[i].SlotQuantity;
        }
    }
    return total;
}

int UInventoryComponent::GetFirstEmptySlot()
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): GetFirstEmptySlot()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: GetFirstEmptySlot"));
    for (int i = 0; i < getNumInventorySlots(); i++)
    {
        if (isEmptySlot(i))
            return i;
    }
    return -1;
}

float UInventoryComponent::getTotalWeight(bool incEquip)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getTotalWeight()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    float totalWeight = 0.0f;
    for (int i = 0; i < getNumInventorySlots(); i++)
    {
        if (!isEmptySlot(i))
        {
            totalWeight += getWeightInSlot(i);
        }
    }
    return totalWeight;
}

float UInventoryComponent::getWeightInSlot(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getWeightInSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber);
    if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
    {
        const float weight = UItemSystem::getItemWeight(InventorySlot.GetItemData());
        return (weight * getQuantityInSlot(slotNumber));
    }
    return 0.0f;
}

TArray<int> UInventoryComponent::getSlotsContainingItem(FName itemName)
{   
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getSlotsContainingItem(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *itemName.ToString());
    }
    TArray<int> foundSlots = TArray<int>();
    for (int i = 0; i < getNumInventorySlots(); i++)
    {
        const FStInventorySlot InventorySlot = GetInventorySlot(i);
        if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
        {
            if (InventorySlot.ItemName == itemName)
            {
                foundSlots.Add(i);
            }
        }
    }
    return foundSlots;
}

FStInventorySlot& UInventoryComponent::GetInventorySlot(int slotNumber, bool IsEquipmentSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): GetInventorySlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (!IsValidSlot(slotNumber, IsEquipmentSlot))
        return m_EmptySlot;
    return IsEquipmentSlot ? m_equipmentSlots[slotNumber] : m_inventorySlots[slotNumber];
}

int UInventoryComponent::getEquipmentSlotNumber(EEquipmentSlotType equipEnum)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getEquipmentSlotNumber(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *UEnum::GetValueAsString(equipEnum));
    }
    // Makes sure incoming param is a valid equip slot
    if (equipEnum != EEquipmentSlotType::NONE)
    {
        // Loop through until we find the equip slot since we only have 1 of each
        for (int i = 0; i < getNumEquipmentSlots(); i++)
        {
            const FStInventorySlot equipSlot = getEquipmentSlotByNumber(i);
            if (equipSlot.EquipType == equipEnum)
            {
                return i;
            }
        }
    }
    return -1;
}

FStInventorySlot UInventoryComponent::getEquipmentSlot(EEquipmentSlotType equipEnum)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getEquipmentSlot(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *UEnum::GetValueAsString(equipEnum));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: getEquipmentSlot(enum)"));
    if (equipEnum != EEquipmentSlotType::NONE)
    {
        for (int i = 0; i < m_equipmentSlots.Num(); i++)
        {
            const FStInventorySlot equipSlot = getEquipmentSlotByNumber(i);
            if (equipSlot.EquipType == equipEnum)
            {
                return equipSlot; // Returns a copy of the equipment slot
            }
        }
    }
    return FStInventorySlot();
}

FStInventorySlot UInventoryComponent::getEquipmentSlotByNumber(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getEquipmentSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    // If the slot is valid, returns a copy of the requested slot
    if (isValidEquipmentSlot(slotNumber))
        return m_equipmentSlots[slotNumber];
    return FStInventorySlot();
}

bool UInventoryComponent::IsValidSlot(int slotNumber, bool IsEquipmentSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): IsValidSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (IsEquipmentSlot)
        return (m_equipmentSlots.IsValidIndex(slotNumber));
    return (m_inventorySlots.IsValidIndex(slotNumber));
}
bool UInventoryComponent::isValidEquipmentSlot(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): isValidEquipmentSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    return (m_equipmentSlots.IsValidIndex(slotNumber));
}
bool UInventoryComponent::isValidEquipmentSlotEnum(EEquipmentSlotType equipSlot)
{
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: isValidEquipmentSlotEnum"));
    for (int i = 0; m_equipmentSlots.Num(); i++)
    {
        if (isValidEquipmentSlot(i))
        {
            if (m_equipmentSlots[i].EquipType == equipSlot)
                return true;
        }
    }
    return false;
}

bool UInventoryComponent::isEmptySlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): isEmptySlot(%d, %s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }
    if (!IsValidSlot(slotNumber, isEquipment))
        return false;
    return (getQuantityInSlot(slotNumber, isEquipment) < 1);
}

bool UInventoryComponent::isEquipmentSlotEmpty(EEquipmentSlotType equipSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): GetFirstEmptySlot(%s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *UEnum::GetValueAsString(equipSlot));
    }
    const int slotNum = getEquipmentSlotNumber(equipSlot);
    if (slotNum >= 0)
    {
        if (isValidEquipmentSlot(slotNum))
            return (m_equipmentSlots[slotNum].SlotQuantity < 1);
    }
    return false;
}

FName UInventoryComponent::getItemNameInSlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getItemNameInSlot(%d, %s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }
    const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber, isEquipment);
    return InventorySlot.ItemName;
}

int UInventoryComponent::getQuantityInSlot(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getQuantityInSlot(%d, %s)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }
    if (!IsValidSlot(slotNumber, isEquipment))
        return -1;
    
    if (isEquipment)
    {
        if (m_equipmentSlots.IsValidIndex(slotNumber))
            return m_equipmentSlots[slotNumber].SlotQuantity;
        return 0;
    }
    if (m_inventorySlots.IsValidIndex(slotNumber))
    {
        return m_inventorySlots[slotNumber].SlotQuantity;
    }
    return 0;
}

/**
 * @brief Adds the item in the referenced slot to this inventory with the given quantity
 * @param InventorySlot READONLY - The inventory slot containing the reference for the item being copied
 * @param quantity How many to add
 * @param SlotNumber The slot to add to. Negative means stack or fill first slot.
 * @param overflowAdd If true, add to the next slot if this slot fills up
 * @param overflowDrop If true, drop items to ground if slot fills up
 * @param showNotify If true, notify the player a new item was added
 * @return The amount of items added. Negative indicates error.
 */
int UInventoryComponent::AddItemFromExistingSlot(const FStInventorySlot& InventorySlot,
    int quantity, int SlotNumber, bool overflowAdd, bool overflowDrop, bool showNotify)
{
    // If reference slot is empty, do nothing
    if (InventorySlot.IsSlotEmpty())
        return -1;
    
    if (quantity < 1)
        quantity = 1;

    int RemainingQuantity = quantity;
    const bool IsHuntingForSlot = (SlotNumber < 0);

    // This is a safeguard against an infinite loop
    int LoopIterations = 0;
    do
    {
        LoopIterations += 1;
        
        // If the slot number is invalid, find first stackable or empty slot
        if (SlotNumber < 0)
        {
            
            // Item must be stackable
            if (UItemSystem::GetIsStackable(InventorySlot.ItemName))
            {
                // Find the first slot with the same item & durability
                const TArray<int> MatchingSlots = getSlotsContainingItem(InventorySlot.ItemName);
                for (const int ExistingSlot : MatchingSlots)
                {
                    if (MatchingSlots.IsValidIndex(ExistingSlot))
                    {
                        const int TempSlotNumber        = MatchingSlots[ExistingSlot];
                        const FStInventorySlot& MatchingSlot  = GetInventorySlot(TempSlotNumber);

                        const float MaxDurability = UItemSystem::getDurability(InventorySlot.ItemName);
                        const bool DoesDurabilityMatch = MatchingSlot.SlotDurability == InventorySlot.SlotDurability;
                        const bool DoesNewItemHaveMaxDurability = InventorySlot.SlotDurability >= MaxDurability;

                        if ( DoesDurabilityMatch || DoesNewItemHaveMaxDurability)
                        {
                            const int QuantityInSlot    = getQuantityInSlot(TempSlotNumber);
                            const int MaxStackSize      = UItemSystem::getMaximumStackSize(InventorySlot.ItemName);

                            // The slot has space for this item
                            if (QuantityInSlot < MaxStackSize)
                            {
                                SlotNumber = TempSlotNumber;
                                break; // We found our slot. Break the loop.
                            }
                        }
                        
                    }
                }
            }// is stackable
            
            // If the item wasn't stacked/can't be stacked, find first empty slot
            if ((SlotNumber < 0) && (quantity > 0))
            {
                const int FirstFreeSlot = GetFirstEmptySlot();
                if (IsValidSlot(FirstFreeSlot))
                {
                    SlotNumber = FirstFreeSlot;
                }
            }
        }

        // If the slot is now valid (or was valid to start with), add the item
        if (SlotNumber >= 0)
        {
            const int QuantityInSlot = getQuantityInSlot(SlotNumber);
            const int MaxStackSize = UItemSystem::getMaximumStackSize(InventorySlot.ItemName);
            const int NewQuantity = QuantityInSlot + RemainingQuantity;

            if (QuantityInSlot < MaxStackSize)
            {
                m_inventorySlots[SlotNumber].ItemName     = InventorySlot.ItemName;
                m_inventorySlots[SlotNumber].SlotQuantity = NewQuantity > MaxStackSize ? MaxStackSize : NewQuantity;

                // The current quantity minus the original quantity is how many items were added
                RemainingQuantity -= (m_inventorySlots[SlotNumber].SlotQuantity - QuantityInSlot);
            }

            // If we're hunting for a slot, keep looping if more items remain.
            if (RemainingQuantity > 0 && IsHuntingForSlot)
                return RemainingQuantity;
        }
        
        // If the slot is still not valid, the inventory is full
        else
        {
            UE_LOG(LogTemp, Display, TEXT("%s(%s): AddItemFromDataTable() Failed - Inventory Full"),
                *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
            break;
        }
        
    }
    /* Keep looping so long as there is:
     *      - 
     *      - 
     *      - 
     *      - 
     */
    while (
        RemainingQuantity > 0       // There is more to add,                            AND
        && overflowAdd              // Add all overflowing items,                       AND
        && IsHuntingForSlot         // No specific spot was specified to begin with,    AND
        && LoopIterations < 1000    // The loop is not suspected to be infinite/hung
        );

    if (LoopIterations >= 1000)
    {
        UE_LOG(LogTemp, Error, TEXT("%s(%s): AddItemFromDataTable() was safeguarded against an infinite loop"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
        return RemainingQuantity;
    }

    // If dropping overflow on the ground
    if (overflowDrop && RemainingQuantity > 0)
    {
        const ACharacter* OwnerCharacter = Cast<ACharacter>( GetOwner() );
        const USkeletalMeshComponent* thisMesh = OwnerCharacter->GetMesh();
        if (!IsValid(thisMesh))
            return RemainingQuantity;
        
        const FTransform spawnTransform(
            FRotator(
                FMath::RandRange(0.f,359.f),
                 FMath::RandRange(0.f,359.f),
                 FMath::RandRange(0.f,359.f)),
            thisMesh->GetBoneLocation("Root"));
        FActorSpawnParameters spawnParams;
        spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        APickupActorBase* pickupItem = GetWorld()->SpawnActorDeferred<APickupActorBase>(
                                                APickupActorBase::StaticClass(), spawnTransform);
        if (IsValid(pickupItem))
        {
            pickupItem->SetupItemFromName(InventorySlot.ItemName, RemainingQuantity);
            pickupItem->FinishSpawning(spawnTransform);
        }
        RemainingQuantity = 0;
    }

    // There are either still items remaining (inventory is full), or
    // the specified slot given was not found or is invalid (doesn't exist)
    return RemainingQuantity;

}

/**
    * @brief Adds the requested item to this inventory with the given quantity.
    *       Internally calls 'AddItemFromExistingSlot()' by making a pseudo-slot.
    * @param ItemName Name of the item to be added from the data table
    * @param quantity The total quantity to add
    * @param SlotNumber The slot to add to. Negative means stack or fill first slot.
    * @param overflowAdd If true, add to the next slot if this slot fills up
    * @param overflowDrop If true, drop items to ground if slot fills up
    * @param showNotify If true, notify the player a new item was added
    * @return The number of items added. Zero indicates failure.
    */
int UInventoryComponent::AddItemFromDataTable(FName ItemName, int quantity, int SlotNumber, bool overflowAdd,
    bool overflowDrop, bool showNotify)
{
    if (!UItemSystem::getItemNameIsValid(ItemName))
        return -1;
    
    const FStItemData ItemData = UItemSystem::getItemDataFromItemName(ItemName);
    
    // Slot number was given, but is an invalid inventory slot
    if (SlotNumber >= 0)
    {
        // Invalid Slot
        if (!IsValidSlot(SlotNumber))
            return -2;
        
        // Slot is Full
        if (GetInventorySlot(SlotNumber).SlotQuantity >= ItemData.maxStackSize)
            return -3;
    }
    
    // If there's nothing to add, then do nothing
    if (quantity < 1)
        return -4;

    // Use a temp variable so we're not operating on the quantity parameter directly
    const int RemainingQuantity = quantity;

    // pseudo-slot to simply function operations & avoid repeated code
    const FStInventorySlot NewSlotReference = FStInventorySlot(ItemName, quantity);

    const int ItemsAdded = AddItemFromExistingSlot(NewSlotReference, quantity, SlotNumber, overflowAdd, overflowDrop, showNotify);
    if (ItemsAdded >= 0)
    {
        return RemainingQuantity - ItemsAdded;
    }
    return RemainingQuantity;
}

// Public accessor version of 'GetInventorySlot', but returns a copy instead of a reference.
FStInventorySlot UInventoryComponent::GetCopyOfSlot(int slotNumber, bool IsEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): GetInventorySlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (IsEquipment)
    {
        if (IsValidSlot(slotNumber))
        {
            return m_inventorySlots[slotNumber];
        }
    }
    else
    {
        if (isValidEquipmentSlot(slotNumber))
        {
            return m_equipmentSlots[slotNumber];
        }
    }
    return FStInventorySlot();
}


bool UInventoryComponent::donEquipment(UInventoryComponent* fromInventory, int fromSlot, EEquipmentSlotType eSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): donEquipment()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (!IsValid(fromInventory)) fromInventory = this;
    
    // Per UE coding convention, "IsValid" should be used when possible over "nullptr" check.
    const FStInventorySlot InventorySlot = fromInventory->GetInventorySlot(fromSlot);
    if (!UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
        return false;

    // If the parameter was not a specific equipment slot, find one that works.
    EEquipmentSlotType equipSlot = eSlot;
    if (eSlot == EEquipmentSlotType::NONE)
    {
        // Get all slots this item can be equipped into as an array
        // Attempt to add the item to each slot until it fits or fails
        TArray<EEquipmentSlotType> eligibleSlots = UItemSystem::getItemEquipSlots(InventorySlot.GetItemData());
        for (int i = 0; i < eligibleSlots.Num(); i++)
        {
            const FStInventorySlot foundSlot = getEquipmentSlot(eligibleSlots[i]);
            // If the found slot is valid and matches, then we equip it.
            if (foundSlot.EquipType != EEquipmentSlotType::NONE)
            {
                equipSlot = eligibleSlots[i];
                break;
            }
        }
    }

    // An equip slot was either provided, or found
    if (equipSlot != EEquipmentSlotType::NONE)
    {
        // If the slot is invalid, or the slot types don't match, fail.
        const int slotNumber = getEquipmentSlotNumber(eSlot);
        if (!isValidEquipmentSlot(slotNumber))                  return false;
        if (getEquipmentSlotByNumber(slotNumber).EquipType != eSlot)    return false;

        m_equipmentSlots[slotNumber].ItemName = fromInventory->getItemNameInSlot(fromSlot);
        const int itemsRemoved = fromInventory->removeItemFromSlot(fromSlot, INT_MAX, false, false, false, true);
        m_equipmentSlots[slotNumber].SlotQuantity = itemsRemoved;
        InventoryUpdate(slotNumber, true);
        return true;
    }
    
    return false;
}

bool UInventoryComponent::doffEquipment(EEquipmentSlotType equipSlot, int slotNumber, UInventoryComponent* toInventory, bool destroyResult)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): doffEquipment()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    // If destination inventory not provided, use this inventory
    if (!IsValid(toInventory)) toInventory = this;
    if (toInventory->isInventoryReady())
    {
        // Make sure the destination slot is available
        if (slotNumber >= 0)
        {
            if (!toInventory->isEmptySlot(slotNumber))
                return false;
        }

        // Remove the equipment
        const int fromSlot = getEquipmentSlotNumber(equipSlot);
        {
            if ( !isEmptySlot(fromSlot, true) )
            {
                FStItemData itemData = getItemInSlot(fromSlot, true);
                
                // Always remove the item first, to avoid duplication exploits
                const FStInventorySlot InventorySlotCopy = GetCopyOfSlot(slotNumber, true);
                const int itemsRemoved = removeItemFromSlot(fromSlot, INT_MAX,
                        true, false, false, true);
                if (itemsRemoved > 0)
                {
                    toInventory->AddItemFromExistingSlot(InventorySlotCopy, itemsRemoved, false, false, false);
                }
            }
        }
    }
    return false;
}


int UInventoryComponent::removeItemFromSlot(int slotNumber, int quantity, bool isEquipment, bool showNotify, bool dropToGround, bool removeAll)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): removeItemFromSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (quantity < 1) quantity = 1;
    if (isInventoryReady())
    {
        if (isEquipment && isValidEquipmentSlot(slotNumber) || !isEquipment && IsValidSlot(slotNumber))
        {
            const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber, isEquipment);
            const FStItemData existingItem = getItemInSlot(slotNumber, isEquipment);
            if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
            {
            
                // Can't remove an item that doesn't exist.
                const FName itemName = InventorySlot.ItemName;
                if (itemName == UItemSystem::getInvalidName())
                {
                    UE_LOG(LogTemp, Warning, TEXT("removeItemFromSlot(%d) called, but slot was invalid/empty."), slotNumber);
                    return -1;
                }
            
                // Ensures 'quantity' is not greater than the actually quantity in slot
                const int slotQuantity = getQuantityInSlot(slotNumber, isEquipment);
                int qty = (slotQuantity < quantity) ? slotQuantity : quantity;

                // All items are being removed
                if (removeAll) { qty = slotQuantity; }
                if (qty == slotQuantity || removeAll)
                {
                    if (isEquipment)
                    {
                        m_equipmentSlots[slotNumber].SlotQuantity = 0;
                        m_equipmentSlots[slotNumber].ItemName = FName();
                    }
                    else
                    {
                        m_inventorySlots[slotNumber].SlotQuantity = 0;
                        m_inventorySlots[slotNumber].ItemName = FName();
                    }
                }
                // Only a portion of the items are being removed.
                else
                {
                    if (isEquipment) m_equipmentSlots[slotNumber].SlotQuantity -= qty;
                    else             m_inventorySlots[slotNumber].SlotQuantity -= qty;
                }
            
                if (showNotify) sendNotification(itemName, qty, false);
            
                // Removes the item from memory
                if (getQuantityInSlot(slotNumber) < 1)
                {
                    if (isEquipment) m_equipmentSlots[slotNumber].ItemName = FName();
                    else             m_inventorySlots[slotNumber].ItemName = FName();
                }

                InventoryUpdate(slotNumber, isEquipment);
                return qty;
            }
        }
    }
    return -1;
}

int UInventoryComponent::removeItemByQuantity(FName itemName, int quantity, bool isEquipment, bool dropToGround, bool removeAll)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): removeItemByQuantity(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), quantity);
    }
    if (quantity < 1) quantity = 1;
    if (isInventoryReady() && itemName != UItemSystem::getInvalidName())
    {
        int qty = quantity; // Track how many we've removed
        const TArray<int> itemSlots = getSlotsContainingItem(itemName);

        for (int i = 0; i < itemSlots.Num(); i++)
        {
            // Continues removing items until quantity is met
            const int numRemoved = removeItemFromSlot(itemSlots[i], qty,
                isEquipment, true, dropToGround, removeAll);
            qty -= numRemoved;

            if (qty < 1) break;
        }
        
        // If current quantity is not equal to the requested quantity, we succeeded
        if (qty != quantity)
            return (quantity - qty); // The amount actually removed
        
    }
    return -1;
}

bool UInventoryComponent::swapOrStackSlots(UInventoryComponent* fromInventory, int fromSlotNum, int toSlotNum, int quantity, bool showNotify, bool isEquipmentSlot, bool fromEquipmentSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): swapOrStackSlots()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    int remainder;
    return swapOrStackWithRemainder(fromInventory, fromSlotNum, toSlotNum, quantity, remainder, showNotify, isEquipmentSlot, fromEquipmentSlot);
}


bool UInventoryComponent::swapOrStackWithRemainder(UInventoryComponent* fromInventory, int fromSlotNum,
    int toSlotNum, int quantity, int& remainder, bool showNotify, bool isEquipmentSlot, bool fromEquipmentSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): swapOrStackWithRemainder()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    
    // If the 'fromInventory' is invalid, we're transferring an item within this same inventory;
    if (!IsValid(fromInventory)) fromInventory = this;
    
    if (!fromInventory->IsValidSlot(fromSlotNum)) return false;
    if (!IsValidSlot(toSlotNum)) toSlotNum = -1;
    if (quantity < 1) quantity = 1;

    // Remember which items were in which slots
    const FStInventorySlot FromInventorySlot = fromInventory->GetInventorySlot(fromSlotNum);
    const FStInventorySlot ToInventorySlot   = GetInventorySlot(toSlotNum);
    
    const int fromQuantity = FromInventorySlot.SlotQuantity;

    // If the FROM item isn't valid, this is not a valid swap. Otherwise.... wtf are they swapping?
    if (!UItemSystem::getItemNameIsValid(FromInventorySlot.ItemName))
    {
        return false;
    }

    // Ensures the slot matches what the client says it is and that the FROM slot is a valid slot type
    // ReSharper disable once CppInitializedValueIsAlwaysRewritten
    bool isCorrectSlotType = false;
    if (fromEquipmentSlot)
    {
        isCorrectSlotType = (fromInventory->getEquipmentSlotType(fromSlotNum)) != EEquipmentSlotType::NONE;
    }
    else
        isCorrectSlotType = (fromInventory->getInventorySlotType(fromSlotNum)) != EInventorySlotType::NONE;
    if (!isCorrectSlotType)
    {
        return false;
    }

    // Ensures the destination matches what the client says, and that the TO slot is a valid slot for this type.
    // ReSharper disable once CppInitializedValueIsAlwaysRewritten
    bool destinationValid = false;
    if (isEquipmentSlot)
        destinationValid = getEquipmentSlotType(toSlotNum) != EEquipmentSlotType::NONE;
    else
    {
        if (toSlotNum < 0) toSlotNum = GetFirstEmptySlot();
        destinationValid = getInventorySlotType(toSlotNum) != EInventorySlotType::NONE;
    }
    if (!destinationValid)
    {
        return false;
    }

    // Check if there's an existing item in the destination slot
    if (UItemSystem::getItemNameIsValid(ToInventorySlot.ItemName))
    {

        // Check if items can be stacked. This will be the easiest, fastest way to manage this call.
        if (UItemSystem::IsSameItem(fromInventory->GetInventorySlot(fromSlotNum), GetInventorySlot(toSlotNum)))
        {
            if (fromInventory->decreaseQuantityInSlot(fromSlotNum, quantity, showNotify))
            {
                const int itemsAdded = increaseQuantityInSlot(toSlotNum, quantity, showNotify);
                remainder = (quantity - itemsAdded);
                return true;   
            }
        }
        
    }

    // We have one last problem to look for; If one of the slots (or both) is an equipment slot, we obviously can't
    // swap something into it that is not a matching equipment type. So we need to check that the types match.
    // i.e
    //   You can't take a sword out of primary, and swap a carrot from the inventory into the primary slot.
    if ((fromEquipmentSlot || isEquipmentSlot) && UItemSystem::getItemNameIsValid(ToInventorySlot.ItemName))
    {
        // Gets the eligible slot types from the existing item (the item in the origination slot)
        const EEquipmentSlotType ourSlotType = getEquipmentSlotType(toSlotNum);
        const TArray<EEquipmentSlotType> slotTypes = UItemSystem::getItemEquipSlots(ToInventorySlot.GetItemData());
        const int numSlots = slotTypes.Num();
        bool typeMatches = false;
        if (numSlots > 0)
        {
            for (int i = 0; i < numSlots; i++)
            {
                // If this equipment slot type matches this destination slot's equip type, we're good.
                if (slotTypes[i] == ourSlotType)
                {
                    typeMatches = true;
                    break;
                }
            }
        }
        if (!typeMatches)
        {
            return false;
        }
    }

    // Step 1: Validate the amount of item to move over.
    const int moveQuantity = quantity > fromQuantity ? fromQuantity : quantity;

    // Step 2: Remember the item we used to have in this slot.
    FStItemData oldItem = getItemInSlot(toSlotNum, isEquipmentSlot); // Returns -1 if this slot was empty

    // If the items match or the destination slot is empty, it will stack.
    // Otherwise it will swap the two slots.
    const bool isSameItem = UItemSystem::IsSameItem(fromInventory->GetInventorySlot(fromSlotNum), GetInventorySlot(toSlotNum)); 
    if (isSameItem || !UItemSystem::getItemNameIsValid(ToInventorySlot.ItemName))
    {
        if (isEquipmentSlot)
        {
            const EEquipmentSlotType eSlot = getEquipmentSlotType(toSlotNum);
            return donEquipment(this, fromSlotNum, eSlot);
        }
        if (fromInventory->decreaseQuantityInSlot(fromSlotNum, moveQuantity, showNotify) > 0)
        {
            const int ItemsAdded = AddItemFromExistingSlot(FromInventorySlot, moveQuantity, toSlotNum, false, false, showNotify);
            remainder = moveQuantity - getQuantityInSlot(toSlotNum, isEquipmentSlot);
            if (ItemsAdded <= 0)
                return false;
            return true;
        }
    }
    else
    {
        if (fromInventory->decreaseQuantityInSlot(fromSlotNum, fromQuantity, fromEquipmentSlot, showNotify) > 0)
        {
            const int ItemsAdded = AddItemFromExistingSlot(FromInventorySlot, fromQuantity, toSlotNum, false, false, showNotify);
            remainder = moveQuantity - getQuantityInSlot(toSlotNum, isEquipmentSlot);
            if (ItemsAdded <= 0)
            {
                return false;
            }
            return true;
        }
        
        remainder = moveQuantity - getQuantityInSlot(toSlotNum, isEquipmentSlot);
        return true;
    }
    return false;
}

/**
 * @deprecated 
 * @brief Update each slot one by one. Never use a sweeping update.
 */
void UInventoryComponent::OnRep_InventorySlotUpdated() const
{
    UE_LOG(LogTemp, Warning, TEXT("InventoryComponent(%s): OnRep_InventorySlotUpdated() - This should really be removed."),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    OnInventoryUpdated.Broadcast(-1, false);
}

/**
 * @deprecated 
 * @brief Update each slot one by one. Never use a sweeping update.
 */
void UInventoryComponent::OnRep_EquipmentSlotUpdated() const
{
    UE_LOG(LogTemp, Warning, TEXT("InventoryComponent(%s): OnRep_EquipmentSlotUpdated() - This should really be removed."),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    OnInventoryUpdated.Broadcast(-1, true);
}

bool UInventoryComponent::splitStack(
    UInventoryComponent* fromInventory, int fromSlotNum, int splitQuantity, int toSlotNum, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): splitStack()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    //UE_LOG(LogTemp, Display, TEXT("InventoryComponent: splitStack"));
    if (splitQuantity < 1) splitQuantity = 1;
    if (fromInventory == nullptr) fromInventory = this;

    // Always show notification if the inventory isn't the same (item gained/lost)
    if (!showNotify) showNotify = (fromInventory == this);
    const FStInventorySlot InventorySlot = fromInventory->GetCopyOfSlot(fromSlotNum);
    
    const FStItemData existingItem = fromInventory->getItemInSlot(fromSlotNum);
    
    if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
    {
        const int existingQty = fromInventory->getQuantityInSlot(fromSlotNum) - 1;
        // Never move the entire stack, always a maximum of 1 less than the stack quantity
        const int qty = (splitQuantity >= existingQty) ? existingQty : splitQuantity;
        if (qty < 1)
        {
            return false;
        }
        
        // Always take the item first, to avoid duplication exploits.
        if (fromInventory->decreaseQuantityInSlot(fromSlotNum, qty) >= 0, showNotify)
        {
            const int itemsAdded = AddItemFromExistingSlot(InventorySlot, qty,
                                                           toSlotNum, false, false, showNotify);
            
            if (itemsAdded > 0)
            {
                // If items added were less than we intended, we weren't able to add all of them.
                if (itemsAdded < qty)
                {
                    // Re-add the ones that weren't able to stack
                    fromInventory->increaseQuantityInSlot(fromSlotNum, qty - itemsAdded, showNotify);
                }
                return true;
            }
            // We failed to add the items. Refund the original item.
            fromInventory->AddItemFromExistingSlot(InventorySlot, qty, fromSlotNum, false, false);
            
        }
    }
    return false;
}


int UInventoryComponent::increaseQuantityInSlot(int slotNumber, int quantity, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): increaseQuantityInSlot()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    int newQty;
    return increaseQuantityInSlot(slotNumber, quantity, newQty, showNotify);
}

int UInventoryComponent::increaseQuantityInSlot(int slotNumber, int quantity, int& newQuantity, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): increaseQuantityInSlot(overload)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (quantity < 1) quantity = 1;
    const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber);
    const FStItemData slotItem = getItemInSlot(slotNumber);
    if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName) && !InventorySlot.IsSlotEmpty())
    {
        const int maxQty = UItemSystem::getMaximumStackSize(InventorySlot.ItemName);
        const int startQty = getQuantityInSlot(slotNumber);
        const int newTotal = startQty + abs(quantity);
        newQuantity = (newTotal >= maxQty) ? maxQty : newTotal;
        const int amountAdded = (newQuantity - startQty);
        m_inventorySlots[slotNumber].SlotQuantity = newQuantity;
        if (showNotify)
        {
            sendNotification(getItemNameInSlot(slotNumber), amountAdded);
        }
        InventoryUpdate(slotNumber);
        return amountAdded;
    }
    return -1;
}

int UInventoryComponent::decreaseQuantityInSlot(int slotNumber, int quantity, int& remainder, bool isEquipment, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): decreaseQuantityInSlot(overload)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (quantity < 1) quantity = 1;
    const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber);
    const FStItemData slotItem = getItemInSlot(slotNumber);
    if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
    {
        const int startQty = getQuantityInSlot(slotNumber);
        const int tempQty = startQty - abs(quantity);
        remainder = (tempQty >= 0) ? tempQty : 0;
        
        // Since we may have only removed 'n' amount instead of 'quantity', we need to recalculate
        const int amountRemoved = startQty - remainder;

        FStInventorySlot& ReferenceSlot = isEquipment ? m_equipmentSlots[slotNumber] : m_inventorySlots[slotNumber];
        ReferenceSlot.SlotQuantity = remainder;
        
        if (showNotify)
        {
            sendNotification(getItemNameInSlot(slotNumber), amountRemoved, false);
        }

        // If nothing is left, remove it altogether.
        if (ReferenceSlot.SlotQuantity < 1)
        {
            ReferenceSlot.EmptyAndResetSlot();
        }
        
        InventoryUpdate(slotNumber);
        return amountRemoved;
    }
    return -1;
}

int UInventoryComponent::decreaseQuantityInSlot(int slotNumber, int quantity, bool isEquipment, bool showNotify)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): decreaseQuantityInSlot()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    int remainder;
    return decreaseQuantityInSlot(slotNumber, quantity, remainder, isEquipment, showNotify);
}

TArray<FStInventoryNotify> UInventoryComponent::getNotifications()
{
    TArray<FStInventoryNotify> invNotify;
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): getNotifications()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (!m_notifications.IsEmpty())
    {
        invNotify = m_notifications;
        m_notifications.Empty();
        return invNotify;
    }
    return invNotify;
}

void UInventoryComponent::sendNotification_Implementation(FName itemName, int quantity, bool wasAdded)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): sendNotification_Implementation()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (quantity <= 0) return;
    FStInventoryNotify invNotify;
    invNotify.itemName = itemName;
    invNotify.itemQuantity = (wasAdded ? abs(quantity) : abs(quantity) * (-1));
    UE_LOG(LogTemp, Display, TEXT("A new inventory notification is available"));
    m_notifications.Add(invNotify);
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("%s(%s): OnNotificationAvailable.Broadcast()"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    OnNotificationAvailable.Broadcast();
}

void UInventoryComponent::resetInventorySlot(int slotNumber)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): resetInventorySlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    removeItemFromSlot(slotNumber, 1, false,true);
    InventoryUpdate(slotNumber);
}

void UInventoryComponent::InventoryUpdate(int slotNumber, bool isEquipment, bool isAtomic)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): InventoryUpdate(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    // Notify delegate so bound functions can trigger
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("%s(%s): OnInventoryUpdated.Broadcast(%d, %s)"),
            *GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"),
            slotNumber, isEquipment?TEXT("TRUE"):TEXT("FALSE"));
    }

    // Fire server delegates
    if (isAtomic)
        OnInventoryUpdated.Broadcast(-1, isEquipment);
    else
        OnInventoryUpdated.Broadcast(slotNumber, isEquipment);

    // Trigger client delegates
    Multicast_InventoryUpdate(isAtomic ? -1 : slotNumber, isEquipment);
}

void UInventoryComponent::Multicast_InventoryUpdate_Implementation(int slotNumber, bool isEquipment)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): Client_InventoryUpdate_Implementation()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    OnInventoryUpdated.Broadcast(slotNumber, isEquipment);
}

bool UInventoryComponent::transferItemBetweenSlots(
    UInventoryComponent* fromInventory, UInventoryComponent* toInventory,
    int fromSlotNum, int toSlotNum, int moveQuantity, bool overflowDrop, bool isFromEquipSlot, bool isToEquipSlot)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): transferItemBetweenSlots()"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
    }
    if (moveQuantity < 1) moveQuantity = 1;
    if (!IsValid(fromInventory)) fromInventory = this;
    if (!IsValid(toInventory)) toInventory = this;

    bool showNotify = false;
    if (fromInventory != toInventory) showNotify = true;

    // The from item MUST be valid since the authoritative inventory is the one GAINING the item.
    const FStInventorySlot FromInventorySlot = fromInventory->GetCopyOfSlot(fromSlotNum, isFromEquipSlot);
    FStItemData fromItem = FromInventorySlot.GetItemData();
    if (UItemSystem::getItemNameIsValid(FromInventorySlot.ItemName))
    {
        // Does an item exist in the destination slot?
        const FStInventorySlot ToInventorySlot = GetInventorySlot(toSlotNum, isToEquipSlot);
        FStItemData toItem = ToInventorySlot.GetItemData();
        if (UItemSystem::getItemNameIsValid(ToInventorySlot.ItemName) || toSlotNum < 0)
        {
            // If to slot is valid, we're swapping or stacking the slots.
            if (toSlotNum >= 0)
            {
                if (toInventory->swapOrStackSlots(fromInventory, fromSlotNum, toSlotNum, moveQuantity, showNotify))
                {
                    return true;
                }
            }
            // If TO slot is not valid, move item to first available slot.
            else
            {
                int itemsRemoved = fromInventory->removeItemFromSlot(fromSlotNum, moveQuantity, showNotify, overflowDrop);
                if (itemsRemoved > 0)
                {
                    int slotNum = toInventory->AddItemFromExistingSlot(FromInventorySlot, fromSlotNum, moveQuantity, true, overflowDrop, showNotify);
                    if (slotNum >= 0)
                    {
                        return true;
                    }
                }
            }
        }
        // If an existing item doesn't exist, we're splitting.
        else
        {
            int fromQuantity = fromInventory->getQuantityInSlot(fromSlotNum);
            if (fromQuantity > moveQuantity)
            {
                toInventory->splitStack(fromInventory, fromSlotNum, moveQuantity, toSlotNum, showNotify);
            }
        }
    }
    
    return false;
}

bool UInventoryComponent::activateItemInSlot(int slotNumber, bool isEquipment, bool consumeItem)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): activateItemInSlot(%d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), slotNumber);
    }
    if (IsValidSlot(slotNumber))
    {
        const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber, isEquipment);
        FStItemData itemInSlot = InventorySlot.GetItemData();
        if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
        {
            if (itemInSlot.itemActivation == EItemActivation::EQUIP)
            {
                int destinationSlot = -1;
                for ( const EEquipmentSlotType slotType : itemInSlot.equipSlots )
                {
                    const int eSlotNum = getEquipmentSlotNumber(slotType);
                    if (isValidEquipmentSlot(eSlotNum))
                    {
                        destinationSlot = eSlotNum;
                        break;
                    }
                }
                return swapOrStackSlots(this,slotNumber, destinationSlot, 1,
                false, !isEquipment, isEquipment);
            }
            // The item is consumed, or consume is forced
            if (consumeItem || itemInSlot.consumeOnUse)
            {
                // If the item fails to be removed/deducted
                if (decreaseQuantityInSlot(slotNumber,1,isEquipment) < 1)
                {
                    return false;
                }
            }
            OnItemActivated.Broadcast(InventorySlot.ItemName);
            return true;
        }
    }
    return false;
}

void UInventoryComponent::Server_RequestItemActivation_Implementation(
    UInventoryComponent* inventoryUsed, int slotNumber, bool isEquipment)
{
    if (GetOwner()->HasAuthority())
    {
        const FStInventorySlot InventorySlot = GetInventorySlot(slotNumber, isEquipment);
        const FStItemData itemData = inventoryUsed->getItemInSlot(slotNumber, isEquipment);
        if (UItemSystem::getItemNameIsValid(InventorySlot.ItemName))
        {
            inventoryUsed->activateItemInSlot(slotNumber, isEquipment);
        }
    }
}

bool UInventoryComponent::setNotifyItem(FName itemName, int quantityAffected)
{
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Display, TEXT("InventoryComponent(%s): setNotifyItem(%s, %d)"),
            GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"), *itemName.ToString(), quantityAffected);
    }
    if (UItemSystem::getItemNameIsValid(itemName))
    {
        sendNotification(itemName, quantityAffected, quantityAffected > 0);
        return true;
    }
    return false;
}





//---------------------------------------------------------------------------------------------------------------
//---------------------------------- REPLICATION ----------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------



void UInventoryComponent::Server_TransferItems_Implementation(UInventoryComponent* fromInventory,
    UInventoryComponent* toInventory, int fromSlot, int toSlot, int moveQty, bool isFromEquipSlot, bool isToEquipSlot)
{
    UE_LOG(LogTemp, Display, TEXT("InventoryComponent: Server_TransferItems_Implementation"));
    // Validation
    if (!IsValid(fromInventory)) fromInventory = this;
    if (!IsValid(toInventory)) toInventory = this;

    // Move is from and to the exact same slot. Do nothing.
    if (fromInventory == toInventory && fromSlot == toSlot && isFromEquipSlot == isToEquipSlot)
        return;
    
    AActor* fromOwner = fromInventory->GetOwner();
    AActor* toOwner = toInventory->GetOwner();
    const ACharacter* fromPlayer = Cast<ACharacter>(fromOwner);
    const ACharacter* toPlayer   = Cast<ACharacter>(toOwner);

    // Make sure destination inventory is not withdraw only. Ignore this if it's the player's own inventory.
    if (toInventory != this && toInventory->m_WithdrawOnly)
    {
        UE_LOG(LogTemp, Display, TEXT("Inventory %s is WITHDRAW ONLY. Unable to place items into it."), *toInventory->GetName());
        return;
    }
    
    // This player is trying to modify a player-owned inventory.
    if (IsValid(fromPlayer))
    {
        // The *from* inventory is not this player. The transfer is invalid.
        if (fromPlayer != GetOwner())
        {
            UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Player attempting to move items from another player's inventory!"),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            return;
        }
    }
    // This player is trying to modify a player-owned inventory.
    if (IsValid(toPlayer))
    {
        // The *TO* inventory is not this player. The transfer is invalid.
        if (toPlayer != GetOwner())
        {
            UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Player attempting to move items from another player's inventory!"),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            return;
        }
    }

    const FStInventorySlot FromInventorySlot = fromInventory->GetCopyOfSlot(fromSlot, isFromEquipSlot);
    
    // If it's the same inventory
    if (fromInventory == toInventory)
    {
        // The operation is NOT taking place within the client's own inventory.
        if (fromInventory != this)
        {
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Same inventory, but the player isn't the owner."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            
            // Validate distance
            if (fromOwner->GetDistanceTo(toOwner) > 1024.0f)
            {
                UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Operation was Rejected (Distance too Far)"),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Moving items within the player's own inventory component."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
        }
        
        // If origin is inventory and destination is equipment, perform a DonEquipment
        if (!isFromEquipSlot && isToEquipSlot)
        {
            if (!fromInventory->IsValidSlot(fromSlot))
            {
                UE_LOG(LogTemp, Warning, TEXT("TransferItems(%s): From slot specified as inventory but was not a valid inventory slot."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
            if (!toInventory->isValidEquipmentSlot(toSlot))
            {
                UE_LOG(LogTemp, Warning, TEXT("TransferItems(%s): To slot specified as equipment but was not a valid equipment slot."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Moving Inventory Item into Equipment Slot."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            if (!toInventory->donEquipment(fromInventory, fromSlot, toInventory->getEquipmentSlotType(toSlot)))
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Failed to execute donEquipment()"),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            }
        }
        
        // If origin is equipment and destination is inventory, perform a DoffEquipment
        else if (isFromEquipSlot && !isToEquipSlot)
        {
            const EEquipmentSlotType fromEquipSlot = fromInventory->getEquipmentSlotType(fromSlot);
            if (!fromInventory->isValidEquipmentSlot(fromSlot))
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): From slot specified as equipment but was not a valid equipment slot."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
            if (!toInventory->IsValidSlot(toSlot))
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): To slot specified as inventory but was not a valid inventory slot."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
            
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Moving Equipment Item into Inventory Slot."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            if (!fromInventory->doffEquipment(fromEquipSlot, toSlot, toInventory, false))
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Failed to execute doffEquipment()."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            }
        }
        
        // Both items are equipment or both items are non-equipment. Perform swap-or-stack.
        else
        {
            if (!toInventory->swapOrStackSlots(fromInventory,fromSlot,toSlot,moveQty,false,isToEquipSlot,isFromEquipSlot))
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): swapOrStack() Failed."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            }
        }
    }
    
    // If they are different inventories, first verify that the player is allowed to do it.
    else
    {
        UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Moving items between different inventory components."),
            (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
        
        if (fromOwner->GetDistanceTo(toOwner) > 1024.0f)
        {
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): Operation was Rejected (Distance too Far)"),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            return;
        }
        
        // Make a copy of the origin item, and decrease it from the origin inventory.
        const FStItemData originItem = FromInventorySlot.GetItemData();
        if (UItemSystem::getItemNameIsValid(FromInventorySlot.ItemName))
        {
            // Remove the items from the origin inventory
            moveQty = fromInventory->decreaseQuantityInSlot(fromSlot,moveQty,isFromEquipSlot);
            if (moveQty < 1)
            {
                UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Failed to remove any items from origin inventory."),
                    (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
                return;
            }
        }
        
        // If decrease was successful, add it to the destination inventory.
        const int itemsAdded = toInventory->AddItemFromDataTable(FromInventorySlot.ItemName, moveQty, toSlot,
                    false,false,true);
        if (itemsAdded > 0)
        {
            UE_LOG(LogTemp, Display, TEXT("TransferItems(%s): The item inventory transfer was successful."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
        }
        
        // If the add failed, reinstate the item to the origin inventory
        else
        {
            UE_LOG(LogTemp, Error, TEXT("TransferItems(%s): Failed to add item. Reinstating the original item."),
                (GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI")));
            fromInventory->AddItemFromExistingSlot(FromInventorySlot, fromSlot,moveQty,true,false,false);
        }
    }
}


void UInventoryComponent::Server_DropItemOnGround_Implementation(UInventoryComponent* fromInventory, int fromSlot,
    int quantity, bool isFromEquipSlot)
{
    UE_LOG(LogTemp, Display, TEXT("InventoryComponent: Server_DropItemOnGround_Implementation"));
    if (!IsValid(fromInventory)) fromInventory = this;

    FStInventorySlot FromInventorySlot = fromInventory->GetInventorySlot(fromSlot,isFromEquipSlot);
    if (FromInventorySlot.IsSlotEmpty())
        return;
    
    AActor* FromActor = fromInventory->GetOwner();
    if (!IsValid(FromActor)) return; // Just in case something went terribly wrong.

    AActor* ToActor = Cast<AActor>(GetOwner());
    if (!IsValid(ToActor)) return; // Safety catch, should never be possible
    
    // If the inventories belong to different actors, make sure it's not player v. player
    if (ToActor != FromActor)
    {
        AController* thisController = FromActor->GetInstigatorController();
        AController* thatController = FromActor->GetInstigatorController();

        // If both actors are controlled, we may have a conflict
        if (IsValid(thisController) && IsValid(thatController))
        {
            // Player trying to access another player's inventory. Deny.
            if (thisController->IsPlayerController() && thatController->IsPlayerController())
            {
                return;
            }
        }
    }

    ACharacter* thisCharacter = Cast<ACharacter>(ToActor);
    if (IsValid(thisCharacter))
    {
        USkeletalMeshComponent* thisMesh = thisCharacter->GetMesh();
        if (!IsValid(thisMesh)) return;
        
        FTransform spawnTransform(
            FRotator(FMath::RandRange(0.f,359.f),FMath::RandRange(0.f,359.f),FMath::RandRange(0.f,359.f)),
            thisMesh->GetBoneLocation("Root"));
        FActorSpawnParameters spawnParams;
        spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        FStInventorySlot InventorySlot = fromInventory->GetCopyOfSlot(fromSlot, isFromEquipSlot);
        FStItemData itemCopy    = InventorySlot.GetItemData();
        int itemQuantity        = InventorySlot.SlotQuantity;
        int tossQuantity        = (quantity > itemQuantity) ? itemQuantity : abs(quantity);

        APickupActorBase* pickupItem = GetWorld()->SpawnActorDeferred<APickupActorBase>(
                                                APickupActorBase::StaticClass(), spawnTransform);
        if (IsValid(pickupItem))
        {
            if (fromInventory->decreaseQuantityInSlot(fromSlot, tossQuantity) > 0)
            {
                pickupItem->SetupItemFromName(InventorySlot.ItemName, tossQuantity);
                pickupItem->FinishSpawning(spawnTransform);
            }
        }
        
    }
    
}

void UInventoryComponent::Server_RequestOtherInventory_Implementation(UInventoryComponent* targetInventory)
{
    UE_LOG(LogTemp, Display, TEXT("RequestOtherInventory() Received (SERVER EVENT) - Player wants to load an inventory"));

    AActor* thisActor = GetOwner();
    if (!IsValid(thisActor)) return;

    // This should never be invalid because the player is the one calling this function.
    const ACharacter* thisPlayer = Cast<ACharacter>(thisActor);
    if (!IsValid(thisPlayer)) return;
    
    if (!IsValid(targetInventory)) targetInventory = this;
    if (targetInventory == this)
    {
        const APlayerState* thisPlayerState = thisPlayer->GetPlayerState();
        UE_LOG(LogTemp, Display, TEXT("Player '%d' tried to request their own inventory, or target inventory was invalid"),
            IsValid(thisPlayerState) ? thisPlayerState->GetPlayerId() : 0);
        return;
    }
    
    // Validate the components, and ensure it isn't another player.
    AActor* ownerActor = targetInventory->GetOwner();
    if (!IsValid(ownerActor)) return; // Should never happen but doesn't let the server crash

    // If this cast comes back valid, the owner the player is trying to access is owned
    // by another player. This should NEVER be allowed.
    const ACharacter* playerRef = Cast<ACharacter>(ownerActor);
    if (IsValid(playerRef))
    {
        const APlayerState* thisPlayerState   = thisPlayer->GetPlayerState();
        const APlayerState* ownerState        = playerRef->GetPlayerState();
        UE_LOG(LogTemp, Warning, TEXT("Player '%d' tried to request the inventory of Player '%d'!!!"),
            IsValid(thisPlayerState) ? thisPlayerState->GetPlayerId() : 0,   // The player running the func
            IsValid(ownerState)      ? ownerState->GetPlayerId()      : 0 ); // The player being accessed
    }

    // at this point we know both references and their inventories are valid
    // and the player isn't accessing another player's inventory.

    // If the inventory's actor is owned, the player can't use it. THis means that it's already in use.
    if (IsValid(ownerActor->GetOwner()))
    {
        const APlayerState* thisPlayerState = thisPlayer->GetPlayerState();
        UE_LOG(LogTemp, Display, TEXT("Player '%d' tried to access the inventory of '%s', but it is already in use."),
            IsValid(thisPlayerState) ? thisPlayerState->GetPlayerId() : 0,   // The player running the func
            *GetOwner()->GetName());

        //TODO - Send a message to the player notifying them that the inventory is in use by another player.
        
    }

    // Set the accessing player as the owner of the inventory's actor.
    // This should enable replication to the player who is using the inventory.
    ownerActor->SetOwner(thisActor);
    FString ownerActorName = "NULL";
    if (IsValid(ownerActor->GetOwner()))
        ownerActorName = ownerActor->GetOwner()->GetName();
    UE_LOG(LogTemp, Display, TEXT("%s(%s): My owner is now %s"),
        *GetName(), GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI"), *ownerActorName);
    
}

void UInventoryComponent::Server_StopUseOtherInventory_Implementation(UInventoryComponent* otherInventory)
{
    if (IsValid(otherInventory))
    {
        AActor* ownerActor = otherInventory->GetOwner();
        if(IsValid(ownerActor))
        {
            ownerActor->SetOwner(nullptr);
            FString ownerActorName = "NULL";
            if (IsValid(ownerActor->GetOwner()))
                ownerActorName = ownerActor->GetOwner()->GetName();
            UE_LOG(LogTemp, Display, TEXT("%s(%s): My owner is now %s"),
                *GetName(), GetOwner()->HasAuthority()?TEXT("SRV"):TEXT("CLI"), *ownerActorName);
        }
    }
}


/****************************************
 * REPLICATION
***************************************/

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Update to owner only. When someone opens something like a storage chest, they'll be set to the owner.
    DOREPLIFETIME(UInventoryComponent, m_inventorySlots);
    DOREPLIFETIME(UInventoryComponent, m_equipmentSlots);
}