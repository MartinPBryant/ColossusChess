#include "nnue.h"

// ------------------------------------------------------------
// IMPORTANT:
//
// Replace this include with the header containing your Brain
// class definition.
//
// For example:
//
// #include "Brain.h"
//
// ------------------------------------------------------------

#include "Brain.h"

#include <cstdio>
#include <cstring>
#include <cmath>


// ============================================================
// Constructor / destructor
// ============================================================

NNUE::NNUE()
{
	featureWeights = nullptr;

	std::memset(fc1Weights, 0, sizeof(fc1Weights));
	std::memset(fc1Bias, 0, sizeof(fc1Bias));

	std::memset(fc2Weights, 0, sizeof(fc2Weights));
	std::memset(fc2Bias, 0, sizeof(fc2Bias));

	std::memset(fc3Weights, 0, sizeof(fc3Weights));
	fc3Bias = 0.0f;

	std::memset(whiteAccumulator, 0, sizeof(whiteAccumulator));
	std::memset(blackAccumulator, 0, sizeof(blackAccumulator));

	std::memset(input, 0, sizeof(input));
	std::memset(hidden1, 0, sizeof(hidden1));
	std::memset(hidden2, 0, sizeof(hidden2));
}


NNUE::~NNUE()
{
	delete[] featureWeights;
	featureWeights = nullptr;
}


// ============================================================
// LoadWeights
// ============================================================

bool NNUE::LoadWeights(const char* filename)
{
	FILE* file = nullptr;

#ifdef _MSC_VER
	if (fopen_s(&file, filename, "rb") != 0)
		file = nullptr;
#else
	file = std::fopen(filename, "rb");
#endif

	if (file == nullptr)
	{
		std::printf(
			"NNUE: Could not open weight file: %s\n",
			filename
		);

		return false;
	}


	// --------------------------------------------------------
	// Read and verify magic
	// --------------------------------------------------------

	char magic[8];

	if (std::fread(magic, 1, 8, file) != 8)
	{
		std::printf("NNUE: Could not read file header.\n");
		std::fclose(file);
		return false;
	}

	if (std::memcmp(magic, "NNUEF32\0", 8) != 0)
	{
		std::printf("NNUE: Invalid file magic.\n");
		std::fclose(file);
		return false;
	}


	// --------------------------------------------------------
	// Read version
	// --------------------------------------------------------

	uint32_t version;

	if (std::fread(&version, sizeof(uint32_t), 1, file) != 1)
	{
		std::printf("NNUE: Could not read version.\n");
		std::fclose(file);
		return false;
	}

	if (version != 1)
	{
		std::printf(
			"NNUE: Unsupported version: %u\n",
			version
		);

		std::fclose(file);
		return false;
	}


	// --------------------------------------------------------
	// Read dimensions
	// --------------------------------------------------------

	uint32_t numFeatures;
	uint32_t featureDim;
	uint32_t hidden1;
	uint32_t hidden2;
	uint32_t outputDim;

	if (std::fread(
		&numFeatures,
		sizeof(uint32_t),
		1,
		file
	) != 1)
	{
		std::fclose(file);
		return false;
	}

	if (std::fread(
		&featureDim,
		sizeof(uint32_t),
		1,
		file
	) != 1)
	{
		std::fclose(file);
		return false;
	}

	if (std::fread(
		&hidden1,
		sizeof(uint32_t),
		1,
		file
	) != 1)
	{
		std::fclose(file);
		return false;
	}

	if (std::fread(
		&hidden2,
		sizeof(uint32_t),
		1,
		file
	) != 1)
	{
		std::fclose(file);
		return false;
	}

	if (std::fread(
		&outputDim,
		sizeof(uint32_t),
		1,
		file
	) != 1)
	{
		std::fclose(file);
		return false;
	}


	// --------------------------------------------------------
	// Verify dimensions
	// --------------------------------------------------------

	if (numFeatures != NUM_FEATURES + 1)
	{
		std::printf(
			"NNUE: Unexpected number of feature rows: %u\n",
			numFeatures
		);

		std::fclose(file);
		return false;
	}

	if (featureDim != FEATURE_DIM)
	{
		std::printf(
			"NNUE: Unexpected feature dimension: %u\n",
			featureDim
		);

		std::fclose(file);
		return false;
	}

	if (hidden1 != HIDDEN1)
	{
		std::printf(
			"NNUE: Unexpected hidden1 dimension: %u\n",
			hidden1
		);

		std::fclose(file);
		return false;
	}

	if (hidden2 != HIDDEN2)
	{
		std::printf(
			"NNUE: Unexpected hidden2 dimension: %u\n",
			hidden2
		);

		std::fclose(file);
		return false;
	}

	if (outputDim != 1)
	{
		std::printf(
			"NNUE: Unexpected output dimension: %u\n",
			outputDim
		);

		std::fclose(file);
		return false;
	}


	// --------------------------------------------------------
	// Allocate feature weights
	// --------------------------------------------------------

	delete[] featureWeights;

	featureWeights = new float[
		static_cast<size_t>(NUM_FEATURES + 1)
			* FEATURE_DIM
	];


	// --------------------------------------------------------
	// Read feature weights
	// --------------------------------------------------------

	const size_t featureWeightCount =
		static_cast<size_t>(NUM_FEATURES + 1)
		* FEATURE_DIM;

	if (std::fread(
		featureWeights,
		sizeof(float),
		featureWeightCount,
		file
	) != featureWeightCount)
	{
		std::printf(
			"NNUE: Could not read feature weights.\n"
		);

		std::fclose(file);

		delete[] featureWeights;
		featureWeights = nullptr;

		return false;
	}


	// --------------------------------------------------------
	// Read FC1
	//
	// [32][512]
	// --------------------------------------------------------

	const size_t fc1WeightCount =
		static_cast<size_t>(HIDDEN1)
		* FEATURE_DIM
		* 2;

	if (std::fread(
		fc1Weights,
		sizeof(float),
		fc1WeightCount,
		file
	) != fc1WeightCount)
	{
		std::printf(
			"NNUE: Could not read fc1 weights.\n"
		);

		std::fclose(file);
		return false;
	}

	if (std::fread(
		fc1Bias,
		sizeof(float),
		HIDDEN1,
		file
	) != HIDDEN1)
	{
		std::printf(
			"NNUE: Could not read fc1 bias.\n"
		);

		std::fclose(file);
		return false;
	}


	// --------------------------------------------------------
	// Read FC2
	// --------------------------------------------------------

	const size_t fc2WeightCount =
		static_cast<size_t>(HIDDEN2)
		* HIDDEN1;

	if (std::fread(
		fc2Weights,
		sizeof(float),
		fc2WeightCount,
		file
	) != fc2WeightCount)
	{
		std::printf(
			"NNUE: Could not read fc2 weights.\n"
		);

		std::fclose(file);
		return false;
	}

	if (std::fread(
		fc2Bias,
		sizeof(float),
		HIDDEN2,
		file
	) != HIDDEN2)
	{
		std::printf(
			"NNUE: Could not read fc2 bias.\n"
		);

		std::fclose(file);
		return false;
	}


	// --------------------------------------------------------
	// Read FC3
	// --------------------------------------------------------

	if (std::fread(
		fc3Weights,
		sizeof(float),
		HIDDEN2,
		file
	) != HIDDEN2)
	{
		std::printf(
			"NNUE: Could not read fc3 weights.\n"
		);

		std::fclose(file);
		return false;
	}

	if (std::fread(
		&fc3Bias,
		sizeof(float),
		1,
		file
	) != 1)
	{
		std::printf(
			"NNUE: Could not read fc3 bias.\n"
		);

		std::fclose(file);
		return false;
	}


	std::fclose(file);


	std::printf(
		"NNUE: Loaded weights from %s\n",
		filename
	);

	std::printf(
		"NNUE: Feature weights: %u x %u\n",
		numFeatures,
		featureDim
	);

	std::printf(
		"NNUE: Network: 512 -> 32 -> 32 -> 1\n"
	);


	return true;
}


// ============================================================
// HalfKP helpers
// ============================================================

int NNUE::FlipSquare(int square)
{
	/*
		Python:

			return square ^ 56

		a1 -> a8
		b1 -> b8
		etc.
	*/

	return square ^ 56;
}


// ------------------------------------------------------------
// Convert engine piece value to HalfKP piece type:
//
// Pawn   = 0
// Knight = 1
// Bishop = 2
// Rook   = 3
// Queen  = 4
//
// King is not a feature.
// ------------------------------------------------------------

int NNUE::PieceType(int piece)
{
	int absolutePiece = piece;

	if (absolutePiece < 0)
		absolutePiece = -absolutePiece;

	switch (absolutePiece)
	{
	case Pawn:
		return 0;

	case Knight:
		return 1;

	case Bishop:
		return 2;

	case Rook:
		return 3;

	case Queen:
		return 4;

	default:
		return -1;
	}
}


// ------------------------------------------------------------
// Engine piece colour:
//
// 0 = White
// 1 = Black
// ------------------------------------------------------------

int NNUE::PieceColour(int piece)
{
	if (piece > 0)
		return 0;

	return 1;
}


// ------------------------------------------------------------
// Exact Python halfkp_index()
//
// piece_index = piece_type * 2 + piece_colour
//
// feature_index = piece_square
//               + (piece_index + king_square * 10) * 64
// ------------------------------------------------------------

int NNUE::HalfKPIndex(
	int kingSquare,
	int pieceSquare,
	int pieceType,
	int colour
)
{
	const int pieceIndex =
		pieceType * 2 + colour;

	return pieceSquare
		+ (pieceIndex + kingSquare * 10) * 64;
}


// ============================================================
// Accumulator handling
// ============================================================

void NNUE::ClearAccumulators()
{
	std::memset(
		whiteAccumulator,
		0,
		sizeof(whiteAccumulator)
	);

	std::memset(
		blackAccumulator,
		0,
		sizeof(blackAccumulator)
	);
}


// ------------------------------------------------------------
// Add one HalfKP feature to an accumulator.
// ------------------------------------------------------------

void NNUE::AddFeature(
	float* accumulator,
	int feature
)
{
	const float* weights =
		featureWeights
		+ static_cast<size_t>(feature) * FEATURE_DIM;

	for (int i = 0; i < FEATURE_DIM; ++i)
	{
		accumulator[i] += weights[i];
	}
}


// ============================================================
// Build both HalfKP accumulators
// ============================================================

void NNUE::BuildAccumulators(const Brain& brain)
{
	ClearAccumulators();


	// --------------------------------------------------------
	// Find both kings.
	//
	// MailboxBoard64:
	//
	//   0 = a1
	//   ...
	//   63 = h8
	//
	// White pieces are positive.
	// Black pieces are negative.
	// --------------------------------------------------------

	int whiteKingSquare = -1;
	int blackKingSquare = -1;


	for (int square = 0; square < 64; ++square)
	{
		const int piece =
			static_cast<int>(brain.MailboxBoard64[square]);

		if (piece == King)
		{
			whiteKingSquare = square;
		}
		else if (piece == -King)
		{
			blackKingSquare = square;
		}
	}


	if (whiteKingSquare < 0 ||
		blackKingSquare < 0)
	{
		return;
	}


	// --------------------------------------------------------
	// Build features for every non-king piece.
	// --------------------------------------------------------

	for (int square = 0; square < 64; ++square)
	{
		const int piece =
			static_cast<int>(brain.MailboxBoard64[square]);

		if (piece == Empty)
			continue;

		// ----------------------------------------------------
		// Kings are the HalfKP reference square and are not
		// themselves HalfKP features.
		// ----------------------------------------------------

		if (piece == King ||
			piece == -King)
		{
			continue;
		}


		const int ptype =
			PieceType(piece);

		if (ptype < 0)
			continue;


		const int originalColour =
			PieceColour(piece);


		// ====================================================
		// WHITE PERSPECTIVE
		//
		// No board transformation.
		// ====================================================

		const int whiteFeature =
			HalfKPIndex(
				whiteKingSquare,
				square,
				ptype,
				originalColour
			);

		AddFeature(
			whiteAccumulator,
			whiteFeature
		);


		// ====================================================
		// BLACK PERSPECTIVE
		//
		// Board vertically flipped.
		// Piece colours reversed.
		// ====================================================

		const int blackKingSquareFlipped =
			FlipSquare(blackKingSquare);

		const int flippedPieceSquare =
			FlipSquare(square);

		const int blackPerspectiveColour =
			1 - originalColour;

		const int blackFeature =
			HalfKPIndex(
				blackKingSquareFlipped,
				flippedPieceSquare,
				ptype,
				blackPerspectiveColour
			);

		AddFeature(
			blackAccumulator,
			blackFeature
		);
	}
}


// ============================================================
// ReLU
// ============================================================

float NNUE::ReLU(float value)
{
	return value > 0.0f
		? value
		: 0.0f;
}


// ============================================================
// Run network
// ============================================================

float NNUE::RunNetwork(int sideToMove)
{
	// --------------------------------------------------------
	// Input ordering must exactly match Python:
	//
	//     first = black if stm else white
	//     second = white if stm else black
	//
	// Assuming:
	//
	//     0 = White
	//     1 = Black
	// --------------------------------------------------------

	const float* first;
	const float* second;


	if (sideToMove == 0)
	{
		first = whiteAccumulator;
		second = blackAccumulator;
	}
	else
	{
		first = blackAccumulator;
		second = whiteAccumulator;
	}


	// --------------------------------------------------------
	// Concatenate the two accumulators.
	// --------------------------------------------------------

	for (int i = 0; i < FEATURE_DIM; ++i)
	{
		input[i] = first[i];
		input[FEATURE_DIM + i] = second[i];
	}


	// --------------------------------------------------------
	// FC1 + ReLU
	// --------------------------------------------------------

	for (int out = 0; out < HIDDEN1; ++out)
	{
		float sum = fc1Bias[out];

		for (int in = 0; in < FEATURE_DIM * 2; ++in)
		{
			sum +=
				fc1Weights[out][in]
				* input[in];
		}

		hidden1[out] = ReLU(sum);
	}


	// --------------------------------------------------------
	// FC2 + ReLU
	// --------------------------------------------------------

	for (int out = 0; out < HIDDEN2; ++out)
	{
		float sum = fc2Bias[out];

		for (int in = 0; in < HIDDEN1; ++in)
		{
			sum +=
				fc2Weights[out][in]
				* hidden1[in];
		}

		hidden2[out] = ReLU(sum);
	}


	// --------------------------------------------------------
	// FC3
	// --------------------------------------------------------

	float output = fc3Bias;

	for (int in = 0; in < HIDDEN2; ++in)
	{
		output +=
			fc3Weights[in]
			* hidden2[in];
	}


	return output;
}


// ============================================================
// Public evaluation function
// ============================================================

float NNUE::Evaluate(
	const Brain& brain,
	int sideToMove
)
{
	if (featureWeights == nullptr)
	{
		std::printf(
			"NNUE: Evaluate called before weights loaded.\n"
		);

		return 0.0f;
	}


	BuildAccumulators(brain);

	return RunNetwork(sideToMove);
}