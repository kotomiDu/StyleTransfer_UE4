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


/**
 * @brief starts style transfer process, will fire an event once style transfer has completed
 * @param filePath image file path to be analysed
 * @return message saying that classification has started
 */
FString
UOpenVinoStyleTransfer::BeginStyleTransferFromPath(UObject* Outer, FString filePath)
{
	//Future = Async(EAsyncExecution::ThreadPool, [=]()GetTextureFromStyleTransfer(Outer, filePath));
	TPromise<UTexture2D*> Result;
	FString resultlog;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Outer, filePath, &resultlog, &Result]()
	{
		
		
		if (!TestFileExists(filePath))
		{
			resultlog = TEXT("File Path has not been passed!");
		}
		else
		{
			// This is the buffer for result, with arbitrary size 1024: 
			vector<char> message(1024, '\0');

			int width, height;
			float* out;
			int arraysize = 1280 * 720 * 3;
			out = (float*)malloc(arraysize * sizeof(float));
			auto ret = OpenVino_Infer_FromTexture(
				TCHAR_TO_ANSI(*filePath), &width, &height, out);

			TArray<uint8> rawdata;

			int idx = 0;
			while (idx < arraysize)
			{
				rawdata.Add(static_cast<uint8>(*out));
				idx++;
				out++;
			}
			
			FString TextureBaseName = TEXT("Texture_") + FPaths::GetBaseFilename("test");
			UTexture2D* temp = CreateTexture(Outer, rawdata, width, height, EPixelFormat::PF_B8G8R8A8, FName(*TextureBaseName));
			Result.SetValue(temp);

			if (!ret)
			{
				resultlog = TEXT("ERRO");//GetAndLogLastError();
			}
				
			else
			{
				resultlog = "value:" + FString::FromInt(height);
			}
				
		}

		// Broadcast event when classification has completed
	});

	Future = Result.GetFuture();
	if (Future.IsValid())
	{
		this->OnStyleTransferComplete.Broadcast(resultlog, Future.Get());
	}

	return TEXT("Starting Style Transfer...");
}

/*UTexture2D*
UOpenVinoStyleTransfer::GetTextureFromStyleTransfer(UObject* Outer, FString filePath)
{
	FString result;

	if (!TestFileExists(filePath))
	{
		result = TEXT("File Path has not been passed!");
	}
	else
	{
		// This is the buffer for result, with arbitrary size 1024: 
		vector<char> message(1024, '\0');

		int width, height;
		float* out;
		int arraysize = 1280 * 720 * 3;
		out = (float*)malloc(arraysize * sizeof(float));
		auto ret = OpenVino_Infer_FromTexture(
			TCHAR_TO_ANSI(*filePath), &width, &height, out);

		TArray<uint8> rawdata;

		int idx = 0;
		while (idx < arraysize)
		{
			rawdata.Add(static_cast<uint8>(*out));
			idx++;
			out++;
		}

		FString TextureBaseName = TEXT("Texture_") + FPaths::GetBaseFilename("test");
		return CreateTexture(Outer, rawdata, width, height, EPixelFormat::PF_B8G8R8A8, FName(*TextureBaseName));
	}
}
*/
UTexture2D* UOpenVinoStyleTransfer::CreateTexture(UObject* Outer, TArray<uint8>& PixelData, int32 InSizeX, int32 InSizeY, EPixelFormat InFormat, FName BaseName)
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
	FMemory::Memcpy(TextureData, PixelData.GetData(), PixelData.Num());
	Mip->BulkData.Unlock();

	NewTexture->UpdateResource();
	UE_LOG(LogTemp,Warning, TEXT("reachable %d"), NewTexture->IsUnreachable());
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

// Called when the game starts
void UOpenVinoStyleTransfer::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UOpenVinoStyleTransfer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

