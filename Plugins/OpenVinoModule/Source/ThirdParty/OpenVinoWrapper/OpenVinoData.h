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

#include <string>
#include <inference_engine.hpp>

/**
 * @class OpenVinoData
 * @brief This class handles actual process of initialization and calls to infer and parsing of results
 */
class OpenVinoData
{
	// Data describing input of OpenVino algorithm
	InferenceEngine::InputInfo::Ptr input_info;
	// Loaded executable nerual network
	InferenceEngine::ExecutableNetwork executable_network;
	// Input and output names for OpenVino algorithm
	std::string input_name, output_name;

public:
	OpenVinoData() = default;
	virtual ~OpenVinoData() = default;

public:
	/**
	 * @brief Initialize OpenVino with passed model files
	 * @param modelXmlFilePath
	 * @param modelBinFilePath
	 * @param modelLabelFilePath
	 */
	void Initialize(
		std::string modelXmlFilePath,
		std::string modelBinFilePath,
		int inferWidth,
		int inferHeight);


	/**
	 * @brief Call infer using loaded model files
	 * @param filePath
	 * @param width of image after style transfer
	 * @param height of image after style transfer
	 * @param height of image after style transfer
	 * @param out, image raw data after style transfer
	 */
	bool
		OpenVinoData::Infer(
			std::string filePath,
			int* width,
			int* height,
			float* out);

	/**
	 * @brief Call infer using image raw data
	 * @param input, image raw data
	 * @param inwidth, width of image
	 * @param inheight, height of image
	 * @param outwidth, width of image after style transfer
	 * @param outheight, height of image after style transfer
	 * @param out, image raw data after style transfer
	 */
	bool
		OpenVinoData::Infer(
			unsigned char* input,
			int inwidth,
			int inheight,
			int* outwidth,
			int* outheight,
			unsigned char* output,
			bool debug_flag);

};
