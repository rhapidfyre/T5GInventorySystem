
#pragma once

#include "CoreMinimal.h"
#include "InventoryComponent.h"
#include "Components/ActorComponent.h"
#include "lib/ItemData.h"

#include "FuelComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFuelUpdated, int, slotNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFuelSystemToggled, bool, isRunning);

UCLASS(BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class T5GINVENTORYSYSTEM_API UFuelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	
	// Sets default values for this component's properties
	UFuelComponent();
	
	// Sets default values and loads x1 of 'startingItem' into the system
	UFuelComponent(FName startingItem);

	void InitializeFuelSystem();

	virtual void OnComponentCreated() override;

	UPROPERTY(BlueprintAssignable, Category = "Fuel Events")
	FOnFuelUpdated OnFuelUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Fuel Events")
	FOnFuelSystemToggled OnFuelSystemToggled;

	bool StartFuelSystem();
	
	bool StopFuelSystem();

	bool IsReserveFuelAvailable();

	bool RemoveFuel();

	/**
	 * Returns the total quantity of fuel items available
	 * @return An integer representing quantity of fuel available
	 */
	int GetTotalFuelItemsAvailable();

	/**
	 * Returns the amount of time remaining of the current fuel item being burned
	 * @return FTimespan with time remaining of current fuel
	 */
	FTimespan GetCurrentFuelTimeRemaining();

	/**
	 * Returns the total amount of time given all fuel items available.
	 * @return FTimespan with total time remaining in fuel
	 */
	FTimespan GetTotalFuelTimeAvailable();

	bool IsFuelAvailable();

	/**
	 * Called during construction to assign the inventory pointers.
	 * @param fuelInv The inventory that contains fuel items
	 */
	UFUNCTION(BlueprintCallable)
	void SetFuelInventory(UInventoryComponent* fuelInv);

	/**
	 * Called during construction to assign the inventory pointers.
	 * @param staticInv The output inventory for byproducts
	 */
	UFUNCTION(BlueprintCallable)
	void SetStaticInventory(UInventoryComponent* staticInv);

	/**
	 * An array of all FStItemData by FName that this fuel system allows.
	 * Players will still be able to put anything in it, but only THESE items
	 * will be counted for fuel consumption. Only read once during BeginPlay.
	 * When searching the inventory for fuel, the system will consume
	 * items in order of this array as an order of precedence. 
	 */
	TArray<FName> FuelItemsAllowed;
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void CheckForConsumption();

	bool ConsumeQueuedItem();

	void CreateByProduct(bool &isOverflowing);

	UFUNCTION(NetMulticast, Reliable) void OnRep_IsRunning();
	UFUNCTION(NetMulticast, Reliable) void OnRep_FuelItemData();
	UFUNCTION(NetMulticast, Reliable) void OnRep_FuelQuantity();
	
private:

	void SetupDefaults();
	
	UPROPERTY(Replicated) UInventoryComponent* mInventoryStatic;
	UPROPERTY(Replicated) UInventoryComponent* mInventoryFuel;

	UPROPERTY(Replicated) bool bIsRunning = false;
	bool bIsFuelSystemReady = false;
	
	UPROPERTY() FTimerHandle mFuelTimer;

	UPROPERTY(Replicated) FName mCurrentFuelItem = UItemSystem::getInvalidName();

	// Seconds remaining until fuel is consumed
	UPROPERTY(Replicated) float mTimeRemaining = 0.f;
	
	UPROPERTY(Replicated) float mTickRate = 1.f;
	
	UPROPERTY() bool bIgnoreFuel = false;
	UPROPERTY() bool bOverflowShutoff = true;
	UPROPERTY() bool bStartActivated = false;

	bool bShowDebug = false;
	bool bVerboseOutput = false;
	
};
