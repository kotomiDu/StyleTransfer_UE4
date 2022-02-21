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


#pragma once

#ifdef OPEN_VINO_LIBRARY

#if defined _WIN32 || defined _WIN64
#include <Windows.h>
#define DLLEXPORT __declspec(dllexport)
#else
#include <stdio.h>
#endif

#ifndef DLLEXPORT
#define DLLEXPORT
#endif

#include <inference_engine.hpp>

#endif

extern "C"
{
	/**
	 * All methods use C-style calls, returning true on success and fail, while setting last error.
	 * All output data is pre-initialized on caller side and passed into the calls.
	 */

	/*
	* @brief This method is called to make initialization of the OpenVino library and load the
	* models based on files specified in "modelXmlFilePath", "modelBinFilePath" and "modelLabelFilePath".
	* @param modelXmlFilePath Path to, for example: squeezenet1.1.xml
	* @param modelBinFilePath Path to, for example: squeezenet1.1.bin
	* @param modelLabelFilePath Path to, for example: squeezenet1.1.labels
	* @return true if call is successfull or false if not
	*/
	DLLEXPORT bool OpenVino_Initialize(
		const char* modelXmlFilePath,
		const char* modelBinFilePath,
		int inferWidth,
		int inferHeight);

	/*
	 * @brief This method is used to infer results, based on loaded model (see "OpenVino_Initialize")
	 * and based on image loaded from "filePath".
	 * @param filePath path to the file to be analysed
	 * @param result called-allocated buffer where result string will be copied to
	 * @param maxResultSize maximum size of result buffer
	 * @return true if call is successfull or false if not
	 */
	DLLEXPORT bool OpenVino_Infer_FromPath(
		char* filePath,
		char* result,
		size_t maxResultSize);

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
	DLLEXPORT bool OpenVino_Infer_FromTexture(
		unsigned char* input, int inwidth, int inheight, int* width, int* height, unsigned char* output, bool debug_flag);

	/*
	* @brief This method is used to infer results, based on loaded model (see "OpenVino_Initialize")
	* and based on image loaded from "filePath".
	* @param filePath path to the file to be analysed
	* @param outwidth, image width after style transfer
	* @param outheight, image height after style transfer
	* @param output, image data after style transfer
	* @return true if call is successfull or false if not
	*/
	DLLEXPORT bool OpenVino_Infer_FromFile(
		char* filePath, int* outwidth, int* outheight, float* output);
	/*
	 * @brief This method is used to get last error message that any of the other methods might have set.
	 * @param lastErrorMessage user-allocated buffer into which error message will be copied
	 * @param maxLength length of buffer in lastErrorMessage
	 * @return true if call is successfull or false if not
	 */
	DLLEXPORT bool OpenVino_GetLastError(
		char* lastErrorMessage,
		size_t maxLength);
}
