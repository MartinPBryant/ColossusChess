#include "Polyglot.h"

// See https://hgm.nubati.net/book_format.html

//-------------------------------------------------------------------------------------------------

Polyglot::Polyglot()
{
}

Polyglot::~Polyglot()
{
}

uint64_t Polyglot::Hash64(int8_t MailboxBoard64[64], int sideToMove, GameRecordCastlingStatus_Union castling, int epSquare)
{
	uint64_t hash64 = 0;

	// Pieces
	for (int square = A1; square <= H8; square++)
	{
		int8_t piece = MailboxBoard64[square];
		if (piece != 0)
		{
			int pieceCode;
			if (piece > 0)
				pieceCode = (piece - 1) * 2 + 1;
			else
				pieceCode = (-piece - 1) * 2;
			int index;
			index = 64 * pieceCode + 8 * (square / 8) + (square % 8);

			hash64 ^= Randoms[index];
		}
	}

	// Side to move
	if (sideToMove == 0)
		hash64 ^= Randoms[780];

	// Castling
	if (castling.ui8[0][0] == 0)
		hash64 ^= Randoms[768];
	if (castling.ui8[0][1] == 0)
		hash64 ^= Randoms[769];
	if (castling.ui8[1][0] == 0)
		hash64 ^= Randoms[770];
	if (castling.ui8[1][1] == 0)
		hash64 ^= Randoms[771];

	// En-passent
	if (epSquare != 0)
		hash64 ^= Randoms[772 + (epSquare % 8)];

	return hash64;
}

PolyglotEntry_Struct Polyglot::GetBookMove(uint64_t hash64)
{
	PolyglotEntry_Struct result{};

	int64_t bookFileEntries = BookFileSize.QuadPart / sizeof(PolyglotEntry_Struct);
	const PolyglotEntry_Struct* entries = reinterpret_cast<const PolyglotEntry_Struct*>(BookFileData);

	int64_t low = 0;
	int64_t high = bookFileEntries - 1;
	int64_t first = -1;
	int64_t last;

	// Perform binary search to find the first/last occurrence(s) of the key
	while (low <= high)
	{
		int64_t mid = low + (high - low) / 2;
		uint64_t key = _byteswap_uint64(entries[mid].key); // polyglot files are stored big-endian(!) so swap the bytes

		if (key >= hash64)
		{
			if (key == hash64)
			{
				first = mid; // Found a match so scan for the first/last
				last = first;
				while (last < bookFileEntries - 1)
				{
					if (_byteswap_uint64(entries[last + 1].key) == hash64)
						last++;
					else
						break;
				}
				while (first > 0)
				{
					if (_byteswap_uint64(entries[first - 1].key) == hash64)
						first--;
					else
						break;
				}
				break;
			}
			high = mid - 1;
		}
		else
			low = mid + 1;
	}

	if (first != -1)
	{
		// Calculate the total weight of all possible moves
		int totalWeight = 0;
		for (int64_t i = first; i <= last; ++i)
			totalWeight += _byteswap_ushort(entries[i].weight);

		if (totalWeight > 0) // In case there are only 'excluded' moves
		{
			int filterWeight = (Random64() % totalWeight) + 1;

			// Find the selected random index
			int cumulativeWeight = 0;
			int64_t selectedIndex;
			for (int64_t i = first; i <= last; ++i)
			{
				cumulativeWeight += _byteswap_ushort(entries[i].weight);
				if (cumulativeWeight >= filterWeight)
				{
					selectedIndex = i;
					break;
				}
			}
			assert((selectedIndex >= first) && (selectedIndex >= first));

			result = entries[selectedIndex];
		}
	}

	return result;
}
