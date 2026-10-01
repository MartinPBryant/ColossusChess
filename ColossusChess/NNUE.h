#pragma once

#include <string>

class Brain;

//----------------------------------------------------------------------------------------------------

// A Chess768 network has two layers
// l0 (input): 768 features with 128 weights each. 128 biases. 2 accumulators (stm/sntm) of 128 entries.
// l1 (output): Takes the 2 concatenated accumulators as input. 256 weights. 1 bias. Outputs the evaluation of the position.

class NNUE
{
public:

	static constexpr int FEATURES_COUNT_CHESS768 = 768;
	static constexpr int FEATURES_WEIGHTS_COUNT_CHESS768 = 128;
	static constexpr int FEATURES_BIASES_COUNT_CHESS768 = FEATURES_WEIGHTS_COUNT_CHESS768;
	static constexpr int OUTPUT_WEIGHTS_COUNT_CHESS768 = FEATURES_WEIGHTS_COUNT_CHESS768 * 2;
	static constexpr int QA = 255;
	static constexpr int QB = 64;
	static constexpr int EVAL_SCALE = 400;

	//----------------------------------------------------------------------------------------------------

	NNUE();
	~NNUE();

	std::string ReadNNUEFromFileChess768(std::string filename);
	std::string ReadNNUEFromResourceChess768();
	void UpdateAccumulatorsChess768(const Brain& brain);
	void InitialiseAccumulatorsChess768(const Brain& brain);
	bool VerifyAccumulatorsChess768(const Brain& brain);
	int16_t EvaluateChess768(const Brain& brain, int sideToMove);
	void NNUE::DumpAccumulatorsChess768(int16_t* acc);

private:

	alignas(32) int16_t* featureWeightsChess768;
	alignas(32) int16_t featureBiasesChess768[FEATURES_BIASES_COUNT_CHESS768];
	alignas(32) int16_t outputWeightsChess768[OUTPUT_WEIGHTS_COUNT_CHESS768];
	int16_t outputBiasChess768;

	int RunNetworkChess768(const Brain& brain, int sideToMove);
	void AddFeatureToAccumulatorChess768(int16_t* accumulator, int featureIndex);
	void SubtractFeatureFromAccumulatorChess768(int16_t* accumulator, int featureIndex);
	int FeatureIndexChess768(int pieceSquare, int pieceType, int colour);
};
