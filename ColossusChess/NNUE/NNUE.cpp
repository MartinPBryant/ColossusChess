#include "NNUE.h"

#include <algorithm>
#include <fstream>

//#include "..\SearchNormal.h"

NNUE2::NNUE2()
	: fc2_bias(0.0f),
	loaded(false)
//NNUE::NNUE()
{
}


bool NNUE2::load(const std::string& filename)
{
	std::ifstream file(
		filename,
		std::ios::binary
	);

	if (!file)
		return false;


	// -------------------------------------------------
	// Embedding
	// -------------------------------------------------

	file.read(
		reinterpret_cast<char*>(embedding),
		sizeof(embedding)
	);

	if (!file)
		return false;


	// -------------------------------------------------
	// FC1 weights
	// -------------------------------------------------

	file.read(
		reinterpret_cast<char*>(fc1_weights),
		sizeof(fc1_weights)
	);

	if (!file)
		return false;


	// -------------------------------------------------
	// FC1 bias
	// -------------------------------------------------

	file.read(
		reinterpret_cast<char*>(fc1_bias),
		sizeof(fc1_bias)
	);

	if (!file)
		return false;


	// -------------------------------------------------
	// FC2 weights
	// -------------------------------------------------

	file.read(
		reinterpret_cast<char*>(fc2_weights),
		sizeof(fc2_weights)
	);

	if (!file)
		return false;


	// -------------------------------------------------
	// FC2 bias
	// -------------------------------------------------

	file.read(
		reinterpret_cast<char*>(&fc2_bias),
		sizeof(fc2_bias)
	);

	if (!file)
		return false;


	loaded = true;

	return true;
}


int NNUE2::evaluate(
	const std::vector<int>& features
) const
{
	if (!loaded)
		return 0;


	// -------------------------------------------------
	// Accumulate embedding vectors
	// -------------------------------------------------

	float accumulator[HIDDEN_SIZE] = {};

	for (int feature : features)
	{
		if (feature < 0 ||
			feature >= NUM_FEATURES_WITH_PADDING)
		{
			continue;
		}

		for (int i = 0; i < HIDDEN_SIZE; ++i)
		{
			accumulator[i] +=
				embedding[feature][i];
		}
	}


	// -------------------------------------------------
	// ReLU / clamp
	// -------------------------------------------------

	for (int i = 0; i < HIDDEN_SIZE; ++i)
	{
		accumulator[i] =
			std::max(0.0f, accumulator[i]);
	}


	// -------------------------------------------------
	// FC1
	// -------------------------------------------------

	float hidden[HIDDEN_SIZE];

	for (int i = 0; i < HIDDEN_SIZE; ++i)
	{
		float value = fc1_bias[i];

		for (int j = 0; j < HIDDEN_SIZE; ++j)
		{
			value +=
				accumulator[j] *
				fc1_weights[i][j];
		}

		hidden[i] =
			std::max(0.0f, value);
	}


	// -------------------------------------------------
	// FC2
	// -------------------------------------------------

	float output = fc2_bias;

	for (int i = 0; i < HIDDEN_SIZE; ++i)
	{
		output +=
			hidden[i] *
			fc2_weights[i];
	}


	// -------------------------------------------------
	// Convert from -1..+1 to centipawns
	// -------------------------------------------------

	int score =
		static_cast<int>(output * 2000.0f);

	return score;
}
