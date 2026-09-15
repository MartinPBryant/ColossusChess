#pragma once

#include <cstdint>
#include <string>
#include <vector>

class NNUE
{
public:
	static constexpr int NUM_FEATURES = 40962;
	static constexpr int NUM_FEATURES_WITH_PADDING = 40963;
	static constexpr int HIDDEN_SIZE = 32;

	NNUE();

	bool load(const std::string& filename);

	// Existing evaluator
	int evaluate(const std::vector<int>& features) const;

	// Evaluate directly from the engine's mailbox
	int evaluate(
		const int8_t* MailboxBoard64,
		int sideToMove
	) const;

private:
	float embedding[NUM_FEATURES_WITH_PADDING][HIDDEN_SIZE];
	float fc1_weights[HIDDEN_SIZE][HIDDEN_SIZE];
	float fc1_bias[HIDDEN_SIZE];
	float fc2_weights[HIDDEN_SIZE];
	float fc2_bias;

	bool loaded;

	int halfkp_feature(
		int kingSquare,
		int pieceSquare,
		int pieceType,
		int colour
	) const;
};