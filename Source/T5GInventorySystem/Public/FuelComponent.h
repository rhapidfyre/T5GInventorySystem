
#pragma once

#include "CoreMinimal.h"
#include "InventoryComponent.h"
#include "Components/ActorComponent.h"
#include "lib/ItemData.h"

#include "FuelComponent.generated.h"

// Called whenever the fuel contents are consumed or modified
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFuelUpdated);

// Called whenever the fuel system is started or stopped
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

	UFUNCTION(BlueprintCallable)
	void InitializeFuelSystem();

	/**
	 * Server Only. Sets the rate at how fast fuel burns. >1 burns fuel faster.
	 * <1 burns slower. Any value of zero or less will result in no fuel consumption.
	 * @param inRate The new rate of fuel burn.
	 */
	UFUNCTION(BlueprintCallable)
	void SetFuelConsumeRate(float inRate = 1.f);

	virtual void OnComponentCreated() override;

	UPROPERTY(BlueprintAssignable, Category = "Fuel Events")
	FOnFuelUpdated OnFuelUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Fuel Events")
	FOnFuelSystemToggled OnFuelSystemToggled;

	UFUNCTION(BlueprintCallable)
	bool StartFuelSystem();
	
	UFUNCTION(BlueprintCallable)
	bool StopFuelSystem();

	UFUNCTION(BlueprintPure)
	bool IsReserveFuelAvailable();

	UFUNCTION(BlueprintCallable)
	bool RemoveFuel();

	/**
	 * Returns the total quantity of fuel items available
	 * @return An integer representing quantity of fuel available
	 */
	UFUNCTION(BlueprintPure)
	int GetTotalFuelItemsAvailable();

	/**
	 * Returns the seconds remaining on the current fuel item being consumed
	 * @return Float representing seconds remaining of current fuel
	 */
	UFUNCTION(BlueprintPure)
	float GetCurrentFuelTimeRemaining() const { return mTimeRemaining; }

	/**
	 * Returns the current item being used for fuel
	 * @return FName with item name of current fuel
	 */
	UFUNCTION(BlueprintPure)
	FName GetCurrentFuelItem() const { return mCurrentFuelItem; }

	/**
	 * Returns the total amount of time given all fuel items available.
	 * @return FTimespan with total time remaining in fuel
	 */
	UFUNCTION(BlueprintPure)
	FTimespan GetTotalFuelTimeAvailable();

	UFUNCTION(BlueprintPure)
	bool IsFuelAvailable();

	UFUNCTION(BlueprintPure)
	bool IsFuelSystemRunning() const { return bIsRunning; }

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> FuelItemsAllowed;
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void CheckForConsumption();

	bool ConsumeQueuedItem();

	void CreateByProduct(bool &isOverflowing);
	
private:

	void SetupDefaults();
	
	UPROPERTY() UInventoryComponent* mInventoryStatic;
	UPROPERTY() UInventoryComponent* mInventoryFuel;

	UPROPERTY(Replicated) bool bIsRunning = false;
	
	bool bIsFuelSystemReady = false;
	
	UPROPERTY() FTimerHandle mFuelTimer;

	UFUNCTION(Client, Reliable) void OnRep_CurrentFuelItem();
	UPROPERTY(Replicated, ReplicatedUsing=OnRep_CurrentFuelItem)
	FName mCurrentFuelItem = UItemSystem::getInvalidName();

	// Seconds remaining until fuel is consumed
	UPROPERTY(Replicated)
	float mTimeRemaining = 0.f;
	
	UPROPERTY(Replicated) float mTickRate = 1.f;
	UPROPERTY() float mTimerTick = 1.f;
	
	UPROPERTY() bool bIgnoreFuel = false;
	UPROPERTY() bool bOverflowShutoff = true;

	bool bShowDebug = false;

	TArray<FName> mAuthorizedFuel;
	
};
