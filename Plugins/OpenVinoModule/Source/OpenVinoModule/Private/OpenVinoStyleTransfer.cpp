// Fill out your copyright notice in the Description page of Project Settings.


#include "OpenVinoStyleTransfer.h"
#include "ThirdParty\OpenVinoWrapper\OpenVinoWrapper.h"

#include <vector>
#include <string>
#include "ImageUtils.h"
using namespace std;


/*
 * @brief Tests if file passed exists, and logs error if it doesn't
 * @param filePath to be tested
 * @return true if file exists, false if it doesn't
 */
static
bool TestFileExists(FString filePath)
{
	if (IFileManager::Get().FileExists(*filePath))
		return true;

	UE_LOG(LogTemp, Error, TEXT("Could not find file: %s"), *filePath);

	return false;
}


// Sets default values for this component's properties
UOpenVinoStyleTransfer::UOpenVinoStyleTransfer()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

/**
 * @brief Initializes OpenVino
 * @param xmlFilePath
 * @param binFilePath
 * @param labelsFilePath
 * @return message saying that plugin has been initialized or not
 */
FString
UOpenVinoStyleTransfer::Initialize(
	FString xmlFilePath,
	FString binFilePath,
	FString labelsFilePath)
{
	// First, test if files passed exist, it is better to catch it early:
	if (!TestFileExists(xmlFilePath) ||
		!TestFileExists(binFilePath) ||
		!TestFileExists(labelsFilePath))
		return TEXT("One or more files passed to Initialize don't exit");

	auto ret = OpenVino_Initialize(
		TCHAR_TO_ANSI(*xmlFilePath),
		TCHAR_TO_ANSI(*binFilePath),
		TCHAR_TO_ANSI(*labelsFilePath));

	if (!ret)
	{
		// If initialization fails, log the error message:
		//auto message = GetAndLogLastError();

		return TEXT("OpenVino has failed to initialize: ") ;
	}

	return TEXT("OpenVino has been initialized.");
}

void  UOpenVinoStyleTransfer::BindBackbufferCallback()
{

	if (FSlateApplication::IsInitialized())
	{
		m_OnBackBufferReadyToPresent.Reset();

		m_OnBackBufferReadyToPresent = FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddUObject(this, &UOpenVinoStyleTransfer::OnBackBufferReady_RenderThread);
	}
}

void UOpenVinoStyleTransfer::UnBindBackbufferCallback()
{
	if (m_OnBackBufferReadyToPresent.IsValid())
	{
		FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(m_OnBackBufferReadyToPresent);
	}
}


void UOpenVinoStyleTransfer::OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer)
{

	//UE_LOG(LogTemp, Log, TEXT("m1"));
	FTexture2DRHIRef GameBuffer = BackBuffer;
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FIntRect Rect(0, 0, GameBuffer->GetSizeX(), GameBuffer->GetSizeY());
	TArray<FColor> outData;
	RHICmdList.ReadSurfaceData(GameBuffer, Rect, outData, FReadSurfaceDataFlags(RCM_UNorm));
	// outData is the image data of the overall rendering scene

	// if not set the aphla, the image will be transparent
	for (FColor& color : outData)
	{
		color.A = 255;
	}
	BeginStyleTransferFromTexture(this, outData, Rect.Width(), Rect.Height());
}


/**
* @brief Style Transfer from Texture rendered by Engine
* @param Outer
* @param TextureData, Texture rendered by Engine
* @param Inwidth, width of Texture
* @param Inheight, height of Texture
* @return message saying that the style transfer begins
*/
FString
UOpenVinoStyleTransfer::BeginStyleTransferFromTexture(UObject* Outer, TArray<FColor>& TextureData, int inwidth, int inheight)
{
	TPromise<UTexture2D*> Result;
	FString resultlog;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Outer, TextureData, inwidth, inheight, &resultlog, &Result, this]()
	{

		int outwidth, outheight;
		unsigned char* out;
		int arraysize = 224 * 224 * 3;
		out = (unsigned char*)malloc(arraysize * sizeof(unsigned char));
		unsigned char* inputdata;
		inputdata = (unsigned char*)malloc(inwidth * inheight * 3 * sizeof(unsigned char));
		int index = 0;
		for (FColor color : TextureData)
		{
			inputdata[index] = color.R ;
			inputdata[index + 1] =color.G ;
			inputdata[index + 2] = color.B ;
			index = index + 3;
		}
		
		auto ret = OpenVino_Infer_FromTexture(
			inputdata, inwidth, inheight, &outwidth, &outheight, out);

		FString TextureBaseName = TEXT("Texture_") + FPaths::GetBaseFilename("test");
		UTexture2D* temp = CreateTexture(Outer, out, outwidth, outheight, EPixelFormat::PF_B8G8R8A8, FName(*TextureBaseName));
		Result.SetValue(temp);
		if (!ret)
		{
			resultlog = TEXT("ERRO");//GetAndLogLastError();
		}

		else
		{
			resultlog = TEXT("value:");// +FString::FromInt(outheight);
		}

	}
	);
	
	Future = Result.GetFuture();
	if (Future.IsValid())
	{
		this->OnStyleTransferComplete.Broadcast(resultlog, Future.Get());
	}

	return TEXT("Starting Style Transfer...");
}


UTexture2D* UOpenVinoStyleTransfer::CreateTexture(UObject* Outer, unsigned char* PixelData, int32 InSizeX, int32 InSizeY, EPixelFormat InFormat, FName BaseName)
{
	// Shamelessly copied from UTexture2D::CreateTransient with a few modifications
	if (InSizeX <= 0 || InSizeY <= 0 ||
		(InSizeX % GPixelFormats[InFormat].BlockSizeX) != 0 ||
		(InSizeY % GPixelFormats[InFormat].BlockSizeY) != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid parameters specified for UImageLoader::CreateTexture()"));
		return nullptr;
	}

	// Most important difference with UTexture2D::CreateTransient: we provide the new texture with a name and an owner
	FName TextureName = MakeUniqueObjectName(Outer, UTexture2D::StaticClass(), BaseName);
	UTexture2D* NewTexture = NewObject<UTexture2D>(Outer, TextureName, RF_Transient);

	NewTexture->PlatformData = new FTexturePlatformData();
	NewTexture->PlatformData->SizeX = InSizeX;
	NewTexture->PlatformData->SizeY = InSizeY;
	NewTexture->PlatformData->PixelFormat = InFormat;

	// Allocate first mipmap and upload the pixel data
	int32 NumBlocksX = InSizeX / GPixelFormats[InFormat].BlockSizeX;
	int32 NumBlocksY = InSizeY / GPixelFormats[InFormat].BlockSizeY;
	FTexture2DMipMap* Mip = new(NewTexture->PlatformData->Mips) FTexture2DMipMap();
	Mip->SizeX = InSizeX;
	Mip->SizeY = InSizeY;
	Mip->BulkData.Lock(LOCK_READ_WRITE);
	void* TextureData = Mip->BulkData.Realloc(NumBlocksX * NumBlocksY * GPixelFormats[InFormat].BlockBytes);
	FMemory::Memcpy(TextureData, PixelData, InSizeX*InSizeY*3);
	Mip->BulkData.Unlock();

	NewTexture->UpdateResource();
	//UE_LOG(LogTemp,Warning, TEXT("reachable %d"), NewTexture->IsUnreachable());
	return NewTexture;
}


/**
 * @brief Returns last error from OpenVino, logging it first to UE's log system
 * @return Last error message
 */
FString
UOpenVinoStyleTransfer::GetAndLogLastError()
{
	FString lastError;

	{
		// Pass in string buffer with arbitrary size (256):
		vector<char> buffer(256, '\0');
		auto ret = OpenVino_GetLastError(buffer.data(), buffer.size());

		if (ret)
			lastError = FString(buffer.data());
		else
			lastError = TEXT("Failed to read OpenVino_GetLastError");
	}

	UE_LOG(LogTemp, Error, TEXT("OpenVino_GetLastError: %s"), *lastError);

	return lastError;
}
