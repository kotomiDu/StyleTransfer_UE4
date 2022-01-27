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

#include "OpenVinoData.h"

using namespace std;

// This variable holds last error message, if any 
static string last_error;
// This is a pointer to initialized OpenVino model structures, assigned by "OpenVino_Initialize"
static unique_ptr<OpenVinoData> initializedData;

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
	LPCSTR modelLabelFilePath)
{
	try
	{
		if (modelXmlFilePath == nullptr ||
			modelBinFilePath == nullptr ||
			modelLabelFilePath == nullptr)
			throw invalid_argument("One of the file paths passed was null");

		last_error.clear();

		// OpenVinoData structure does actual processing:
		auto ptr = std::make_unique<OpenVinoData>();
		// Forward initialization to OpenVinoData:
		ptr->Initialize(modelXmlFilePath, modelBinFilePath, modelLabelFilePath);
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
 * and based on image loaded from "filePath".
 * @param filePath path to the file to be analysed
 * @param result called-allocated buffer where result string will be copied to
 * @param maxResultSize maximum size of result buffer
 * @return true if call is successfull or false if not
 */
DLLEXPORT
bool __cdecl
OpenVino_Infer_FromPath(
	char* filePath,
	char* result,
	size_t maxResultSize)
{
	try
	{
		if (!initializedData)
			throw std::invalid_argument("OpenVINO has not been initialized");

		if (filePath == nullptr)
			throw std::invalid_argument("File path passed was null");

		if (result == nullptr)
			throw std::invalid_argument("Argument result passed was null");

		if (maxResultSize == 0)
			throw std::invalid_argument("Argument maxResultSize was 0");

		// Actual Infer call passed to OpenVinoData
		auto ret = initializedData->Infer(filePath);

		// Copy the result, provided enough space was available:
		if (ret.length() >= maxResultSize)
			throw invalid_argument("Result size was smaller then expected");

		strcpy_s(result, maxResultSize, ret.c_str());

		return true;
	}
	catch (exception& ex)
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
 * and based on texture loaded from game engine.
 * @param filePath path to the file to be analysed
 * @param result called-allocated buffer where result string will be copied to
 * @param maxResultSize maximum size of result buffer
 * @return true if call is successfull or false if not
 */
DLLEXPORT
bool __cdecl
OpenVino_Infer_FromTexture(
	char* filePath, int* width, int* height, float* out)
{
	try
	{
		if (!initializedData)
			throw std::invalid_argument("OpenVINO has not been initialized");

		if (filePath == nullptr)
			throw std::invalid_argument("File path passed was null");

		// Actual Infer call passed to OpenVinoData
		initializedData->Infer(filePath, width, height, out);

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
