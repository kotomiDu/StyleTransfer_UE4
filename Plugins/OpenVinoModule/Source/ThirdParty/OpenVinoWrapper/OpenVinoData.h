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
#include <fstream>
#include <d3d11.h>
#include <ie/inference_engine.hpp>
#include "openvino/openvino.hpp"
#include "openvino/runtime/intel_gpu/properties.hpp"
#include "openvino/runtime/intel_gpu/ocl/ocl.hpp"
#include "OpenCLUtil.h"
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

	// varibales for ov2.0 
	ov::InferRequest      infer_request;
	ov::CompiledModel     compiled_model;
	cl::Buffer            _inputBuffer;
	cl::Buffer            _outputBuffer;
	cl::Context           _oclCtx;
	ov::Shape             input_shape;

	//opencl
	OCL       ocl;
	OCLEnv* oclEnv;
	OCLFilterStore* oclStore;
	SourceConversion* srcConversionKernel;
	ID3D11DeviceContext* m_pD3D11Cxt;
	ID3D11Device* m_pD3D11Dev;
	ID3D11Texture2D* m_ovSurfaceRGBA_cpu_copy;

	//performance metric
	double total_inference_time;
	double loading_time;
	int frame_count;
	std::ofstream logfile_mode1;
	std::ofstream logfile_mode2;

public:
	OpenVinoData()
	{
		frame_count = 0;
		total_inference_time = 0.0;
		logfile_mode1.open("mode1_log.txt",std::ios::binary);
		logfile_mode2.open("mode2_log.txt", std::ios::binary);
	};
	virtual ~OpenVinoData()
	{
		logfile_mode1.close();
		logfile_mode2.close();
	};

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
		int inferHeight,
		std::string devicename);

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
	 * @param out, image raw data after style transfer
	 */
	bool
		OpenVinoData::Infer(
			unsigned char* input,
			int inwidth,
			int inheight,
			unsigned char* output,
			bool debug_flag);


	/**
	 *Create OCL Context and Kernel
	 */
	bool Create_OCLCtx(ID3D11Device* d3dDevice);

	/**
	 * @brief Initialize OpenVino with passed model files and opencl context
	 * @param modelXmlFilePath
	 * @param ctx
	 * @param inferWidth
	 * @param inferHeight
	 */
	void Initialize_BaseOCL(
		std::string modelXmlFilePath,
		int inferWidth,
		int inferHeight);

	/**
	 * @brief Call infer using DirectX Texture2D RGBA
	 * @param input_surface, input Texture2D RGBA data
	 * @param output_surface, output Texture2D RGBA data
	 * @param surfaceWidth, width of Texture2D
	 * @param surfaceHeight, height of Texture2D
	 * @param debug_flag, debug mode
	 */
	bool OpenVinoData::Infer(
		ID3D11Texture2D * input_surface,
		ID3D11Texture2D * output_surface,
		int surfaceWidth,
		int surfaceHeight,
		bool debug_flag);

};
