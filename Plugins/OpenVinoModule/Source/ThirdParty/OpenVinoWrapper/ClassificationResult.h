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

#include <iostream>
#include <streambuf>
#include <inference_engine.hpp>
/**
 * @brief trims whitespace from begining and end of the string
 */
inline std::string& trim(std::string& s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
	s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());

	return s;
}

/**
 * @class ClassificationResult
 * @brief A ClassificationResult creates an output table with results
 */
class ClassificationResult
{
	const std::string _classidStr = "classid";
	const std::string _probabilityStr = "probability";
	const std::string _labelStr = "label";
	size_t _nTop;
	InferenceEngine::Blob::Ptr _outBlob;
	const std::vector<std::string> _labels;
	const std::vector<std::string> _imageNames;
	const size_t _batchSize;

private:
	/**
	 * @brief returns formated header for result
	 */
	std::string getHeader()
	{
		std::stringstream res;

		res << _classidStr << " " << _probabilityStr;
		if (!_labels.empty())
			res << " " << _labelStr;
		std::string classidColumn(_classidStr.length(), '-');
		std::string probabilityColumn(_probabilityStr.length(), '-');
		std::string labelColumn(_labelStr.length(), '-');
		res << std::endl << classidColumn << " " << probabilityColumn;
		if (!_labels.empty())
			res << " " << labelColumn;
		res << std::endl;
		res.flush();

		return res.str();
	}

	/**
	 * @brief Gets the top n results from a tblob
	 *
	 * @param n Top n count
	 * @param input 1D tblob that contains probabilities
	 * @param output Vector of indexes for the top n places
	 */
	template <class T>
	void topResults(unsigned int n, InferenceEngine::TBlob<T>& input, std::vector<unsigned>& output) {
		InferenceEngine::SizeVector dims = input.getTensorDesc().getDims();
		size_t input_rank = dims.size();
		if (!input_rank || !dims[0]) THROW_IE_EXCEPTION << "Input blob has incorrect dimensions!";
		size_t batchSize = dims[0];
		std::vector<unsigned> indexes(input.size() / batchSize);

		n = static_cast<unsigned>(std::min<size_t>((size_t)n, input.size()));

		output.resize(n * batchSize);

		for (size_t i = 0; i < batchSize; i++) {
			size_t offset = i * (input.size() / batchSize);
			T* batchData = input.data();
			batchData += offset;

			std::iota(std::begin(indexes), std::end(indexes), 0);
			std::partial_sort(std::begin(indexes), std::begin(indexes) + n, std::end(indexes),
				[&batchData](unsigned l, unsigned r) {
				return batchData[l] > batchData[r];
			});
			for (unsigned j = 0; j < n; j++) {
				output.at(i * n + j) = indexes.at(j);
			}
		}
	}

	/**
	 * @brief Gets the top n results from a blob
	 *
	 * @param n Top n count
	 * @param input 1D blob that contains probabilities
	 * @param output Vector of indexes for the top n places
	 */
	void topResults(unsigned int n, InferenceEngine::Blob& input, std::vector<unsigned>& output) {
#define TBLOB_TOP_RESULT(precision)                                                                             \
        case InferenceEngine::Precision::precision: {                                                               \
            using myBlobType = InferenceEngine::PrecisionTrait<InferenceEngine::Precision::precision>::value_type;  \
            InferenceEngine::TBlob<myBlobType>& tblob = dynamic_cast<InferenceEngine::TBlob<myBlobType>&>(input);   \
            topResults(n, tblob, output);                                                                           \
            break;                                                                                                  \
        }

		switch (input.getTensorDesc().getPrecision()) {
			TBLOB_TOP_RESULT(FP32);
			TBLOB_TOP_RESULT(FP16);
			TBLOB_TOP_RESULT(Q78);
			TBLOB_TOP_RESULT(I16);
			TBLOB_TOP_RESULT(U8);
			TBLOB_TOP_RESULT(I8);
			TBLOB_TOP_RESULT(U16);
			TBLOB_TOP_RESULT(I32);
			TBLOB_TOP_RESULT(U64);
			TBLOB_TOP_RESULT(I64);
		default:
			THROW_IE_EXCEPTION << "cannot locate blob for precision: " << input.getTensorDesc().getPrecision();
		}

#undef TBLOB_TOP_RESULT
	}


public:
	explicit ClassificationResult(InferenceEngine::Blob::Ptr output_blob,
		std::vector<std::string> image_names = {},
		size_t batch_size = 1,
		size_t num_of_top = 10,
		std::vector<std::string> labels = {}) :
		_nTop(num_of_top),
		_outBlob(std::move(output_blob)),
		_labels(std::move(labels)),
		_imageNames(std::move(image_names)),
		_batchSize(batch_size) {
		if (_imageNames.size() != _batchSize) {
			throw std::logic_error("Batch size should be equal to the number of images.");
		}
	}

	/**
	 * @brief returns formated result as a string
	 */
	std::string toString()
	{
		std::stringstream res;

		/** This vector stores id's of top N results **/
		std::vector<unsigned> results;
		topResults(_nTop, *_outBlob, results);

		/** Print the result iterating over each batch **/
		res << std::endl << "Top " << _nTop << " results:" << std::endl << std::endl;
		for (unsigned int image_id = 0; image_id < _batchSize; ++image_id)
		{
			res << getHeader();

			for (size_t id = image_id * _nTop, cnt = 0; id < (image_id + 1) * _nTop; ++cnt, ++id)
			{
				std::cout.precision(7);
				/** Getting probability for resulting class **/
				const auto result = _outBlob->buffer().
					as<InferenceEngine::PrecisionTrait<InferenceEngine::Precision::FP32>::value_type*>()
					[results[id] + image_id * (_outBlob->size() / _batchSize)];

				res << std::setw(static_cast<int>(_classidStr.length())) << std::left << results[id] << " ";
				res << std::left << std::setw(static_cast<int>(_probabilityStr.length())) << std::fixed << result;

				if (!_labels.empty())
					res << " " + _labels[results[id]];
				res << std::endl;
			}
			res << std::endl;
		}

		return res.str();
	}
};