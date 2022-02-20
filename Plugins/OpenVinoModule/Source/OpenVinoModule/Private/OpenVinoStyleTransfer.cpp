// Fill out your copyright notice in the Description page of Project Settings.


#include "OpenVinoStyleTransfer.h"
#include "ThirdParty\OpenVinoWrapper\OpenVinoWrapper.h"

#include <vector>
#include <string>
#include "ImageUtils.h"
#include "Slate/SceneViewport.h"
using namespace std;

// open vino style transfer width
static TAutoConsoleVariable<int32> CVarTransferWidth(
	TEXT("r.OVST.Width"),
	224,
	TEXT("Set Openvino Style transfer rect width."));

static TAutoConsoleVariable<int32> CVarTransferHeight(
	TEXT("r.OVST.Height"),
	224,
	TEXT("Set Openvino Style transfer rect height."));


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
	: transfer_width(nullptr)
	, transfer_height(nullptr)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// Set initialize width/height
	transfer_width = IConsoleManager::Get().FindConsoleVariable(TEXT("r.OVST.Width"));
	transfer_height = IConsoleManager::Get().FindConsoleVariable(TEXT("r.OVST.Height"));

	last_width = transfer_width->GetInt();
	last_height = transfer_height->GetInt();
}

/**
 * @brief Initializes OpenVino
 * @param xmlFilePath
 * @param binFilePath
 * @param labelsFilePath
 * @return message saying that plugin has been initialized or not
 */
bool
UOpenVinoStyleTransfer::Initialize(
	FString xmlFilePath,
	FString binFilePath,
	FString labelsFilePath,
	FString& retLog)
{
	// First, test if files passed exist, it is better to catch it early:
	if (!TestFileExists(xmlFilePath) ||
		!TestFileExists(binFilePath) ||
		!TestFileExists(labelsFilePath))
	{
		retLog = TEXT("One or more files passed to Initialize don't exit");
		return false;
	}

	auto ret = OpenVino_Initialize(
		TCHAR_TO_ANSI(*xmlFilePath),
		TCHAR_TO_ANSI(*binFilePath),
		TCHAR_TO_ANSI(*labelsFilePath));

	if (!ret)
	{
		retLog = TEXT("OpenVino has failed to initialize: ") ;
	}
	else
	{
		retLog = TEXT("OpenVino has been initialized.");
	}
	return ret;
}

UTexture2D* UOpenVinoStyleTransfer::GetTransferedTexture()
{
	return out_tex;
}

void UOpenVinoStyleTransfer::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// begin transfer from texture
	BeginStyleTransferFromTexture(this, fb_data, last_width, last_height);

	// write back to framebuffer
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
	// Read the size we need
	int width = transfer_width->GetInt();
	int height = transfer_height->GetInt();

	if (width == 0 || height == 0)
		return;

	if (buffer.Num() == 0 || last_width != width || last_height != height)
	{
		size_t buffer_size = width * height * 3;
		buffer.Reset(buffer_size);
		buffer.SetNum(buffer_size);
		tmp_buffer.Reset(buffer_size);
		tmp_buffer.SetNum(buffer_size);
		rgba_buffer.Reset(width * height);
		rgba_buffer.SetNum(width * height);
	}
	last_width = width;
	last_height = height;

	UGameViewportClient* gameViewport = GetWorld()->GetGameViewport();
	FSceneViewport* vp = gameViewport->GetGameViewport();
	SWindow* gameWin = gameViewport->GetWindow().Get();

	if (gameWin->GetId() != SlateWindow.GetId())
		return;

	FVector2D gameWinPos = gameWin->GetPositionInScreen();
	FVector2D vpPos = vp->GetCachedGeometry().GetAbsolutePosition();
	FVector2D origin = vpPos - gameWinPos;

	FIntRect Rect(origin.X, origin.Y, origin.X + width, origin.Y + height);

	FTexture2DRHIRef GameBuffer = BackBuffer;
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	// Get out data
	RHICmdList.ReadSurfaceData(GameBuffer, Rect, fb_data, FReadSurfaceDataFlags(RCM_UNorm));
}

void UOpenVinoStyleTransfer::BeginStyleTransferFromTexture(UObject* Outer, TArray<FColor>& TextureData, int inwidth, int inheight)
{
	TPromise<FColor*> Result;
	FString resultlog;
	int outWidth, outHeight;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Outer, TextureData, inwidth, inheight, &resultlog, &outWidth, &outHeight, &Result, this]()
		{
			
			int index = 0;
			for (FColor color : TextureData)
			{
				tmp_buffer[index] = color.B;
				tmp_buffer[index + 1] = color.G;
				tmp_buffer[index + 2] = color.R;
				index = index + 3;
			}
			if( OpenVino_Infer_FromTexture(tmp_buffer.GetData(), inwidth, inheight, &outWidth, &outHeight, buffer.GetData()) )
			{
				index = 0;
				for (FColor color : rgba_buffer)
				{
					color.B = buffer[index];
					color.G = buffer[index + 1];
					color.R = buffer[index + 2];
					color.A = 255;
					index = index + 3;
				}
				Result.SetValue(rgba_buffer.GetData());
				resultlog = FString::Format(TEXT("Success:Width({0}), Height({1})"), { FString::FromInt(outWidth), FString::FromInt(outHeight) });
			}
			else
			{
				Result.SetValue(nullptr);
				resultlog = GetAndLogLastError();
			}
		}
	);
	
	Future = Result.GetFuture();
	if (Future.IsValid())
	{
		FColor* transfered = Future.Get();
		if (transfered != nullptr)
		{
			if (out_tex == nullptr || outWidth != out_tex->GetSizeX() || outHeight != out_tex->GetSizeY())
			{
				if (out_tex != nullptr)
				{
					out_tex->RemoveFromRoot();
					out_tex->ConditionalBeginDestroy();
					out_tex = nullptr;
				}
				out_tex = CreateTexture(transfered, outWidth, outHeight);
			}
			else
			{
				// update
				UpdateTexture(out_tex, transfered);
			}
			this->OnStyleTransferComplete.Broadcast(resultlog, out_tex);
		}
		else
		{
			this->OnStyleTransferComplete.Broadcast(resultlog, nullptr);
		}
	}
}

UTexture2D* UOpenVinoStyleTransfer::CreateTexture(FColor* data, int width, int height)
{
	UTexture2D* Texture;

	Texture = UTexture2D::CreateTransient(width, height, PF_B8G8R8A8);
	if (!Texture)
	{
		return nullptr;
	}

#if WITH_EDITORONLY_DATA
	Texture->MipGenSettings = TMGS_NoMipmaps;
#endif
	Texture->NeverStream = true;

	Texture->SRGB = 0;

	FTexture2DMipMap& Mip = Texture->PlatformData->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);

	FMemory::Memcpy(Data, data, width * height * 4);
	Mip.BulkData.Unlock();
	Texture->UpdateResource();

	return Texture;
}

void UOpenVinoStyleTransfer::UpdateTexture(UTexture2D* tex, FColor* data)
{
	FTexture2DMipMap& Mip = tex->PlatformData->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);

	FMemory::Memcpy(Data, data, tex->GetSizeX() * tex->GetSizeY() * 4);
	Mip.BulkData.Unlock();
	tex->UpdateResource();
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
		vector<char> last_error(256, '\0');
		auto ret = OpenVino_GetLastError(last_error.data(), last_error.size());

		if (ret)
			lastError = FString(last_error.data());
		else
			lastError = TEXT("Failed to read OpenVino_GetLastError");
	}

	UE_LOG(LogTemp, Error, TEXT("OpenVino_GetLastError: %s"), *lastError);

	return lastError;
}
