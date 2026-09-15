#include "nnue.h"

#include <algorithm>
#include <fstream>


NNUE::NNUE()
	: fc2_bias(0.0f),
	loaded(false)
{
}


bool NNUE::load(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary);

	if (!file)
		return false;

	file.read(
		reinterpret_cast<char*>(embedding),
		sizeof(embedding)
	);

	if (!file)
		return false;

	file.read(
		reinterpret_cast<char*>(fc1_weights),
		sizeof(fc1_weights)
	);

	if (!file)
		return false;

	file.read(
		reinterpret_cast<char*>(fc1_bias),
		sizeof(fc1_bias)
	);

	if (!file)
		return false;

	file.read(
		reinterpret_cast<char*>(fc2_weights),
		sizeof(fc2_weights)
	);

	if (!file)
		return false;

	file.read(
		reinterpret_cast<char*>(&fc2_bias),
		sizeof(fc2_bias)
	);

	if (!file)
		return false;

	loaded = true;

	return true;
}


int NNUE::halfkp_feature(
	int kingSquare,
	int pieceSquare,
	int pieceType,
	int colour
) const
{
	// pieceType:
	//
	// 0 = pawn
	// 1 = knight
	// 2 = bishop
	// 3 = rook
	// 4 = queen

	// Python's chess module uses:
	//
	// WHITE = 1
	// BLACK = 0
	//
	// So we deliberately preserve that here.

	int pieceIndex =
		pieceType * 2 + colour;

	return
		kingSquare
		+ 64 * pieceSquare
		+ 64 * 64 * pieceIndex;
}


int NNUE::evaluate(
	const std::vector<int>& features
) const
{
	if (!loaded)
		return 0;

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

	// ReLU
	for (int i = 0; i < HIDDEN_SIZE; ++i)
	{
		accumulator[i] =
			std::max(0.0f, accumulator[i]);
	}

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

		// ReLU
		hidden[i] =
			std::max(0.0f, value);
	}

	float output = fc2_bias;

	for (int i = 0; i < HIDDEN_SIZE; ++i)
	{
		output +=
			hidden[i] *
			fc2_weights[i];
	}

	return static_cast<int>(
		output * 2000.0f
		);
}


int NNUE::evaluate(
	const int8_t* MailboxBoard64,
	int sideToMove
) const
{
	if (!loaded)
		return 0;

	int whiteKingSquare = -1;
	int blackKingSquare = -1;

	// --------------------------------------------------
	// Find the two kings
	// --------------------------------------------------

	for (int square = 0; square < 64; ++square)
	{
		int piece = MailboxBoard64[square];

		if (piece == 6)
			whiteKingSquare = square;

		else if (piece == -6)
			blackKingSquare = square;
	}

	// A legal chess position must contain both kings.

	if (whiteKingSquare < 0 ||
		blackKingSquare < 0)
	{
		return 0;
	}

	std::vector<int> features;

	features.reserve(32);

	// --------------------------------------------------
	// Generate HalfKP features
	// --------------------------------------------------

	for (int square = 0; square < 64; ++square)
	{
		int piece = MailboxBoard64[square];

		if (piece == 0)
			continue;

		// White pieces
		if (piece > 0)
		{
			// King is not represented as a piece feature.
			if (piece == 6)
				continue;

			int pieceType = piece - 1;

			features.push_back(
				halfkp_feature(
					whiteKingSquare,
					square,
					pieceType,
					1       // Python WHITE
				)
			);
		}

		// Black pieces
		else
		{
			// Black king is not represented as a piece feature.
			if (piece == -6)
				continue;

			int pieceType = (-piece) - 1;

			features.push_back(
				halfkp_feature(
					blackKingSquare,
					square,
					pieceType,
					0       // Python BLACK
				)
			);
		}
	}

	// --------------------------------------------------
	// Side to move
	// --------------------------------------------------

	constexpr int SIDE_TO_MOVE_OFFSET =
		NUM_FEATURES - 2;

	if (sideToMove == 0)
	{
		// White to move
		features.push_back(
			SIDE_TO_MOVE_OFFSET
		);
	}
	else
	{
		// Black to move
		features.push_back(
			SIDE_TO_MOVE_OFFSET + 1
		);
	}

	// --------------------------------------------------
	// Evaluate
	// --------------------------------------------------

	return evaluate(features);
}