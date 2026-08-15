// This code is simply #include at the top of SearchNormal.cpp
// It is broken out into a separate file for readability

short Normal::TreeSearchNormalQuiescence(short alpha, short beta, int ply, int depthRemaining, int sideToMove, int isInCheck)
{
	assert(CompareMailboxBoard64ToPiecesBB(normalBrain.mailboxBoard64, normalBrain.piecesBB));
	assert((PopulationCountX(normalBrain.piecesBB[0][King]) == 1) && (PopulationCountX(normalBrain.piecesBB[1][King]) == 1));
	assert((PopulationCountX(normalBrain.piecesBB[0][Queen]) <= 9) && (PopulationCountX(normalBrain.piecesBB[1][Queen]) <= 9));
	assert((PopulationCountX(normalBrain.piecesBB[0][Rook]) <= 10) && (PopulationCountX(normalBrain.piecesBB[1][Rook]) <= 10));
	assert((PopulationCountX(normalBrain.piecesBB[0][Bishop]) <= 10) && (PopulationCountX(normalBrain.piecesBB[1][Bishop]) <= 10));
	assert((PopulationCountX(normalBrain.piecesBB[0][Knight]) <= 10) && (PopulationCountX(normalBrain.piecesBB[1][Knight]) <= 10));
	assert((PopulationCountX(normalBrain.piecesBB[0][Pawn]) <= 8) && (PopulationCountX(normalBrain.piecesBB[1][Pawn]) <= 8));
	assert(normalBrain.piecesBB[0][AllPieces] == (normalBrain.piecesBB[0][Pawn] | normalBrain.piecesBB[0][Knight] | normalBrain.piecesBB[0][Bishop] | normalBrain.piecesBB[0][Rook] | normalBrain.piecesBB[0][Queen] | normalBrain.piecesBB[0][King]));
	assert(normalBrain.piecesBB[1][AllPieces] == (normalBrain.piecesBB[1][Pawn] | normalBrain.piecesBB[1][Knight] | normalBrain.piecesBB[1][Bishop] | normalBrain.piecesBB[1][Rook] | normalBrain.piecesBB[1][Queen] | normalBrain.piecesBB[1][King]));
	assert(normalBrain.gameRecordPointer->transpositionTableHash64 == ((sideToMove == 0) ? GenerateTranspositionTableHash64(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer) : ~GenerateTranspositionTableHash64(normalBrain.mailboxBoard64, normalBrain.gameRecordPointer)));
	assert(normalBrain.gameRecordPointer->transpositionTableHash64WithEP == (normalBrain.gameRecordPointer->transpositionTableHash64 ^ TranspositionTableRandomsEnPassant[normalBrain.gameRecordPointer->epSquare]));
	assert((ply >= 1) && (ply <= MaximumPlyInQS));
	assert(depthRemaining <= 0);
	assert((sideToMove >= 0) && (sideToMove < Sides));
	assert(-MatingIn0Score <= alpha && alpha < beta && beta <= MatingIn0Score);
	assert((normalBrain.gameRecordPointer->gamePhase[0] >= 0) && (normalBrain.gameRecordPointer->gamePhase[0] <= 103) && (normalBrain.gameRecordPointer->gamePhase[1] >= 0) && (normalBrain.gameRecordPointer->gamePhase[1] <= 103));
	//assert(depthRemaining >= -30); N.B. we cannot test this as we also search non-captures when in check!

	// The quiescence search mainly tries out capture sequences (the maximum number of captures in a row is 30)
	// N.B. With MVVLVA ordering it's ok to search QxQ before PxR because opponent has to recapture Q to recover his loss already and then you can take the R anyway!
	// When the QS is first entered from the main search, depthRemaining is zero but gets decremented each recursion

	//----------------------------------------------------------------------------------------------------

	// Preamble

	// Deepest so far this iteration?
	if (ply > MaximumPlyReached)
	{
		MaximumPlyReached = ply;
		if (IsDebug)
			LongestLineWithQS = normalBrain.CurrentLine(ply - 1) + " (Iteration:" + MyITOA(IterationPly) + " Ply:" + MyITOA(ply - 1) + " Alpha:" + MyITOA(alpha) + " Beta:" + MyITOA(beta) + ")";
	}

	// At maximum depth possible?
	if (ply >= MaximumPlyInQS)
	{
		//OutputError("Reached MaximumPly in QS!\nIterationPly=" + MyITOA(IterationPly) + "\nCurrent line=" + normalBrain.CurrentLine(ply));
		return Evaluate(sideToMove);
	}

	NodeCountQuiescenceSearch++; // About 67% of all searched nodes are in the QS

	short originalAlpha = alpha;
	bool isPVNode = (alpha != beta - 1);
	short bestMoveScore = -MatingIn0Score; // If anything takes over as best (a 'pv' or 'cut' node) then bestMoveScore will be equal to alpha. If nothing takes over as best (an 'all' node) then bestMoveScore will be less than alpha and will be a more accurate upper bound.
	GameRecordEntry_Struct* currentGameRecordPointer = normalBrain.gameRecordPointer;
	short drawScore = DrawScore(sideToMove);

	//----------------------------------------------------------------------------------------------------

#pragma region Draws
	// Drawn?
	// Although the QS is mainly captures the first two ply could be a check and a move out of check so DBRs could occur. Also, the last move in the main search could have caused a draw.
	int pliesSinceIrreversible = currentGameRecordPointer->pliesSinceIrreversible;
	if (pliesSinceIrreversible >= 3)
	{
		short nonStickyDrawScore = drawScore + (NodeCount & 1) * 2 - 1; // Add +/-1 randomly to avoid DBR stickiness
		assert(nonStickyDrawScore != drawScore);

		// 'Immediate' draw-by-repetition
		if (nonStickyDrawScore > alpha) // Immediate repetition possible?
		{
			if (
				(((currentGameRecordPointer - 1)->move.mf.fromSquare) == ((currentGameRecordPointer - 3)->move.mf.toSquare))
				&& (((currentGameRecordPointer - 1)->move.mf.toSquare) == ((currentGameRecordPointer - 3)->move.mf.fromSquare))
				) // Did the opponent just undo his previous move?
			{
				if (nonStickyDrawScore >= beta)
				{
					*currentGameRecordPointer->principalVariationPointer = PVTDrawImmediateRepetition; // Should NEVER see this on the end of a PV!!!
					return nonStickyDrawScore;
				}
				// Must be a PV node
				// It is tempting to increase alpha here but it seems problematic.
				// SF sets alpha to ds but when the 'drawing' move is searched later it won't become part of the PV
				// Setting it to ds-1 seemed to lose a few ELO
			}
		}

		// Draw-by-repetition
		int cycles = 4;
		int requiredRepeats = 1;
		//cycles = 8;//TEST
		//requiredRepeats = 2;

		if (pliesSinceIrreversible >= cycles)
		{
			// About 9% of nodes get tested here
			int repeats = 0;
			for (int i = 4; i <= pliesSinceIrreversible; i += 2) // Repetition?
			{
				// Make sure we haven't gone past the start of the array. This could happen if a position is setup from a FEN string which provides a high half-move count (for the 50-move rule).
				if ((currentGameRecordPointer - i) < &normalBrain.gameRecord[2])
					break;

				if ((currentGameRecordPointer - i)->transpositionTableHash64 == currentGameRecordPointer->transpositionTableHash64)
				{
					repeats++;
					if (repeats == requiredRepeats)
					{
						*currentGameRecordPointer->principalVariationPointer = PVTDrawByRepetition;
						return nonStickyDrawScore;
					}
				}
			}

			// 50-move draw
			// In this position 6k1/5pp1/4p3/1bBpP1P1/1P1P1P2/1q6/7Q/K7 w - - 99 90 Colossus played Qh5 which allows a mate in 2! :O
			// (In fact, it could have played any move including leaving a piece en-prise! Or at the 99th ply it could have allowed a 'winning' knight fork at the 100th ply!)
			// This dumb 'bowel trembling' behaviour has occurred in other games too!
			// Thankfully the GUI declared it a draw!
			if (pliesSinceIrreversible >= 100) // 50-moves made?
			{
				if (isInCheck) // Being mated on the 100th ply takes precedence over the draw!
				{
					normalBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
					if (normalBrain.CountAllMoves(sideToMove, true) == 0)
					{
						*currentGameRecordPointer->principalVariationPointer = PVTCheckmate;
						return (short)(-MatingIn0Score + ply);
					}
				}
				*currentGameRecordPointer->principalVariationPointer = PVTDrawBy50MoveRule;
				return drawScore;
			}
		}
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------

#pragma region Mate-distance pruning
	// 'Mate-distance pruning' (Helps massively when we have a mate score)
	// Ensures that we don't go deeper than the mate we've already got...
	// e.g. if we have a #10 we won't go deeper than ply=19
	// e.g. if we have a #-10 we won't go deeper than ply=20
	// (In the 'alpha' line below we could test for 'isInCheck' and add +2 if we're not, but although it does make a slight difference it seems to harm the search depth rather than helping it!?)
	// This is also included in the QS because reductions can cause you to enter the QS early and bypass this test in main.
	// Because EITHER alpha gets increased, OR beta gets decreased, but not BOTH, you can omit the alpha test to save a few cycles during most normal searches with NO change in node count when you are mating (because the beta test is always hit a ply sooner than the alpha test)
	// In fact, I think that the alpha test only ever does something if alpha has been set to -INF
	//alpha = std::max(-MateBaseScore + ply, (int)alpha); // If the worst possible score for the side to move in this position (i.e. being mated here) is > alpha, then increase alpha
	beta = std::min(MatingIn0Score - ply - 1, (int)beta); // If the best possible score for the side to move in this position (i.e. giving mate in 1) < beta, then lower beta
	if (alpha >= beta)
		return alpha;
#pragma endregion

	//----------------------------------------------------------------------------------------------------

	// Initialisation
	MoveWithScore_Struct moveList[220];
	int legalMovesMade;
	short currentMoveScore;
	Move_Struct currentMove;
	short futilityBaseScore;
	short standPatScore = INT16_MIN; // This may be retrieved from the TT below

	currentGameRecordPointer->isTWM = 0; // These may get set if we find a TT entry
	currentGameRecordPointer->isO1M = 0;
	currentGameRecordPointer->isFMTP = 0;
	currentGameRecordPointer->isZLKM = 0;
	currentGameRecordPointer->isO1PCM = 0;
	*currentGameRecordPointer->principalVariationPointer = PVTUnknown; // Default PV terminator
#ifdef _DEBUG
	isFollowingPV = false;
#endif

	//----------------------------------------------------------------------------------------------------

#pragma region TT
	// Is this position in the tranposition table?
	NormalTranspositionTableEntry_Struct* tte0;
	Move_Struct tteBestMove;
	tteBestMove.ui32 = 0;
	short tteLowerLimitScore = -MatingIn0Score;
	int8_t tteSubTreeDepth; // Useful for debugging to declare this outside the block below
	int tteFound = -1;
	if (NormalTranspositionTableBuckets > 0)
	{
		//normalProbes++;

		uint64_t hash64 = currentGameRecordPointer->transpositionTableHash64WithEP;
		tte0 = (NormalTranspositionTableEntry_Struct*)(NormalTranspositionTablePointer + (hash64 & NormalTranspositionTableBucketsMask));

		bool found = false;
		for (int entry = 0; entry < NormalTranspositionTableEntriesPerBucket; entry++) // Do we already have this position in the table?
		{
			uint64_t data = tte0[entry].data;
			uint64_t hash = tte0[entry].hash64 ^ data;

			if (hash == hash64)
			{
				tteFound = entry;

				// Get the entry's data
				//tteBestMove.ui32 = MGUnCompressMove((uint16_t)(data & bestMoveMask));
				tteBestMove.ui32 = MGUnCompressMove(((NormalTranspositionTableEntryDataFields_Struct*)&data)->bestMove);
				assert((tteBestMove.ui32 == 0) == (((uint16_t)tteBestMove.ui32) == 0));
				//tteSubTreeDepth = (int8_t)((data >> 56) & subTreeDepthMask);
				tteSubTreeDepth = ((NormalTranspositionTableEntryDataFields_Struct*)&data)->subTreeDepth;
				//uint8_t flag = (uint8_t)((data >> 48) & flagMask);
				uint8_t flag = ((NormalTranspositionTableEntryDataFields_Struct*)&data)->flag;
				uint8_t tteEUL = (flag & TTFlagEULMask);
				currentGameRecordPointer->isTWM = flag & TTFlagThreatenedWithMate;
				currentGameRecordPointer->isO1M = flag & TTFlagOnlyOneLegalMove;
				currentGameRecordPointer->isFMTP = flag & TTFlagFewerMovesThanPieces;
				currentGameRecordPointer->isO1PCM = flag & TTFlagOnlyOnePieceCanMove;
				//standPatScore = (short)((data >> 32) & staticEvaluationMask);
				standPatScore = ((NormalTranspositionTableEntryDataFields_Struct*)&data)->staticEvaluation;
				//assert((standPatScore == INT16_MIN) || (standPatScore == Evaluate(sideToMove)));
				short tteScore;
				//tteScore = (short)((data >> 16) & scoreMask);
				tteScore = ((NormalTranspositionTableEntryDataFields_Struct*)&data)->score;

				if (abs(tteScore) >= EGTBWinningScore)
				{
					if (tteScore >= EGTBWinningScore) // A 'winning' score is a lower bound
					{
						//if (tteScore >= MatingScore)
							tteScore -= ply;
					}
					else // A 'losing' score is an upper bound
					{
						//if (tteScore < MatedScore)
							tteScore += ply;
					}
				}

				if (tteSubTreeDepth >= depthRemaining)
				{
					//if (((data >> 48) & ageMask) != TranspositionTableAge) // Touch the age for aged entries
					//{
					//	data = data & ~(3ULL << 48);
					//	data = data | ((uint64_t)TranspositionTableAge << 48);
					//	tte0[entry].data = data;
					//	tte0[entry].hash64 = hash64 ^ data;
					//}

					//if (!isPVNode) // Don't use TT values at a PV node to avoid search inconsistencies THIS GAINS ELO IN MAIN BUT LOSES IT IN QS! WTF?!?!?! (-5.3, +/-3.4, 20000 for having this in)
//if (currentGameRecordPointer->pliesSinceIrreversible < 90)//TEST
					{
						if (tteEUL == TTFlagLower) // Lower limit? (Came from a Cut node: exact value is "at least" (>=) this value)
						{
							if (tteScore >= beta)
							{
								*currentGameRecordPointer->principalVariationPointer = PVTTTLower; // TEMP
								//normalProbesSuccessful++;
								return tteScore; // We can exit because we know that at least one move will exceed current beta
							}
							tteLowerLimitScore = tteScore;
						}
						else if (tteEUL == TTFlagUpper) // Upper limit? (Came from an All node: exact value is "at most" (<=) this value)
						{
							if (tteScore <= alpha)
							{
								*currentGameRecordPointer->principalVariationPointer = PVTTTUpper; // TEMP
								//normalProbesSuccessful++;
								return tteScore; // We can exit because we know that no move will exceed current alpha
							}
						}
						else // Exact value. (Came from a PV node)
						{
							// If we use 'exact' entries at PV nodes it can truncate the PV returned for the best move
							*currentGameRecordPointer->principalVariationPointer = PVTTTExact;
							if (tteBestMove.ui32 != 0) // Sometimes there won't be a move stored as it might be a checkmate/stalemate/DBR position
							{
								*currentGameRecordPointer->principalVariationPointer = tteBestMove.ui32; // Return TT best move as part of pv
								*(currentGameRecordPointer->principalVariationPointer + 1) = PVTTTExact;
							}
							//normalProbesSuccessful++;
							return tteScore; // We can exit because we have an exact value
						}
					}
				}

				break;
			}
		}
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------

#pragma region EGTB
	// Is this position in the endgame tablebases?
	short egtbScore = -MatingIn0Score; // This may be tested at the end of the node
	//TODO: do we really need EGTB see thru in QS???
	if (EndgameTablebasesPiecesFound == 0)
	{
		// Just in case we don't have any EGTBs
		if (normalBrain.KnownLowMaterialDraws(sideToMove) == PVTDrawMinimumMaterial)
		{
			*currentGameRecordPointer->principalVariationPointer = PVTDrawMinimumMaterial;
			return drawScore;
		}
	}
	else
	{
		int totalPieces = PopulationCountX(normalBrain.piecesBB[0][AllPieces] | normalBrain.piecesBB[1][AllPieces]); // Get how many pieces remain on the board
		if (
			(totalPieces <= EndgameTablebasesTreeProbeLimitQS)
			//&& (currentGameRecordPointer->castlingStatus.ui32 == 0x01010101) // Only probe the endgame tablebases when no castling possible (8/8/8/8/8/8/1Nr3P1/R3K1k1 b Q - 0 1 Rxb2? O-O-O #13) : This condition would realistically never fail in the QS in a real game so it is commented out for efficiency!
			&& ((alpha < EGTBWinningScore + 1000 - ply) && (beta > EGTBLosingScore - 1000 + ply)) // Is the current window such that no EGTB score can possibly be in it? If so, skip the EGTB probe. This allows us to 'see-thru' the EGTBs to find any mates in this subtree.
			)
		{
			//EndgameTablebasesProbes++;

			uint32_t result;
			result = tb_probe_wdl(
				normalBrain.piecesBB[0][AllPieces],
				normalBrain.piecesBB[1][AllPieces],
				normalBrain.piecesBB[0][King] | normalBrain.piecesBB[1][King],
				normalBrain.piecesBB[0][Queen] | normalBrain.piecesBB[1][Queen],
				normalBrain.piecesBB[0][Rook] | normalBrain.piecesBB[1][Rook],
				normalBrain.piecesBB[0][Bishop] | normalBrain.piecesBB[1][Bishop],
				normalBrain.piecesBB[0][Knight] | normalBrain.piecesBB[1][Knight],
				normalBrain.piecesBB[0][Pawn] | normalBrain.piecesBB[1][Pawn],
				currentGameRecordPointer->epSquare,
				(sideToMove == 0)
			);

			// Probing the EGTB (even on a SSDD) can be very slow (>>100ms)
			// We want the QS to be as fast as possible so we don't check TimeUp/StopImmediately at the start of the function
			// Therefore we must do it after an EGTB probe to minimise any overstep of the time control (especially at hyper-bullet speeds) as we might probe the EGTB many times in a single QS
			// Even with this, we sometimes overstep at my standard 100ms/move testing. It's minimal with the 4pc but naturally increases as we move to the 5pc and 6pc.
			TimeUp(0.2f);
			if (StopImmediately)
				return -MatingIn0Score;

			if (result != TB_RESULT_FAILED)
			{
				//short egtbScore;

				EndgameTablebasesHits++;

				// Do NOT try to offset decisive scores with 'ply' (as you would with mate scores) because it just seems to reduce cutoffs and make problem solutions take longer
				// However, offsetting them with the number of pieces left encourages trading down
				switch (result)
				{
				case TB_LOSS:
					egtbScore = EGTBLosingScore - 1000 + ply;
					// If we've already got a 'losing' score at a PV node then don't use the EGTB score immediately. Allow this current subtree to be searched (to possibly find a mate) and only then use the EGTB score.
					if (isPVNode)
						if (beta <= EGTBLosingScore)
							goto EGTBExit;
					break;
				case TB_BLESSED_LOSS:
					egtbScore = -5;
					break;
				case TB_DRAW:
					egtbScore = 0;
					// Bias EGTB draw scores towards the side with the most material
					if (currentGameRecordPointer->totalMaterial[sideToMove] > currentGameRecordPointer->totalMaterial[sideToMove ^ 1])
						egtbScore++;
					else if (currentGameRecordPointer->totalMaterial[sideToMove] < currentGameRecordPointer->totalMaterial[sideToMove ^ 1])
						egtbScore--;
					break;
				case TB_CURSED_WIN:
					egtbScore = 5;
					break;
				case TB_WIN:
					egtbScore = EGTBWinningScore + 1000 - ply;
					// If we've already got a 'winning' score at a PV node then don't use the EGTB score immediately. Allow this current subtree to be searched (to possibly find a mate) and only then use the EGTB score.
					if (isPVNode)
						if (alpha >= EGTBWinningScore)
							goto EGTBExit;
					break;
				}

				// Tried storing EGTB results in the TT but no significant ELO difference
				//AddToNormalTranspositionTable(0, ply, egtbScore, TTFlagExact, PVREGTB); // The 3&4 piece SYZYGY EGTBs are only just over 1.2MB (for the WDL .rtbw files) so should soon saturate the disk cache so no point in putting them in the TT (+1.6, +/-4.6, 12404 for taking this line out)
				*currentGameRecordPointer->principalVariationPointer = PVTEGTB;
				return egtbScore;
			}
			else
			{
				EndgameTablebasesErrors = true;
				EndgameTablebasesErrorCounts[totalPieces]++;
			}
		}
	EGTBExit:;
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------

#pragma region Stand pat
	// Stand pat?
	if (!isInCheck)
	{
		// Get the 'stand pat' evaluation (About 77% of all QS nodes get evaluated here)
		if (standPatScore == INT16_MIN) // The value may already have been retrieved from the TT
		{
			if ((currentGameRecordPointer - 1)->move.ui32 == NullMove) // If the previous move was a null move we can use its score (negated and corrected for tempo) to save some time (about 12% of nodes)
			{
				standPatScore = -(currentGameRecordPointer - 1)->staticEvaluation + Tempo * 2;
				//if (standPatScore != Evaluate(sideToMove))//TEMP
				//	standPatScore = Evaluate(sideToMove);
				//assert(standPatScore == Evaluate(sideToMove));
			}
			else
				standPatScore = Evaluate(sideToMove);
		}
		bestMoveScore = standPatScore;

		// tteLowerLimitScore will only have been set if tteSubTreeDepth >= depthRemaining. It is a lower limit from a cut node
		// This helps greatly finding mates. It only happens at PV nodes. It has the side effect of terminating some mating lines with PVTStandPat
		if (tteLowerLimitScore > bestMoveScore)
			bestMoveScore = tteLowerLimitScore;

		// Will this terminal node score ever be part of a principal variation?
		if (bestMoveScore > alpha)
		{
			// If the stand-pat score causes a cutoff then don't need to search captures
			if (bestMoveScore >= beta)
			{ // About 41% (of evaluated QS nodes) get cutoff here
				// Update transposition table
				//if (!tteFound)
				//	AddToNormalTranspositionTable(0, ply, bestMoveScore, TTFlagLower, tteBestMove.ui32, standPatScore); // Keep any existing tteBestMove! {Taking this out gains 5.5 ELO}
				return bestMoveScore;
			}

			assert(isPVNode);
			alpha = bestMoveScore;
			*currentGameRecordPointer->principalVariationPointer = PVTStandPat; // Mark end of PV
		}
		//else
		//{
		//	short largestOpponentsPieceValue;
		//	if (normalBrain.piecesBB[sideToMove ^ 1][Queen])
		//		largestOpponentsPieceValue = MVQueen;
		//	else if (normalBrain.piecesBB[sideToMove ^ 1][Rook])
		//		largestOpponentsPieceValue = MVRook;
		//	else if (normalBrain.piecesBB[sideToMove ^ 1][Bishop])
		//		largestOpponentsPieceValue = MVBishop;
		//	else if (normalBrain.piecesBB[sideToMove ^ 1][Knight])
		//		largestOpponentsPieceValue = MVKnight;
		//	else
		//		largestOpponentsPieceValue = MVPawn;
		//	if (standPatScore + MVPawn + largestOpponentsPieceValue <= alpha)
		//		return standPatScore;
		//}

		//futilityBaseScore = standPatScore + MVPawn;
		futilityBaseScore = bestMoveScore + MVPawn;
		// About 59% (of evaluated QS nodes) get through here
	}
#pragma endregion

	//----------------------------------------------------------------------------------------------------

	// Generate move list
	// (Staged move generation in QS doesn't seem worth it as only ~0.1% of positions here get cut off by the TT move [as most will get cut off immediately when the TT entry is retrieved above])
	int movesCount;
	normalBrain.CalculatePinnedPieces(sideToMove); // Required for legal move generation
	movesCount = normalBrain.GenerateMovesQuiescence(sideToMove, isInCheck, moveList, depthRemaining);

	if (movesCount > 0)
	{
		// ~19% of nodes have zero captures
		// ~26% of nodes have one capture
		// ~81% of nodes have some captures
		assert(NoDuplicateMoves(moveList, movesCount));

		// Score moves for ordering
		if (movesCount > 1)
		{
			if (!isInCheck)
				normalBrain.ScoreMovesMVVLVA(moveList, movesCount);
			else
			{
				int8_t pt1, ts1;
				pt1 = abs((currentGameRecordPointer - 1)->move.fromSquarePiece) - 1; // 0..5
				ts1 = (currentGameRecordPointer - 1)->move.mf.toSquare;
				assert((pt1 >= Pawn - 1) && (pt1 <= King - 1) && (ts1 >= A1) && (ts1 <= H8));
				//currentGameRecordPointer->historyPointer = &CounterMoveHistory[pt1][ts1];
				currentGameRecordPointer->historyPointer = &CounterMoveHistory->CMH[pt1][ts1];

				int8_t fupt1, futs1;
				fupt1 = abs((currentGameRecordPointer - 2)->move.fromSquarePiece) - 1;
				futs1 = (currentGameRecordPointer - 2)->move.mf.toSquare;

				normalBrain.ScoreMoves(moveList, movesCount, tteBestMove.ui32, ply, KillerMoves, &CounterMoves[pt1][ts1], &FollowUpMoves[fupt1][futs1]);
			}
		}

		//----------------------------------------------------------------------------------------------------

		// Loop through move list
		int SEEResult;
		legalMovesMade = 0;
		int enemyKingSquare = BitScanForwardX(normalBrain.piecesBB[sideToMove ^ 1][King]);
		int winningCapturesSearched = 0; // N.B. only increments this if not in check

		for (int moveListIndexIterator = 0; moveListIndexIterator < movesCount; moveListIndexIterator++)
		{
			if ((depthRemaining <= -5) && (winningCapturesSearched)) // Deep into the QS only search the FIRST winning capture (-1.0, +/-3.6, 20000 searching ALL winning captures)
				break;

			// Get next move
			int bestSortScore = moveList[moveListIndexIterator].score;
			int bestSortIndex = moveListIndexIterator;

			for (int index = moveListIndexIterator + 1; index < movesCount; index++)
			{
				if (moveList[index].score > bestSortScore)
				{
					bestSortScore = moveList[index].score;
					bestSortIndex = index;
				}
			}

			currentMove.ui32 = moveList[bestSortIndex].ui32;
			currentGameRecordPointer->move.ui32 = currentMove.ui32;
			moveList[bestSortIndex] = moveList[moveListIndexIterator];

			if ((moveListIndexIterator == 0) && (tteBestMove.ui32 == 0))
				tteBestMove.ui32 = currentMove.ui32; // If we don't have a 'best move' from the transposition table, use the highest ordered move. This will get saved later if this is an 'All' type node. This can help when trying to detect tranposition table type-2 errors.

			//----------------------------------------------------------------------------------------------------

			// SEE pruning
			if (!isInCheck) // Pruning when in check can cause false mates to be returned!
			{
				// Calculate the SEE result
				SEEResult = normalBrain.SEE(currentMove.mf.fromSquare, currentMove.mf.toSquare, sideToMove); // For ExE and HxL we have to calculate it

				if (SEEResult == 1)
					winningCapturesSearched++; // N.B. only increments this if not in check
				else if (bestMoveScore > EGTBLosingScore) // Don't prune if we're losing!
				{
					// N.B. checks may be SEE pruned
					if (depthRemaining <= -5) // Deep into the QS discard losing and equal captures/moves
						continue;
					else
					{
						// SEE pruning of apparently bad captures/moves (reduces the node count quite a bit and gives a large ELO gain)
						// N.B. LxH and ExE are allowed so only 'losing' HxL are pruned. The remaining moves may not be 'relatively' bad but they might still be 'absolutely' futile in that they can't achieve alpha
						if (SEEResult < 0)
							continue;
					}
				}
			}

			//----------------------------------------------------------------------------------------------------
			//PRINTTREE(PrintTree(IterationPly, ply, alpha, beta, depthRemaining, currentMove.ui32, bestSortScore, currentGameRecordPointer->staticEvaluation);)

			// Up-date move
			normalBrain.MakeMove(sideToMove); // N.B. MakeMove increments normalBrain.gameRecordPointer!
			legalMovesMade++;

			// Initiate the retrieval of the next transposition table cache line as soon as possible
			_mm_prefetch((char*)(NormalTranspositionTablePointer + (normalBrain.gameRecordPointer->transpositionTableHash64 & NormalTranspositionTableBucketsMask)), _MM_HINT_T0);

#ifdef SEARCHINGFORLINE
			if (TargetLineLength == ply)
				if (TargetLine == normalBrain.CurrentLine(ply))
					AC9++;
#endif

			bool givesCheck = normalBrain.IsEnemyKingAttacked(enemyKingSquare, sideToMove);
			assert(isInCheck || givesCheck || (currentGameRecordPointer->move.toSquarePiece) || (currentMove.mf.flag >= MFPromotion));

			//----------------------------------------------------------------------------------------------------

			// Pruning
			if (
				(!isPVNode)
				&& (!isInCheck) // Pruning when in check can cause false mates to be returned!
				&& (!givesCheck) // Don't prune checks
				&& (currentMove.mf.flag < MFPromotion) // Don't prune promotions (~+1.5 ELO)
				&& (bestMoveScore > EGTBLosingScore) // Don't prune if we're losing! (~2 ELO)
				)
			{
				assert(!(currentMove.mf.flag >= MFPromotion));
				// Move count pruning (addresses QS explosion when there are loads of captures)
				// N.B. There shouldn't be any 'bad' HxL here (as they will have been SEE pruned above) and LxH are allowed through so this just prunes ExE
				if (SEEResult == 0)
					if (legalMovesMade > 2)
					{
						normalBrain.UnMakeMove(sideToMove);
						continue;
					}

				// Futility pruning (futilityBaseScore <= alpha about 65% of the time)
				if (futilityBaseScore <= alpha)
				{
					// If SEE says that it doesn't win anything then prune
					if (SEEResult <= 0)
					{
						normalBrain.UnMakeMove(sideToMove);
						continue;
					}

					// If we win the whole piece and still don't exceed alpha then prune (about 24% get pruned)
					short futilityScore = futilityBaseScore + MaterialValue[abs(currentGameRecordPointer->move.toSquarePiece)];
					if (futilityScore <= alpha)
					{
						normalBrain.UnMakeMove(sideToMove);
						continue;
					}
				}
			}

			currentMoveScore = (short)-TreeSearchNormalQuiescence((short)-beta, (short)-alpha, ply + 1, depthRemaining - 1, sideToMove ^ 1, givesCheck);

			//----------------------------------------------------------------------------------------------------

			// Down-date move
			normalBrain.UnMakeMove(sideToMove); // N.B. UnMakeMove decrements normalBrain.gameRecordPointer!

			//----------------------------------------------------------------------------------------------------

			// Stopping? (N.B. Must do this BEFORE the 'new best move' test below otherwise a partially searched move could take over as best or be added to the TT!)
			if (StopImmediately)
				return -MatingIn0Score;

			//----------------------------------------------------------------------------------------------------

			// New best move?
			if (currentMoveScore > bestMoveScore)
			{
				if (currentMoveScore >= beta)
				{
					// This move has returned a score >= beta, therefore this is a 'Cut' node
					// The currentMoveScore is a lower bound (floor) on the exact score of the node (i.e. the exact score might be greater than currentMoveScore, it is "at least" currentMoveScore)
					AddToNormalTranspositionTable(depthRemaining, ply, currentMoveScore, TTFlagLower + currentGameRecordPointer->isTWM + currentGameRecordPointer->isO1M + currentGameRecordPointer->isFMTP + currentGameRecordPointer->isO1PCM, currentMove.ui32, standPatScore);// , tteFound);
					return currentMoveScore;
				}

				if (currentMoveScore > alpha)
				{
					assert(isPVNode);
					alpha = currentMoveScore;
					normalBrain.SavePrincipalVariation(currentMove.ui32); // Only have to save the PV when alpha < currentMoveScore < beta i.e. at PV nodes
				}

				bestMoveScore = currentMoveScore;
			}

		} // (Loop through move list)
		// About 63% of nodes that start searching through the move list (about 14% of all QS nodes) don't get cut off
	}
	else
	{
		// Zero moves generated
		if (isInCheck)
		{ // Checkmate
			assert(tteBestMove.ui32 == 0);
			*currentGameRecordPointer->principalVariationPointer = PVTCheckmate;
			return (short)(-MatingIn0Score + ply);
		}
		if ( // Lone K stalemate? (Doesn't add any ELO but helps with some puzzle solving)
			(normalBrain.piecesBB[sideToMove][AllPieces] == normalBrain.piecesBB[sideToMove][King])
			&& (!normalBrain.KingCanLegallyMove(sideToMove))
			)
		{
			*currentGameRecordPointer->principalVariationPointer = PVTDrawStalemate;
			return drawScore;
		}
	}

	//----------------------------------------------------------------------------------------------------

	// Do we have an EGTB score from earlier?
	if (egtbScore != -MatingIn0Score)
	{
		*currentGameRecordPointer->principalVariationPointer = PVTEGTB;
		return egtbScore;
	}

	//if (movesCount > 0)
	{
		// Update transposition table
		if (alpha == originalAlpha)
		{
			// No move has returned a score > alpha, therefore this is an 'All' node (all legal moves have been searched)
			// The bestMoveScore is an upper bound (ceiling) on the exact score of the node (i.e. the exact score might be less than bestMoveScore, it is "at most" bestMoveScore)
			// The children of an All node are Cut nodes. The parent of an All node is a Cut node. The ply distance of an All node to its PV ancestor is even.
			assert((bestMoveScore > -MatingIn0Score) && (bestMoveScore <= alpha));
			assert(*currentGameRecordPointer->principalVariationPointer == PVTUnknown);
			AddToNormalTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagUpper + currentGameRecordPointer->isTWM + currentGameRecordPointer->isO1M + currentGameRecordPointer->isFMTP + currentGameRecordPointer->isO1PCM, tteBestMove.ui32, standPatScore);// , tteFound); // Keep any existing tteBestMove even though it didn't raise alpha
		}
		else
		{
			// A move has returned a score > (the original) alpha but < beta, therefore this is a 'PV' node (all legal moves have been searched)
			// The bestMoveScore is the EXACT score of the node
			// The root node and the leftmost nodes are always PV-nodes. All siblings of a PV node are expected Cut nodes.
			assert((originalAlpha < bestMoveScore) && (bestMoveScore == alpha) && (bestMoveScore < beta));
			assert(isPVNode);
			assert(*currentGameRecordPointer->principalVariationPointer != PVTUnknown);
			AddToNormalTranspositionTable(depthRemaining, ply, bestMoveScore, TTFlagExact + currentGameRecordPointer->isTWM + currentGameRecordPointer->isO1M + currentGameRecordPointer->isFMTP + currentGameRecordPointer->isO1PCM, *currentGameRecordPointer->principalVariationPointer == PVTStandPat ? tteBestMove.ui32 : *currentGameRecordPointer->principalVariationPointer, standPatScore);// , tteFound);
		}
	}

	//----------------------------------------------------------------------------------------------------

	assert((bestMoveScore > -MatingIn0Score) && (bestMoveScore < MatingIn0Score));
	return bestMoveScore;
}
