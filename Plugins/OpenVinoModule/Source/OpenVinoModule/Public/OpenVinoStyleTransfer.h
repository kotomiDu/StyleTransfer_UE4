// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OpenVinoStyleTransfer.generated.h"

/** Delegate for the event fired when style transfer completes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStyleTransferComplete, const FString&, result, UTexture2D*, StyleTexture);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class OPENVINOMODULE_API UOpenVinoStyleTransfer : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UOpenVinoStyleTransfer();

	/**
	 * @brief Initializes OpenVino
	 * @param xmlFilePath
	 * @param binFilePath
	 * @param labelsFilePath (for classification)
	 * @return message saying that plugin has been initialized or not
	 */
	UFUNCTION(BlueprintCallable, Category = "OpenVINO Plugin")
		bool Initialize(FString xmlFilePath, FString binFilePath, FString labelsFilePath, FString& retLog);

	UFUNCTION(BlueprintCallable, Category = "OpenVINO Plugin")
		UTexture2D* GetTransferedTexture();

	/**
	* @brief This Blueprint event will be fired once classification has completed
	* @param classification result
	*/
	UPROPERTY(BlueprintAssignable, Category = "OpenVINO Plugin")
		FStyleTransferComplete OnStyleTransferComplete;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction);
private:

	/** Helper function to dynamically create a new texture from raw pixel data. */
	UTexture2D* CreateTexture(FColor* data, int width, int height);
	void UpdateTexture(UTexture2D* tex, FColor* data);

	void BeginStyleTransferFromTexture(UObject* Outer, TArray<FColor>& TextureData, int Inwidth, int Inheight);

	/**
	 * @brief Returns last error from OpenVino, logging it first to UE's log system
	 * @return Last error message
	 */
	FString GetAndLogLastError();

	/** Holds the future value which represents the asynchronous loading operation. */
	TFuture<FColor*> Future;

	/*UTexture2D* GetTextureFromStyleTransfer(UObject* Outer, FString filePath);*/

	IConsoleVariable* transfer_width;
	IConsoleVariable* transfer_height;

	int last_width;
	int last_height;

	TArray<FColor> fb_data;
	TArray<BYTE> buffer;
	TArray<BYTE> tmp_buffer;
	TArray<FColor> rgba_buffer;

	UTexture2D* out_tex;

public:
	FDelegateHandle m_OnBackBufferReadyToPresent;

	void OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer);

	//注册获取BackBuffer数据的回调函数
	UFUNCTION(BlueprintCallable, Category = "OpenVINO Plugin")
		void BindBackbufferCallback();

	//解绑获取BackBuffer数据的回调函数
	UFUNCTION(BlueprintCallable, Category = "OpenVINO Plugin")
		void UnBindBackbufferCallback();
};
