#pragma once

//#include <atomic>

//----------------------------------------------------------------------------------------------------

class Perft
{
public:
	static const uint32_t PerftTranspositionTableEntriesPerBucket = 4;
	//static const uint64_t nodesMask = 0xFFFFFFFFFF;
	static const uint64_t nodesMask = 0xFFFFFFFFFFFFFF;
	static const uint64_t subTreeDepthMask = 0x3F;
	static const uint64_t ageMask = 3;

	// In Perft, using a 64-bit hash is fine up to a point but eventually collisions become unavoidable so you would have to switch to a perfect hashing scheme for deeper depths
	struct PerftTranspositionTableEntry_Struct //16
	{
		// N.B. Microsoft pack structures to be a multiple of the largest field, in this case 8-bytes (uint64_t)

		// This structure is 16 bytes which means that we can fit 4 entries per 'bucket' (64-byte cache line)
		uint64_t hash64; //8
		//uint64_t data; //8 - 0-39=nodes, 40-47=ageAndSubTreeDepth, 48-63=occupiedFolded
		uint64_t data; //8 - 0-55=nodes, 56-63=ageAndSubTreeDepth - 'age' used for when we search a number of similar positions sequentially
	};

	struct PerftTranspositionTableBucket_Struct // 64
	{
		// A transposition table bucket typically will be 64 bytes, the same size as a memory cache line
		PerftTranspositionTableEntry_Struct Entries[PerftTranspositionTableEntriesPerBucket];
	};

	struct PerftResult_Struct
	{
		uint64_t perftNodes;
		uint64_t perftNodesOverflows;
		uint64_t ms;
		uint64_t perftStores, perftStoresSuccessful, perftProbes, perftProbesSuccessful, perftPositionsFromTranspositionTable;
	};

	void static ClearPerftTranspositionTable();
	void static AllocatePerftTranspositionTable();
	uint32_t static HashfullPerftTranspositionTable();
	std::string static StatsPerftTranspositionTable();
	void AddToPerftTranspositionTable(Perft::PerftTranspositionTableEntry_Struct* tte0, uint8_t depthRemaining, uint64_t nodes64);

	void TreeSearchPerft(int ply, int sideToMove, int isInCheck);
	PerftResult_Struct ComputePerft();
	static void ComputePerftMTLaunchHelperThread(int threadId);
	static PerftResult_Struct ComputePerftMT(bool clearTT);
	static void ComputePerftFile(std::string filename);
	static void ComputePerftWrapper();
	void static Perft::ComputePerftUnique();
	void static Perft::ComputePerftUnique2();

	int ThreadId = -1;

	static PerftTranspositionTableBucket_Struct* PerftTranspositionTablePointer;
	static uint32_t PerftTranspositionTableBuckets;
	static uint32_t PerftTranspositionTableBucketsMask;
	static int PerftDepth;
	static bool PerftSilent;

private:
	Brain perftBrain;

	uint64_t perftNodes; // N.B. counts are 64-bit and MAY overflow!
	uint64_t previousPerftNodes;
	uint64_t perftNodesOverflows;
	uint64_t perftStores, perftStoresSuccessful, perftProbes, perftProbesSuccessful, perftPositionsFromTranspositionTable;
	std::chrono::time_point<std::chrono::steady_clock> startClock, previousClock, nowClock;
};

//----------------------------------------------------------------------------------------------------

#ifdef EXPERIMENTAL
void Unique();
#endif // EXPERIMENTAL
