#pragma once

#include <cstdint>


class Brain;


class NNUE
{
public:

	static constexpr int NUM_FEATURES = 40960;
	static constexpr int PADDING_INDEX = 40960;
	static constexpr int FEATURE_DIM = 256;

	static constexpr int HIDDEN1 = 32;
	static constexpr int HIDDEN2 = 32;

	NNUE();
	~NNUE();

	bool LoadWeights(const char* filename);

	/*
		Evaluate the current position.

		sideToMove:
			0 = White
			1 = Black

		Returns:
			Evaluation in pawns from the side-to-move perspective.
	*/
	float Evaluate(const Brain& brain, int sideToMove);

private:

	// --------------------------------------------------------
	// HalfKP feature weights
	//
	// [feature][256]
	//
	// There are 40961 rows:
	//
	//   0 .. 40959 = real features
	//   40960      = padding
	// --------------------------------------------------------

	float* featureWeights;


	// --------------------------------------------------------
	// First fully-connected layer
	//
	// PyTorch shape:
	//
	//     [32][512]
	// --------------------------------------------------------

	float fc1Weights[HIDDEN1][FEATURE_DIM * 2];
	float fc1Bias[HIDDEN1];


	// --------------------------------------------------------
	// Second fully-connected layer
	//
	// [32][32]
	// --------------------------------------------------------

	float fc2Weights[HIDDEN2][HIDDEN1];
	float fc2Bias[HIDDEN2];


	// --------------------------------------------------------
	// Output layer
	//
	// [1][32]
	// --------------------------------------------------------

	float fc3Weights[HIDDEN2];
	float fc3Bias;


	// --------------------------------------------------------
	// Temporary accumulators
	//
	// These are rebuilt from scratch by this reference
	// implementation.
	// --------------------------------------------------------

	float whiteAccumulator[FEATURE_DIM];
	float blackAccumulator[FEATURE_DIM];


	// --------------------------------------------------------
	// Temporary dense-layer buffers
	// --------------------------------------------------------

	float input[FEATURE_DIM * 2];
	float hidden1[HIDDEN1];
	float hidden2[HIDDEN2];


	// --------------------------------------------------------
	// HalfKP functions
	// --------------------------------------------------------

	static int FlipSquare(int square);

	static int PieceType(int piece);

	static int PieceColour(int piece);

	static int HalfKPIndex(
		int kingSquare,
		int pieceSquare,
		int pieceType,
		int colour
	);


	void ClearAccumulators();

	void AddFeature(
		float* accumulator,
		int feature
	);

	void BuildAccumulators(const Brain& brain);

	float RunNetwork(int sideToMove);


	static float ReLU(float value);
};
