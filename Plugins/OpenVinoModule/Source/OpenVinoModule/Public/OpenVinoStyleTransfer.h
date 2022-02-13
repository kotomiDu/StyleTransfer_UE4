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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenVINO Plugin")
	UTexture2D* resTexture;

	/**
	 * @brief Initializes OpenVino
	 * @param xmlFilePath
	 * @param binFilePath
	 * @param labelsFilePath (for classification)
	 * @return message saying that plugin has been initialized or not
	 */
	UFUNCTION(BlueprintCallable, Category = "OpenVINO Plugin")
		FString Initialize(FString xmlFilePath, FString binFilePath, FString labelsFilePath);

	/**
	* @brief Style Transfer from Texture rendered by Engine
	* @param Outer 
	* @param TextureData, Texture rendered by Engine
	* @param Inwidth, width of Texture
	* @param Inheight, height of Texture
	* @return message saying that the style transfer begins
	*/
	UFUNCTION(BlueprintCallable, meta = (HidePin = "Outer", DefaultToSelf = "Outer"), Category = "OpenVINO Plugin")
		FString BeginStyleTransferFromTexture(UObject* Outer, TArray<FColor>& TextureData, int Inwidth, int Inheight);
	/**
	* @brief This Blueprint event will be fired once classification has completed
	* @param classification result
	*/
	UPROPERTY(BlueprintAssignable, Category = "OpenVINO Plugin")
		FStyleTransferComplete OnStyleTransferComplete;
private:

	/** Helper function to dynamically create a new texture from raw pixel data. */
	static UTexture2D*  CreateTexture(UObject* Outer, unsigned char* PixelData, int32 InSizeX, int32 InSizeY, EPixelFormat InFormat, FName BaseName);

	/**
	 * @brief Returns last error from OpenVino, logging it first to UE's log system
	 * @return Last error message
	 */
	FString
		GetAndLogLastError();

	/** Holds the future value which represents the asynchronous loading operation. */
	TFuture<UTexture2D*> Future;

	/*UTexture2D* GetTextureFromStyleTransfer(UObject* Outer, FString filePath);*/


public:
	FDelegateHandle m_OnBackBufferReadyToPresent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenVINO Plugin")
		UTexture2D* OutTex;

	void OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer);

	//注册获取BackBuffer数据的回调函数
	UFUNCTION(BlueprintCallable, Category = "OpenVINO Plugin")
		void BindBackbufferCallback();

	//解绑获取BackBuffer数据的回调函数
	UFUNCTION(BlueprintCallable, Category = "OpenVINO Plugin")
		void UnBindBackbufferCallback();
};
