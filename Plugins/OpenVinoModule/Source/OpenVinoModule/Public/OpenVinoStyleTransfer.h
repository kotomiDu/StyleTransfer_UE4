// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OpenVinoStyleTransfer.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStyleTransfer, Log, All);

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
		bool Initialize(FString xmlFilePath, FString binFilePath, FString& retLog);

	UFUNCTION(BlueprintCallable, Category = "OpenVINO Plugin")
		void Release();

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

	// transfer style to utexture
	// if texture created, return true
	// else return false
	bool StyleTransferToTexture(UObject* Outer, TArray<FColor>& TextureData, int Inwidth, int Inheight);

	// bind callback for frame buffer capture
	void BindBackbufferCallback();
	void UnBindBackbufferCallback();

	// on resize output width/height
	bool OnResizeOutput(int width, int height);

	/**
	 * @brief Returns last error from OpenVino, logging it first to UE's log system
	 * @return Last error message
	 */
	FString GetAndLogLastError();

	/** Holds the future value which represents the asynchronous loading operation. */
	TFuture<FColor*> Future;

	// output
	IConsoleVariable* transfer_width;
	IConsoleVariable* transfer_height;
	int last_out_width;
	int last_out_height;

	// input
	FVector2D input_origin;
	FIntPoint last_input_size;
	FIntPoint input_size;

	bool debug_flag;

	TArray<FColor> fb_data;
	TArray<BYTE> buffer;
	TArray<BYTE> tmp_buffer;
	TArray<FColor> rgba_buffer;

	UTexture2D* out_tex;

	class SStyleTransferResultDialog* dialog;
	SWindow* window;

	FString xml_file_path;
	FString bin_file_path;

public:
	FDelegateHandle m_OnBackBufferReadyToPresent;

	void OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer);
};

//////////////////////////////////////////////////////////////////////////
// SPaperExtractSpritesDialog

class SStyleTransferResultDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStyleTransferResultDialog){}
	SLATE_END_ARGS()

	// Constructs this widget with InArgs
	void Construct(const FArguments& InArgs);

	virtual ~SStyleTransferResultDialog();

	// Show the dialog, returns true if successfully extracted sprites
	static SStyleTransferResultDialog* ShowWindow(int outputWidth, int outputHeight, SWindow*& showWindow);

	void UpdateTexture(UTexture2D* SourceTexture);

private:
	SImage* image;

	TSharedPtr<FSlateDynamicImageBrush> ImageBrush;
};
