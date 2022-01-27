// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OpenVinoStyleTransfer.generated.h"

/** Delegate for the event fired when style transfer completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStyleTransferComplete, const FString&, result, UTexture2D*, StyledTexture);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class OPENVINOMODULE_API UOpenVinoStyleTransfer : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UOpenVinoStyleTransfer();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* resTexture;

	/**
 * @brief Initializes OpenVino
 * @param xmlFilePath
 * @param binFilePath
 * @param labelsFilePath
 * @return message saying that plugin has been initialized or not
 */
	UFUNCTION(BlueprintCallable)
		FString Initialize(FString xmlFilePath, FString binFilePath, FString labelsFilePath);

	/**
	 * @brief starts classification process, will fire an event once classification has completed
	 * @param filePath image file path to be analysed
	 * @return message saying that classification has started
	 */
	UFUNCTION(BlueprintCallable, meta = (HidePin = "Outer", DefaultToSelf = "Outer"))
		FString BeginStyleTransferFromPath(UObject* Outer, FString filePath);
private:

	/** Helper function to dynamically create a new texture from raw pixel data. */
	static UTexture2D*  CreateTexture(UObject* Outer, TArray<uint8>& PixelData, int32 InSizeX, int32 InSizeY, EPixelFormat InFormat, FName BaseName);
public:
	/**
	 * @brief This Blueprint event will be fired once classification has completed
	 * @param classification result
	 */
	UPROPERTY(BlueprintAssignable)
		FStyleTransferComplete OnStyleTransferComplete;
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/**
	 * @brief Returns last error from OpenVino, logging it first to UE's log system
	 * @return Last error message
	 */
	FString
		GetAndLogLastError();

	/** Holds the future value which represents the asynchronous loading operation. */
	TFuture<UTexture2D*> Future;

	/*UTexture2D* GetTextureFromStyleTransfer(UObject* Outer, FString filePath);*/



		
};
