#pragma once

#include <cstdint>
#include <string>
#include <vector>


class NNUE2
{
public:

	static constexpr int NUM_FEATURES = 40962;
	static constexpr int NUM_FEATURES_WITH_PADDING = 40963;
	static constexpr int HIDDEN_SIZE = 32;

	NNUE2();

	bool load(const std::string& filename);

	int evaluate(const std::vector<int>& features) const;

private:

	// Embedding:
	// 40963 features × 32 values
	float embedding[NUM_FEATURES_WITH_PADDING][HIDDEN_SIZE];

	// First fully-connected layer
	float fc1_weights[HIDDEN_SIZE][HIDDEN_SIZE];
	float fc1_bias[HIDDEN_SIZE];

	// Second fully-connected layer
	float fc2_weights[HIDDEN_SIZE];
	float fc2_bias;

	bool loaded;
};
