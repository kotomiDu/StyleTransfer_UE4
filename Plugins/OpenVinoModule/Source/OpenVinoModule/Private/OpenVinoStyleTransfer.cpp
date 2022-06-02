// Fill out your copyright notice in the Description page of Project Settings.


#include "OpenVinoStyleTransfer.h"
#include "ThirdParty\OpenVinoWrapper\OpenVinoWrapper.h"
#include "EditorStyleSet.h"

#include <vector>
#include <string>
#include "ImageUtils.h"
#include "Slate/SceneViewport.h"
#include "Widgets/Images/SImage.h"
using namespace std;

DEFINE_LOG_CATEGORY(LogStyleTransfer);

// open vino style transfer width
static TAutoConsoleVariable<int32> CVarTransferWidth(
	TEXT("r.OVST.Width"),
	512,
	TEXT("Set Openvino Style transfer rect width."));

static TAutoConsoleVariable<int32> CVarTransferHeight(
	TEXT("r.OVST.Height"),
	512,
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

	UE_LOG(LogStyleTransfer, Error, TEXT("Could not find file: %s"), *filePath);

	return false;
}

static UOpenVinoStyleTransfer* openVinoTransfer = nullptr;
// Sets default values for this component's properties
UOpenVinoStyleTransfer::UOpenVinoStyleTransfer()
	: transfer_mode(nullptr)
	, transfer_width(nullptr)
	, transfer_height(nullptr)
	, debug_flag(false)
	, dialog(nullptr)
	, window(nullptr)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// Set initialize width/height
	transfer_width = IConsoleManager::Get().FindConsoleVariable(TEXT("r.OVST.Width"));
	transfer_height = IConsoleManager::Get().FindConsoleVariable(TEXT("r.OVST.Height"));
	transfer_mode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.OVST.Enabled"));

	input_size.X = input_size.Y = 0;
	last_input_size.X = last_input_size.Y = 0;
}

UOpenVinoStyleTransfer* UOpenVinoStyleTransfer::GetInstance()
{
	return openVinoTransfer;
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
	FString& retLog)
{
	// First, test if files passed exist, it is better to catch it early:
	if (!TestFileExists(xmlFilePath) ||
		!TestFileExists(binFilePath) )
	{
		retLog = TEXT("One or more files passed to Initialize don't exit");
		return false;
	}

	mode = 0;

	tmp_buffer.Reset(0);
	tmp_buffer.SetNum(0);

	xml_file_path = xmlFilePath;
	bin_file_path = binFilePath;

	retLog = TEXT("Style transfer has been initialized.");
	UE_LOG(LogStyleTransfer, Log, TEXT("Style transfer has been initialized!"));

	// bind callback
	BindBackbufferCallback();

	openVinoTransfer = this;

	return true;
}

void UOpenVinoStyleTransfer::Release()
{
	openVinoTransfer = nullptr;

	// unbind buffer
	UnBindBackbufferCallback();

	// release
	OnResizeOutput(0, 0);
}

UTexture2D* UOpenVinoStyleTransfer::GetTransferedTexture()
{
	return out_tex;
}

bool UOpenVinoStyleTransfer::OnResizeOutput(int width, int height)
{
	if (window != nullptr)
	{
		window->DestroyWindowImmediately();
		window = nullptr;
		dialog = nullptr;
	}

	if (out_tex != nullptr)
	{
		//out_tex->RemoveFromRoot();
		out_tex = nullptr;
	}

	if (width == 0 || height == 0)
		return true;

	if (!OpenVino_Initialize(TCHAR_TO_ANSI(*xml_file_path), TCHAR_TO_ANSI(*xml_file_path), width, height))
		return false;

	UE_LOG(LogStyleTransfer, Log, TEXT("OpenVino initialize successful, width = %d, height = %d!"), width, height);

	// initialize buffer size
	size_t size = width * height;
	size_t buffer_size = size * 3;
	buffer.Reset(buffer_size);
	buffer.SetNum(buffer_size);
	rgba_buffer.Reset(size);
	rgba_buffer.SetNum(size);

	UE_LOG(LogStyleTransfer, Log, TEXT("Style transfer buffer initialized!"));

	// new window
	dialog = SStyleTransferResultDialog::ShowWindow(width, height, window);
	window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this](const TSharedRef<SWindow>& Window)
		{
			this->ClearWindow();
		}));

	UE_LOG(LogStyleTransfer, Log, TEXT("Style transfer window created!"));

	return true;
}

void UOpenVinoStyleTransfer::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	int newMode = transfer_mode->GetInt();
	if (newMode != mode)
	{
		if (mode == 1)
		{
			OnResizeOutput(0, 0);
		}
		mode = newMode;
		if (mode == 1)
		{
			last_out_width = transfer_width->GetInt();
			last_out_height = transfer_height->GetInt();
			OnResizeOutput(last_out_width, last_out_height);
		}
	}

	if (mode != 1)	// only cpu mode use buffer copy
		return;

	// reset input tmp process buffer
	if (input_size.X != 0 && input_size.Y != 0 && input_size != last_input_size)
	{
		UE_LOG(LogStyleTransfer, Log, TEXT("Style transfer resize input from %d*%d to %d*%d!"), last_input_size.X, last_input_size.Y, input_size.X, input_size.Y);

		size_t buffer_size = input_size.X * input_size.Y * 3;
		tmp_buffer.Reset(buffer_size);
		tmp_buffer.SetNum(buffer_size);
	}
	last_input_size = input_size;

	// output buffer change
	if (transfer_width->GetInt() != last_out_width || transfer_height->GetInt() != last_out_height)
	{
		UE_LOG(LogStyleTransfer, Log, TEXT("Style transfer resize output from %d*%d to %d*%d!"), last_out_width, last_out_height, transfer_width->GetInt(), transfer_height->GetInt());

		last_out_width = transfer_width->GetInt();
		last_out_height = transfer_height->GetInt();
		OnResizeOutput(last_out_width, last_out_height);
	}

	// begin transfer from captured data to texture via cpu pass
	if (tmp_buffer.Num() > 0 && StyleTransferToTexture(this, fb_data, input_size.X, input_size.Y))
	{
		// show texture in dialog
		dialog->UpdateTexture(out_tex);
	}
}

void  UOpenVinoStyleTransfer::BindBackbufferCallback()
{
	if (FSlateApplication::IsInitialized())
	{
		m_OnBackBufferReadyToPresent.Reset();

		m_OnBackBufferReadyToPresent = FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddUObject(this, &UOpenVinoStyleTransfer::OnBackBufferReady_RenderThread);

		UE_LOG(LogStyleTransfer, Log, TEXT("Style transfer bind back buffer copy callback!"));
	}
}

void UOpenVinoStyleTransfer::UnBindBackbufferCallback()
{
	if (m_OnBackBufferReadyToPresent.IsValid())
	{
		FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(m_OnBackBufferReadyToPresent);

		UE_LOG(LogStyleTransfer, Log, TEXT("Style transfer unbind back buffer copy callback!"));
	}
}

void UOpenVinoStyleTransfer::OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& BackBuffer)
{
	if (mode != 1)	// only cpu mode use buffer copy
		return;

	UGameViewportClient* gameViewport = GetWorld()->GetGameViewport();
	if (gameViewport == nullptr)
		return;

	FSceneViewport* vp = gameViewport->GetGameViewport();
	SWindow* gameWin = gameViewport->GetWindow().Get();

	if (vp == nullptr || gameWin == nullptr)
		return;

	if (gameWin->GetId() != SlateWindow.GetId())
		return;

	FVector2D gameWinPos = gameWin->GetPositionInScreen();
	FVector2D vpPos = vp->GetCachedGeometry().GetAbsolutePosition();

	// get input and reset tmp process buffer
	input_origin = vpPos - gameWinPos;
	input_size = vp->GetSize();
	
	FIntRect Rect(input_origin.X, input_origin.Y, input_origin.X + input_size.X, input_origin.Y + input_size.Y);
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	// Get out data
	RHICmdList.ReadSurfaceData(BackBuffer, Rect, fb_data, FReadSurfaceDataFlags(RCM_UNorm));
}

bool UOpenVinoStyleTransfer::StyleTransferToTexture(UObject* Outer, TArray<FColor>& TextureData, int inwidth, int inheight)
{
	TPromise<FColor*> Result;
	FString resultlog;
	
	// Read the size we need
	int width = transfer_width->GetInt();
	int height = transfer_height->GetInt();

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Outer, TextureData, inwidth, inheight, width, height, &resultlog, &Result, this]()
		{
			
			int index = 0;
			for (FColor color : TextureData)
			{
				tmp_buffer[index] = color.B;
				tmp_buffer[index + 1] = color.G;
				tmp_buffer[index + 2] = color.R;
				index = index + 3;
			}

			if( OpenVino_Infer_FromTexture(tmp_buffer.GetData(), inwidth, inheight, buffer.GetData(), debug_flag) )
			{
				index = 0;
				for(int i = 0; i < width * height; i++)
				{
					rgba_buffer[i].B = buffer[index];
					rgba_buffer[i].G = buffer[index + 1];
					rgba_buffer[i].R = buffer[index + 2];
					rgba_buffer[i].A = 255;
					index = index + 3;
				}
				Result.SetValue(rgba_buffer.GetData());
				resultlog = FString::Format(TEXT("Success:Width({0}), Height({1})"), { FString::FromInt(width), FString::FromInt(height) });
			}
			else
			{
				Result.SetValue(nullptr);
				resultlog = GetAndLogLastError();
			}
		}
	);
	
	Future = Result.GetFuture();
	bool ret = false;
	if (Future.IsValid())
	{
		FColor* transfered = Future.Get();
		if (transfered != nullptr)
		{
			if (out_tex == nullptr)
			{
				out_tex = CreateTexture(transfered, width, height);
				out_tex->AddToRoot();

				// texture created
				ret = true;
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
	return ret;
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

// SStyleTransferResultWin

void SStyleTransferResultDialog::Construct(const FArguments& InArgs)
{
	TSharedRef<SImage> NewImage = SNew(SImage).Image(nullptr);
	image = &NewImage.Get();

	ChildSlot
	[
		NewImage
	];
}

SStyleTransferResultDialog::~SStyleTransferResultDialog()
{
}

SStyleTransferResultDialog* SStyleTransferResultDialog::ShowWindow(int outputWidth, int outputHeight, SWindow*& showWindow)
{
	// new window
	const FText TitleText = NSLOCTEXT("StyleTransfer", "Result", "Show");
	// Create the window to pick the class
	TSharedRef<SWindow> TransWindow = SNew(SWindow)
		.Title(TitleText)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(outputWidth, outputHeight))
		//.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false);
	TSharedRef<SStyleTransferResultDialog> TransResultDialog = SNew(SStyleTransferResultDialog);

	TransWindow->SetContent(TransResultDialog);
	TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (RootWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(TransWindow, RootWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(TransWindow);
	}
	showWindow = &TransWindow.Get();
	return &TransResultDialog.Get();
}

void SStyleTransferResultDialog::UpdateTexture(UTexture2D* SourceTexture)
{
	ImageBrush = MakeShareable(new FSlateDynamicImageBrush(SourceTexture, FVector2D(SourceTexture->GetSizeX(), SourceTexture->GetSizeY()), FName(SourceTexture->GetName())));
	image->SetImage(ImageBrush.Get());
}