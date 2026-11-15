// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/Dialogue/StageDialogueTypes.h"
#include "Internationalization/BreakIterator.h"

void StageDialogue::Tokenize(const FString& Text, TArray<int32>& LetterEnds, TArray<int32>& WordStarts)
{
	LetterEnds.Reset();
	WordStarts.Reset();
	const auto Characters = FBreakIterator::CreateCharacterBoundaryIterator();
	Characters->SetStringRef(Text);
	int32 Start = Characters->ResetToBeginning();
	for (int32 End = Characters->MoveToNext(); End != INDEX_NONE; End = Characters->MoveToNext())
	{
		if (!FChar::IsWhitespace(Text[Start]))
		{
			LetterEnds.Add(End);
			if (Start == 0 || FChar::IsWhitespace(Text[Start - 1])) WordStarts.Add(Start);
		}
		Start = End;
	}
}

float StageDialogue::GetDuration(const FDialogueLine& Line)
{
	TArray<int32> Ends, Starts;
	const FString Text = Line.Text.ToString();
	Tokenize(Text, Ends, Starts);
	if (Ends.IsEmpty()) return 0;
	double Duration = FMath::Max(0.0f, Line.HoldDuration);
	for (int32 Index = 0; Index + 1 < Ends.Num(); ++Index)
	{
		Duration += FMath::Max(0.0f, Line.LetterInterval);
		if (Ends[Index] < Text.Len() && FChar::IsWhitespace(Text[Ends[Index]])) Duration += FMath::Max(0.0f, Line.WordInterval);
	}
	return static_cast<float>(Duration);
}
