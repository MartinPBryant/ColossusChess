// This code is simply #include at the top of SearchNormal.cpp
// It is broken out into a separate file for readability

int16_t Normal::Evaluate(int sideToMove)
{
	int16_t result = Nnue.RunNetworkChess768(normalBrain, sideToMove);
	return result;
}

//----------------------------------------------------------------------------------------------------

void Normal::StaticEvaluation()
{
	short score;

	normalBrain.CopyFrom(&EngineBrain);

	ConvertMailboxBoard64ToPiecesBB(normalBrain.MailboxBoard64, normalBrain.PiecesBB);
	InitialiseMaterialValues(normalBrain.MailboxBoard64, normalBrain.GameRecordPointer);
	InitialiseGamePhase(normalBrain.MailboxBoard64, normalBrain.GameRecordPointer);
	Nnue.InitialiseAccumulatorsChess768(normalBrain);
	Output("info string normalBrain.gameRecordPointer->totalMaterial[0] = " + MyITOA(normalBrain.GameRecordPointer->totalMaterial[0]));
	Output("info string normalBrain.gameRecordPointer->totalMaterial[1] = " + MyITOA(normalBrain.GameRecordPointer->totalMaterial[1]));
	Output("info string normalBrain.gameRecordPointer->gamePhase[0] = " + MyITOA(normalBrain.GameRecordPointer->gamePhase[0]));
	Output("info string normalBrain.gameRecordPointer->gamePhase[1] = " + MyITOA(normalBrain.GameRecordPointer->gamePhase[1]));

	score = Evaluate(0);
	Output("info string Static Evaluation WTM = " + MyITOA(score));
	score = Evaluate(1);
	Output("info string Static Evaluation BTM = " + MyITOA(score));
}
