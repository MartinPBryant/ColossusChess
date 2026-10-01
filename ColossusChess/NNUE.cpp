#define NOMINMAX // Need to include this to stop windows.h (below) breaking std::min etc
#include <algorithm> // Needed to make max/min compile!
#include <assert.h>
#include <windows.h>
#include <cstdint>
#include <vector>
#include <stdexcept>

#include "nnue.h"
#include "BitBoard.h"
#include "Engine.h"
#include "GlobalTypes.h"
#include "Utilities.h"
#include "Brain.h"

//----------------------------------------------------------------------------------------------------

NNUE::NNUE()
{
	featureWeightsChess768 = nullptr;
}

NNUE::~NNUE()
{
	// Delete dynamically allocated array
	delete[] featureWeightsChess768;
	featureWeightsChess768 = nullptr;
}

//----------------------------------------------------------------------------------------------------

std::string NNUE::ReadNNUEFromFileChess768(std::string filename)
{
	FILE* file = nullptr;

	if (fopen_s(&file, filename.c_str(), "rb") != 0)
		return "File not found";
	
	fseek(file, 0L, SEEK_END);
	long size = ftell(file);
	fseek(file, 0L, SEEK_SET);
	if (size != 197440)
	{
		fclose(file);
		return "File length wrong";
	}

	// Allocate feature weights
	delete[] featureWeightsChess768;
	featureWeightsChess768 = new int16_t[FEATURES_COUNT_CHESS768 * FEATURES_WEIGHTS_COUNT_CHESS768];

	// Read feature weights and biases
	std::fread(featureWeightsChess768, sizeof(int16_t), FEATURES_COUNT_CHESS768 * FEATURES_WEIGHTS_COUNT_CHESS768, file);
	std::fread(featureBiasesChess768, sizeof(int16_t), FEATURES_WEIGHTS_COUNT_CHESS768, file);

	// Read output layer weights and biases
	std::fread(outputWeightsChess768, sizeof(int16_t), OUTPUT_WEIGHTS_COUNT_CHESS768, file);
	std::fread(&outputBiasChess768, sizeof(int16_t), 1, file);

	// Ensure we've read everything correctly and that this is a Bullet trainer file! This should be 'bulletbullet...bu'
	char tail[62];
	std::fread(&tail, 1, 62, file);
	if (memcmp(tail, "bullet", 6))
	{
		fclose(file);
		return "File format wrong";
	}

	fclose(file);

	return "";
}

std::string NNUE::ReadNNUEFromResourceChess768()
{
#define IDR_CHESS768 101

	HMODULE hModule = GetModuleHandle(nullptr);
	HRSRC hResource = FindResource(hModule, MAKEINTRESOURCE(IDR_CHESS768), RT_RCDATA);
	if (!hResource)
		return "NNUE resource not found";

	DWORD size = SizeofResource(hModule, hResource);
	if (size != 197440)
		return "Resource length wrong";

	HGLOBAL hLoaded = LoadResource(hModule, hResource);
	if (!hLoaded)
		return "Unable to load resource";

	const int16_t* data = (int16_t*)LockResource(hLoaded);

	delete[] featureWeightsChess768;
	featureWeightsChess768 = new int16_t[FEATURES_COUNT_CHESS768 * FEATURES_WEIGHTS_COUNT_CHESS768];
	memcpy(featureWeightsChess768, data, FEATURES_COUNT_CHESS768 * FEATURES_WEIGHTS_COUNT_CHESS768 * 2);
	memcpy(featureBiasesChess768, data + (FEATURES_COUNT_CHESS768 * FEATURES_WEIGHTS_COUNT_CHESS768), FEATURES_WEIGHTS_COUNT_CHESS768 * 2);
	memcpy(outputWeightsChess768, data + (FEATURES_COUNT_CHESS768 * FEATURES_WEIGHTS_COUNT_CHESS768) + (FEATURES_WEIGHTS_COUNT_CHESS768), OUTPUT_WEIGHTS_COUNT_CHESS768 * 2);
	memcpy(&outputBiasChess768, data + (FEATURES_COUNT_CHESS768 * FEATURES_WEIGHTS_COUNT_CHESS768) + FEATURES_WEIGHTS_COUNT_CHESS768 + OUTPUT_WEIGHTS_COUNT_CHESS768, 1 * 2);

	return "";
}

//----------------------------------------------------------------------------------------------------

// Calculate the evaluation for the position
//int NNUE::RunNetworkChess768(const Brain& brain, int sideToMove)
//{
//	const int16_t* stm;
//	const int16_t* sntm;
//
//	if (sideToMove == 0)
//	{
//		stm = &brain.GameRecordPointer->nnueAccumulatorsChess768[0][0];
//		sntm = &brain.GameRecordPointer->nnueAccumulatorsChess768[1][0];
//	}
//	else
//	{
//		stm = &brain.GameRecordPointer->nnueAccumulatorsChess768[1][0];
//		sntm = &brain.GameRecordPointer->nnueAccumulatorsChess768[0][0];
//	}
//
//	int32_t output = 0;
//
//	// Side-to-move accumulator
//	for (int i = 0; i < FEATURES_WEIGHTS_COUNT_CHESS768; ++i)
//	{
//		int32_t x = stm[i];
//
//		// SCReLU: clamp(x, 0, QA) then square it
//		x = std::max(0, std::min(QA, x));
//
//		output += x * x * static_cast<int32_t>(outputWeightsChess768[i]);		
//	}
//
//	// Side-not-to-move accumulator
//	for (int i = 0; i < FEATURES_WEIGHTS_COUNT_CHESS768; ++i)
//	{
//		int32_t x = sntm[i];
//
//		// SCReLU: clamp(x, 0, QA) then square it
//		x = std::max(0, std::min(QA, x));
//
//		output += x * x * static_cast<int32_t>(outputWeightsChess768[FEATURES_WEIGHTS_COUNT_CHESS768 + i]);
//	}
//
//	// --------------------------------------------------------
//	// SCReLU gives us a QA� scale.
//	//
//	// l1 (output) weights have QB scale.
//	//
//	// Therefore output currently has:
//	//     QA� * QB
//	//
//	// But l1 bias was quantized at:
//	//     QA * QB
//	//
//	// Reduce by QA before adding the bias.
//	// --------------------------------------------------------
//
//	output /= QA;
//
//	output += static_cast<int32_t>(outputBiasChess768);
//
//	// Convert network output to centipawns (eval_scale = 400)
//	output *= EVAL_SCALE;
//
//	output /= (QA * QB);
//
//	return output;
//}

// Calculate the evaluation for the position
// This AVX2 function was provided by ChatGPT. I haven't tried to totally understand it but it seems to behave identically to the scalar function above and is much faster!
int NNUE::RunNetworkChess768(const Brain& brain, int sideToMove)
{
	const int16_t* stm;
	const int16_t* sntm;

	if (sideToMove == 0)
	{
		stm = &brain.GameRecordPointer->nnueAccumulatorsChess768[0][0];
		sntm = &brain.GameRecordPointer->nnueAccumulatorsChess768[1][0];
	}
	else
	{
		stm = &brain.GameRecordPointer->nnueAccumulatorsChess768[1][0];
		sntm = &brain.GameRecordPointer->nnueAccumulatorsChess768[0][0];
	}

	const __m256i zero = _mm256_setzero_si256();
	const __m256i qa = _mm256_set1_epi16(QA);

	// We have 128 STM inputs + 128 SNTM inputs = 256 l1 inputs.
	//
	// Accumulate in 64 bits to avoid overflow.
	__m256i sum0 = _mm256_setzero_si256();
	__m256i sum1 = _mm256_setzero_si256();
	__m256i sum2 = _mm256_setzero_si256();
	__m256i sum3 = _mm256_setzero_si256();

	// ------------------------------------------------------------
	// Process both accumulators.
	//
	// Each accumulator contains 128 int16 values.
	// There are 16 values per AVX2 register.
	// ------------------------------------------------------------

	for (int i = 0; i < FEATURES_WEIGHTS_COUNT_CHESS768; i += 16)
	{
		// ============================================================
		// STM
		// ============================================================

		__m256i x = _mm256_loadu_si256(
			reinterpret_cast<const __m256i*>(stm + i)
		);

		// SCReLU: clamp(x, 0, QA)
		x = _mm256_max_epi16(x, zero);
		x = _mm256_min_epi16(x, qa);

		// Convert 16 x int16 -> two groups of 8 x int32
		__m256i xlo = _mm256_cvtepi16_epi32(
			_mm256_castsi256_si128(x)
		);

		__m256i xhi = _mm256_cvtepi16_epi32(
			_mm256_extracti128_si256(x, 1)
		);

		// Square
		__m256i x2lo = _mm256_mullo_epi32(xlo, xlo);
		__m256i x2hi = _mm256_mullo_epi32(xhi, xhi);

		// Load 16 l1 weights
		__m256i w = _mm256_loadu_si256(
			reinterpret_cast<const __m256i*>(
				outputWeightsChess768 + i
				)
		);

		__m256i wlo = _mm256_cvtepi16_epi32(
			_mm256_castsi256_si128(w)
		);

		__m256i whi = _mm256_cvtepi16_epi32(
			_mm256_extracti128_si256(w, 1)
		);

		// x� * weight
		__m256i plo = _mm256_mullo_epi32(x2lo, wlo);
		__m256i phi = _mm256_mullo_epi32(x2hi, whi);

		// Widen to int64 and accumulate
		sum0 = _mm256_add_epi64(
			sum0,
			_mm256_cvtepi32_epi64(
				_mm256_castsi256_si128(plo)
			)
		);

		sum1 = _mm256_add_epi64(
			sum1,
			_mm256_cvtepi32_epi64(
				_mm256_extracti128_si256(plo, 1)
			)
		);

		sum2 = _mm256_add_epi64(
			sum2,
			_mm256_cvtepi32_epi64(
				_mm256_castsi256_si128(phi)
			)
		);

		sum3 = _mm256_add_epi64(
			sum3,
			_mm256_cvtepi32_epi64(
				_mm256_extracti128_si256(phi, 1)
			)
		);


		// ============================================================
		// SNTM
		// ============================================================

		x = _mm256_loadu_si256(
			reinterpret_cast<const __m256i*>(sntm + i)
		);

		// SCReLU
		x = _mm256_max_epi16(x, zero);
		x = _mm256_min_epi16(x, qa);

		// int16 -> int32
		xlo = _mm256_cvtepi16_epi32(
			_mm256_castsi256_si128(x)
		);

		xhi = _mm256_cvtepi16_epi32(
			_mm256_extracti128_si256(x, 1)
		);

		// Square
		x2lo = _mm256_mullo_epi32(xlo, xlo);
		x2hi = _mm256_mullo_epi32(xhi, xhi);

		// SNTM weights start after the first 128 weights
		w = _mm256_loadu_si256(
			reinterpret_cast<const __m256i*>(
				outputWeightsChess768 +
				FEATURES_WEIGHTS_COUNT_CHESS768 + i
				)
		);

		wlo = _mm256_cvtepi16_epi32(
			_mm256_castsi256_si128(w)
		);

		whi = _mm256_cvtepi16_epi32(
			_mm256_extracti128_si256(w, 1)
		);

		// x� * weight
		plo = _mm256_mullo_epi32(x2lo, wlo);
		phi = _mm256_mullo_epi32(x2hi, whi);

		// Widen to int64 and accumulate
		sum0 = _mm256_add_epi64(
			sum0,
			_mm256_cvtepi32_epi64(
				_mm256_castsi256_si128(plo)
			)
		);

		sum1 = _mm256_add_epi64(
			sum1,
			_mm256_cvtepi32_epi64(
				_mm256_extracti128_si256(plo, 1)
			)
		);

		sum2 = _mm256_add_epi64(
			sum2,
			_mm256_cvtepi32_epi64(
				_mm256_castsi256_si128(phi)
			)
		);

		sum3 = _mm256_add_epi64(
			sum3,
			_mm256_cvtepi32_epi64(
				_mm256_extracti128_si256(phi, 1)
			)
		);
	}

	// ------------------------------------------------------------
	// Horizontally sum the four int64 vectors.
	// ------------------------------------------------------------

	__m256i total01 = _mm256_add_epi64(sum0, sum1);
	__m256i total23 = _mm256_add_epi64(sum2, sum3);
	__m256i total = _mm256_add_epi64(total01, total23);

	alignas(32) int64_t temp[4];
	_mm256_store_si256(
		reinterpret_cast<__m256i*>(temp),
		total
	);

	int64_t output =
		temp[0] + temp[1] + temp[2] + temp[3];

	// ------------------------------------------------------------
	// SCReLU gives us a QA� scale.
	//
	// l1 weights have QB scale.
	//
	// l1 bias has QA * QB scale.
	// Therefore divide the accumulated l1 result by QA
	// before adding the bias.
	// ------------------------------------------------------------

	output /= QA;

	output += static_cast<int32_t>(outputBiasChess768);

	// Convert network output to centipawns.
	output *= EVAL_SCALE;

	output /= (QA * QB);

	return static_cast<int>(output);
}




int16_t NNUE::EvaluateChess768(const Brain& brain, int sideToMove)
{
	//BuildAccumulatorsChess768(brain);//THESE SHOULD BE INCREMENTALLY UPDATED - NEED TO COMPARE!!! AND TIME!

	//bool result = true;
	//for (int i = 0; i < FEATURE_DIM_CHESS768; i++)
	//{
	//	if (whiteAccumulatorChess768[i] != brain.GameRecordPointer->nnueAccumulatorsChess768[0][i])
	//	{
	//		result = false;
	//	}
	//	if (blackAccumulatorChess768[i] != brain.GameRecordPointer->nnueAccumulatorsChess768[1][i])
	//	{
	//		result = false;
	//	}
	//}

	//DON'T NOW NEED THIS FN AS A WRAPPER! CAN JUST CALL RunNetworkChess768 FROM MY EVALUATE.CPP
	int16_t result = RunNetworkChess768(brain, sideToMove);

	return result;
}

//----------------------------------------------------------------------------------------------------

inline int NNUE::FeatureIndexChess768(int square, int pieceType, int colour)
{
	int result = ((colour * 6) + pieceType) * 64 + square;
	assert((result >= 0) && (result < 768));
	return result;
}

// Add one feature to an accumulator
void NNUE::AddFeatureToAccumulatorChess768(int16_t* accumulator, int featureIndex)
{
	// Get a pointer to the weights corresponding to this feature
	alignas(32) const int16_t* weights = featureWeightsChess768 + static_cast<size_t>(featureIndex) * FEATURES_WEIGHTS_COUNT_CHESS768;

	//for (int i = 0; i < FEATURES_WEIGHTS_COUNT_CHESS768; ++i)
	//	accumulator[i] += weights[i];
	for (int i = 0; i < FEATURES_WEIGHTS_COUNT_CHESS768; i += 16)
	{
		__m256i a = _mm256_load_si256(reinterpret_cast<const __m256i*>(accumulator + i));
		__m256i w = _mm256_load_si256(reinterpret_cast<const __m256i*>(weights + i));
		a = _mm256_add_epi16(a, w);
		_mm256_store_si256(reinterpret_cast<__m256i*>(accumulator + i), a);
	}
}

// Subtract one feature from an accumulator
void NNUE::SubtractFeatureFromAccumulatorChess768(int16_t* accumulator, int featureIndex)
{
	// Get a pointer to the weights corresponding to this feature
	const int16_t* weights = featureWeightsChess768 + static_cast<size_t>(featureIndex) * FEATURES_WEIGHTS_COUNT_CHESS768;

	//for (int i = 0; i < FEATURES_WEIGHTS_COUNT_CHESS768; ++i)
	//	accumulator[i] -= weights[i];
	for (int i = 0; i < FEATURES_WEIGHTS_COUNT_CHESS768; i += 16)
	{
		__m256i a = _mm256_load_si256(reinterpret_cast<const __m256i*>(accumulator + i));
		__m256i w = _mm256_load_si256(reinterpret_cast<const __m256i*>(weights + i));
		a = _mm256_sub_epi16(a, w);
		_mm256_store_si256(reinterpret_cast<__m256i*>(accumulator + i), a);
	}
}

// Incrementally update both accumulators
void NNUE::UpdateAccumulatorsChess768(const Brain& brain)
{
	int16_t* nnueAccumulatorsChess768 = &brain.GameRecordPointer->nnueAccumulatorsChess768[0][0];
	MoveUndo_Struct* currentMove = &(brain.GameRecordPointer - 1)->move;

	int movingPieceColour, movingPieceType;

	if (currentMove->mf.flag != MFCastling)
	{
		movingPieceType = std::abs(currentMove->fromSquarePiece) - 1;
		movingPieceColour = (currentMove->fromSquarePiece > 0) ? 0 : 1;
		// Subtract the fromSquarePiece from the fromSquare from the accumulators
		SubtractFeatureFromAccumulatorChess768(nnueAccumulatorsChess768, FeatureIndexChess768(currentMove->mf.fromSquare, movingPieceType, movingPieceColour));
		SubtractFeatureFromAccumulatorChess768(nnueAccumulatorsChess768 + FEATURES_WEIGHTS_COUNT_CHESS768, FeatureIndexChess768(currentMove->mf.fromSquare ^ 56, movingPieceType, 1 - movingPieceColour));
		if (currentMove->mf.flag >= MFPromotion)
		{
			// Add the promoted piece to the accumulators
			movingPieceType = PromotedPieces[currentMove->mf.flag >> 2] - 1;
			AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768, FeatureIndexChess768(currentMove->mf.toSquare, movingPieceType, movingPieceColour));
			AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768 + FEATURES_WEIGHTS_COUNT_CHESS768, FeatureIndexChess768(currentMove->mf.toSquare ^ 56, movingPieceType, 1 - movingPieceColour));
		}
		else if (currentMove->mf.flag != MFEnPassant)
		{
			// Add the fromSquarePiece to the toSquare to the accumulators
			AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768, FeatureIndexChess768(currentMove->mf.toSquare, movingPieceType, movingPieceColour));
			AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768 + FEATURES_WEIGHTS_COUNT_CHESS768, FeatureIndexChess768(currentMove->mf.toSquare ^ 56, movingPieceType, 1 - movingPieceColour));
		}
		// Capture?
		if (currentMove->toSquarePiece != Empty)
		{
			// Subtract the toSquarePiece from the toSquare from the accumulators
			int capturedPieceType = std::abs(currentMove->toSquarePiece) - 1;
			int capturedPieceColour = movingPieceColour ^ 1;
			SubtractFeatureFromAccumulatorChess768(nnueAccumulatorsChess768, FeatureIndexChess768(currentMove->mf.toSquare, capturedPieceType, capturedPieceColour));
			SubtractFeatureFromAccumulatorChess768(nnueAccumulatorsChess768 + FEATURES_WEIGHTS_COUNT_CHESS768, FeatureIndexChess768(currentMove->mf.toSquare ^ 56, capturedPieceType, 1 - capturedPieceColour));
			if (currentMove->mf.flag == MFEnPassant)
			{ // An EP move is stored as e.g. fromSquare=d5, toSquare = e5 (not e6), so we have to move the capturing pawn forward one square
				// Add the fromSquarePiece to the TRUE toSquare to the accumulators
				int toSquare = currentMove->mf.toSquare + 8;
				if (currentMove->mf.fromSquare <= H4)
					toSquare = currentMove->mf.toSquare - 8;
				AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768, FeatureIndexChess768(toSquare, movingPieceType, movingPieceColour));
				AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768 + FEATURES_WEIGHTS_COUNT_CHESS768, FeatureIndexChess768(toSquare ^ 56, movingPieceType, 1 - movingPieceColour));
			}
		}
	}
	else
	{
		// Castling
		movingPieceType = King -1;
		movingPieceColour = (currentMove->fromSquarePiece > 0) ? 0 : 1;

		// Subtract the fromSquarePiece from the fromSquare from the accumulators
		SubtractFeatureFromAccumulatorChess768(nnueAccumulatorsChess768, FeatureIndexChess768(currentMove->mf.fromSquare, movingPieceType, movingPieceColour));
		SubtractFeatureFromAccumulatorChess768(nnueAccumulatorsChess768 + FEATURES_WEIGHTS_COUNT_CHESS768, FeatureIndexChess768(currentMove->mf.fromSquare ^ 56, movingPieceType, 1 - movingPieceColour));
		// Add the fromSquarePiece to the toSquare to the accumulators
		AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768, FeatureIndexChess768(currentMove->mf.toSquare, movingPieceType, movingPieceColour));
		AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768 + FEATURES_WEIGHTS_COUNT_CHESS768, FeatureIndexChess768(currentMove->mf.toSquare ^ 56, movingPieceType, 1 - movingPieceColour));

		// Move the rooks too
		int initialRookSquare, finalRookSquare;
		if (currentMove->mf.toSquare == BackRankBaseSquareIndex[movingPieceColour] + G) // King-side?
		{
			initialRookSquare = BackRankBaseSquareIndex[movingPieceColour] + InitialKingSideRookFile;
			finalRookSquare = BackRankBaseSquareIndex[movingPieceColour] + F;
		}
		else
		{
			initialRookSquare = BackRankBaseSquareIndex[movingPieceColour] + InitialQueenSideRookFile;
			finalRookSquare = BackRankBaseSquareIndex[movingPieceColour] + D;
		}
		SubtractFeatureFromAccumulatorChess768(nnueAccumulatorsChess768, FeatureIndexChess768(initialRookSquare, Rook - 1, movingPieceColour));
		AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768, FeatureIndexChess768(finalRookSquare, Rook - 1, movingPieceColour));
		SubtractFeatureFromAccumulatorChess768(nnueAccumulatorsChess768 + FEATURES_WEIGHTS_COUNT_CHESS768, FeatureIndexChess768(initialRookSquare ^ 56, Rook - 1, 1 - movingPieceColour));
		AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768 + FEATURES_WEIGHTS_COUNT_CHESS768, FeatureIndexChess768(finalRookSquare ^ 56, Rook - 1, 1 - movingPieceColour));
	}
}

// Initialise the accumulators based on the root position
void NNUE::InitialiseAccumulatorsChess768(const Brain& brain)
{
	int16_t* nnueAccumulatorsChess768 = &brain.GameRecordPointer->nnueAccumulatorsChess768[0][0];

	// Clear the accumulators
	std::memset(nnueAccumulatorsChess768, 0, sizeof(int16_t) * NNUE::FEATURES_WEIGHTS_COUNT_CHESS768 * 2);

	// Copy the biases
	for (int i = 0; i < FEATURES_WEIGHTS_COUNT_CHESS768; i++)
	{
		nnueAccumulatorsChess768[i] = featureBiasesChess768[i];
		nnueAccumulatorsChess768[FEATURES_WEIGHTS_COUNT_CHESS768 + i] = featureBiasesChess768[i];
	}

	// Add all the features (32 in the initial position) into both accumulators
	for (int square = 0; square < 64; ++square)
	{
		const int8_t piece = brain.MailboxBoard64[square];
		if (piece == Empty)
			continue;

		const int pieceType = std::abs(piece) - 1;
		const int pieceColour = (piece > 0) ? 0 : 1;
		AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768, FeatureIndexChess768(square, pieceType, pieceColour));
		AddFeatureToAccumulatorChess768(nnueAccumulatorsChess768 + FEATURES_WEIGHTS_COUNT_CHESS768, FeatureIndexChess768(square ^ 56, pieceType, 1 - pieceColour));
	}
}

// Verify the incrementally updated accumulators against locally recreated accumulators (used in Debug mode)
bool NNUE::VerifyAccumulatorsChess768(const Brain& brain)
{
	int16_t nnueAccumulatorsChess768[Sides][128];

	// Clear the accumulators
	std::memset(nnueAccumulatorsChess768, 0, sizeof(int16_t) * FEATURES_WEIGHTS_COUNT_CHESS768 * 2);

	for (int i = 0; i < FEATURES_WEIGHTS_COUNT_CHESS768; i++)
	{
		nnueAccumulatorsChess768[0][i] = featureBiasesChess768[i];
		nnueAccumulatorsChess768[1][i] = featureBiasesChess768[i];
	}

	// Add all the features (32 in the initial position) into both accumulators
	for (int square = 0; square < 64; ++square)
	{
		const int8_t piece = brain.MailboxBoard64[square];
		if (piece == Empty)
			continue;

		const int pieceType = std::abs(piece) - 1;
		const int pieceColour = (piece > 0) ? 0 : 1;
		AddFeatureToAccumulatorChess768(&nnueAccumulatorsChess768[0][0], FeatureIndexChess768(square, pieceType, pieceColour));
		AddFeatureToAccumulatorChess768(&nnueAccumulatorsChess768[0][0] + FEATURES_WEIGHTS_COUNT_CHESS768, FeatureIndexChess768(square ^ 56, pieceType, 1 - pieceColour));
	}

	// Compare them
	bool result = true;
	for (int side = 0; side < Sides; side++)
		for (int i = 0; i < FEATURES_WEIGHTS_COUNT_CHESS768; i++)
			if (nnueAccumulatorsChess768[side][i] != brain.GameRecordPointer->nnueAccumulatorsChess768[side][i])
			{
				result = false;
				goto exit;
			}

exit:
	return result;
}

void NNUE::DumpAccumulatorsChess768(int16_t* acc)
{
	std::string s = "";
	for (int side = 0; side < Sides; side++)
		for (int i = 0; i < FEATURES_WEIGHTS_COUNT_CHESS768; i++)
			s += std::to_string((acc + (side * FEATURES_WEIGHTS_COUNT_CHESS768))[i]) + " ";
	Output(s);
}
