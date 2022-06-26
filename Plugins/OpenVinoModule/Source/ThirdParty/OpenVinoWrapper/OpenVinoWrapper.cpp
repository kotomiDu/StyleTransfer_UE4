/*
"Copyright 2019 Intel Corporation.

The source code, information and material ("Material") contained herein is owned by Intel Corporation or its
suppliers or licensors, and title to such Material remains with Intel Corporation or its suppliers or licensors.
The Material contains proprietary information of Intel or its suppliers and licensors. The Material is
protected by worldwide copyright laws and treaty provisions. No part of the Material may be used, copied,
reproduced, modified, published, uploaded, posted, transmitted, distributed or disclosed in any way without Intel's
prior express written permission. No license under any patent, copyright or other intellectual property rights in
the Material is granted to or conferred upon you, either expressly, by implication, inducement, estoppel or otherwise.
Any license under such intellectual property rights must be express and approved by Intel in writing.


Include supplier trademarks or logos as supplier requires Intel to use, preceded by an asterisk. An asterisked footnote
can be added as follows: *Third Party trademarks are the property of their respective owners.

Unless otherwise agreed by Intel in writing, you may not remove or alter this notice or any other notice
embedded in Materials by Intel or Intel's suppliers or licensors in any way."
*/


// OpenVinoWrapper.cpp : Defines the entry point for the DLL that is going to be wrapper for calls to
// Intel's OpenVino from Unreal Engine

#include "OpenVinoWrapper.h"

#include <vector>
#include <memory>
#include <string>
#include <d3d11.h>

#include "OpenVinoData.h"
using namespace std;

// This variable holds last error message, if any 
static string last_error;
// This is a pointer to initialized OpenVino model structures, assigned by "OpenVino_Initialize"
static unique_ptr<OpenVinoData> initializedData;
// This variable holds the model width and height
static int modelWidth;
static int modelHeight;

/*
 * @brief This method is called to make initialization of the OpenVino library and load the
 * models based on files specified in "modelXmlFilePath", "modelBinFilePath" and "modelLabelFilePath".
 * @param modelXmlFilePath Path to, for example: squeezenet1.1.xml
 * @param modelBinFilePath Path to, for example: squeezenet1.1.bin
 * @param modelLabelFilePath Path to, for example: squeezenet1.1.labels
 * @return true if call is successfull or false if not
 */
DLLEXPORT
bool __cdecl 
OpenVino_Initialize(
	LPCSTR modelXmlFilePath,
	LPCSTR modelBinFilePath,
	int inferWidth,
	int inferHeight)
{
	try
	{
		if (modelXmlFilePath == nullptr ||
			modelBinFilePath == nullptr)
			throw invalid_argument("One of the file paths passed was null");

		last_error.clear();

		// OpenVinoData structure does actual processing:
		auto ptr = std::make_unique<OpenVinoData>();
		// Forward initialization to OpenVinoData:
		ptr->Initialize(modelXmlFilePath, modelBinFilePath, inferWidth, inferHeight);
		// Save it for use in later calls:
		initializedData = std::move(ptr);

		return true;
	}
	catch (std::exception& ex)
	{
		last_error = ex.what();

		return false;
	}
	catch (...)
	{
		last_error = "General error";

		return false;
	}
}


/*
* @brief This method is used to infer results, based on loaded model (see "OpenVino_Initialize")
* and based on texture rendered by engine.
* @param texture data
* @param inwidth, texture width
* @param inheight, texture height
* @param outwidth, image width after style transfer
* @param outheight, image height after style transfer
* @param output, image data after style transfer
* @return true if call is successfull or false if not
*/
DLLEXPORT
bool __cdecl
OpenVino_Infer_FromTexture(
	unsigned char* input, int inwidth, int inheight,  unsigned char* out, bool debug_flag)
{
	try
	{
		if (!initializedData)
			throw std::invalid_argument("OpenVINO has not been initialized");

		/*if (filePath == nullptr)
			throw std::invalid_argument("File path passed was null");*/

		// Actual Infer call passed to OpenVinoData
		initializedData->Infer(input, inwidth, inheight, out, debug_flag);

		return true;
	}
	catch (...)
	{
		last_error = "General error";

		return false;
	}
}

/*
* @brief This method is used to infer results, based on loaded model (see "OpenVino_Initialize")
* and based on image loaded from "filePath".
* @param filePath path to the file to be analysed
* @param outwidth, image width after style transfer
* @param outheight, image height after style transfer
* @param output, image data after style transfer
* @return true if call is successfull or false if not
*/
DLLEXPORT
bool __cdecl
OpenVino_Infer_FromFile(
	char* filePath, int* outwidth, int* outheight, float* output)
{
	try
	{
		if (!initializedData)
			throw std::invalid_argument("OpenVINO has not been initialized");

		if (filePath == nullptr)
			throw std::invalid_argument("File path passed was null");

		// Actual Infer call passed to OpenVinoData
		initializedData->Infer(filePath, outwidth, outheight, output);

		return true;
	}
	catch (...)
	{
		last_error = "General error";

		return false;
	}
}

/*
 * @brief This method is used to get last error message that any of the other methods might have set.
 * @param lastErrorMessage user-allocated buffer into which error message will be copied
 * @param maxLength length of buffer in lastErrorMessage
 * @return true if call is successfull or false if not
 */
DLLEXPORT
bool __cdecl
OpenVino_GetLastError(
	char* lastErrorMessage,
	size_t maxLength)
{
	if (lastErrorMessage == nullptr)
		return false;

	if (last_error.length() >= maxLength)
		return false;

	strcpy_s(lastErrorMessage, maxLength, last_error.c_str());

	return true;
}

/*
* @brief This method is called to make initialization of the OpenVino library and load the
* models based on files specified in "modelXmlFilePath", "modelBinFilePath" and "d3dDevice".
* @param modelXmlFilePath Path to, for example: style_transfer.xml
* @param modelBinFilePath Path to, for example: style_transfer.bin
* @param d3dDevice
* @param inferWidth, inference width
* @param inferHeight, inference height
* @return true if call is successfull or false if not
*/
DLLEXPORT
bool __cdecl
OpenVino_Initialize_BaseOCL(
	LPCSTR modelXmlFilePath,
	LPCSTR modelBinFilePath,
	void* d3dDevice,
	int inferWidth,
	int inferHeight)
{
	try
	{
		if (modelXmlFilePath == nullptr ||
			modelBinFilePath == nullptr)
			throw invalid_argument("One of the file paths passed was null");

		last_error.clear();
		ID3D11Device* dxDevice = (ID3D11Device*)d3dDevice;
		

		// OpenVinoData structure does actual processing:
		auto ptr = std::make_unique<OpenVinoData>();
		//Create opencl context
		ptr->Create_OCLCtx(dxDevice);
		
		// Forward initialization to OpenVinoData:
		ptr->Initialize_BaseOCL(modelXmlFilePath,inferWidth, inferHeight);
		// Save it for use in later calls:
		initializedData = std::move(ptr);

		return true;
	}
	catch (std::exception& ex)
	{
		last_error = ex.what();

		return false;
	}
	catch (...)
	{
		last_error = "General error";

		return false;
	}
}


DLLEXPORT
bool __cdecl
OpenVino_Infer_FromDXData(
	void* input_surface,
	void* output_surface,
	int surfaceWidth,
	int surfaceHeight, 
	bool debug_flag)
{
	try
	{
		if (!initializedData)
			throw std::invalid_argument("OpenVINO has not been initialized");

		ID3D11Texture2D* input_dxdata = (ID3D11Texture2D*)input_surface;
		ID3D11Texture2D* output_dxdata = (ID3D11Texture2D*)output_surface;
		// Actual Infer call passed to OpenVinoData
		initializedData->Infer(input_dxdata, output_dxdata, surfaceWidth,surfaceHeight,debug_flag);

		return true;
	}
	catch (...)
	{
		last_error = "General error";

		return false;
	}
}

DLLEXPORT
bool __cdecl
OpenVino_GetSuitableSTsize(
	int inputWidth,
	int inputHeight,
	int* expectedWidth,
	int* expectedHeight)
{
	try
	{
		last_error.clear();
		*expectedWidth = (int)floor((inputWidth + 3) / 4.0) * 4;
		*expectedHeight = (int)floor((inputHeight + 3) / 4.0) * 4;

		if (*expectedWidth == inputWidth
			&& *expectedHeight == inputHeight)
		{
			std::cout << "The original input shape (width, height) is not suitable:" << to_string(inputWidth) << "," << to_string(inputHeight) << std::endl;
			std::cout << ";try new input shape (width, height) :" << to_string(*expectedWidth) << "," << to_string(*expectedHeight) << std::endl;
		}
		modelWidth = *expectedWidth;
		modelHeight = *expectedHeight;

		return true;
	}
	catch (std::exception& ex)
	{
		last_error = ex.what();

		return false;
	}
	catch (...)
	{
		last_error = "Cannot get sutitable size";

		return false;
	}	
}

DLLEXPORT
bool __cdecl
OpenVINO_GetCurrentSTsize(
	int* width,
	int* height)
{
	try
	{
		last_error.clear();
		*width = modelWidth;
		*height = modelHeight;
		return true;
	}
	catch (std::exception& ex)
	{
		last_error = ex.what();

		return false;
	}
	catch (...)
	{
		last_error = "Cannot get current style transfer size";
		return false;
	}
}
