// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMVerseException.h"

namespace Verse
{
constexpr SRuntimeDiagnosticInfo DiagnosticInfos[] = {
#define VISIT_DIAGNOSTIC(_Severity, _EnumName, _Description) {.Name = #_EnumName, .Description = _Description, .Severity = ERuntimeDiagnosticSeverity::_Severity},
	VERSE_RUNTIME_GLITCH_ENUM_DIAGNOSTICS(VISIT_DIAGNOSTIC)
#undef VISIT_DIAGNOSTIC
};

const SRuntimeDiagnosticInfo& GetRuntimeDiagnosticInfo(const ERuntimeDiagnostic Diagnostic)
{
	if (ensureMsgf(size_t(Diagnostic) < UE_ARRAY_COUNT(DiagnosticInfos), TEXT("Invalid runtime diagnostic enum: %zu"), Diagnostic))
	{
		return DiagnosticInfos[size_t(Diagnostic)];
	}
	// Just return that an unknown internal error occured if the code can't be found
	return DiagnosticInfos[size_t(ERuntimeDiagnostic::ErrRuntime_Internal)];
}

FString AsFormattedString(const ERuntimeDiagnostic& Diagnostic, const FText& MessageText)
{
	const SRuntimeDiagnosticInfo& Info = GetRuntimeDiagnosticInfo(Diagnostic);
	FStringBuilderBase Result;
	Result += TEXT("Verse ");
	switch (Info.Severity)
	{
		case ERuntimeDiagnosticSeverity::UnrecoverableError:
			Result.Append(TEXT("unrecoverable error"));
			break;
		default:
			// Unreachable code
			ensureMsgf(false, TEXT("Unsupported enum: %d!"), Info.Severity);
			break;
	}
	Result.Appendf(TEXT(": %s: %s"), ANSI_TO_TCHAR(Info.Name), ANSI_TO_TCHAR(Info.Description));
	if (!MessageText.IsEmpty())
	{
		Result.Appendf(TEXT(" (%s)"), *MessageText.ToString());
	}
	return Result.ToString();
}
} // namespace Verse

FVerseRuntimeErrorReportHandler FVerseExceptionReporter::OnVerseRuntimeError;