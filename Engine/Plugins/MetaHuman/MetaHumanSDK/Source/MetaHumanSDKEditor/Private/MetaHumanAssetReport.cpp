// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAssetReport.h"

#include "MetaHumanSDKEditor.h"

#include "JsonObjectConverter.h"
#include "Logging/StructuredLog.h"

#define LOCTEXT_NAMESPACE "MetaHumanAssetReport"

void UMetaHumanAssetReport::SetSubject(const FString& InSubject)
{
	Subject = InSubject;
}

void UMetaHumanAssetReport::AddVerbose(const FMetaHumanAssetReportItem& Message)
{
	if (bVerbose)
	{
		Infos.Add(Message);
	}
}

void UMetaHumanAssetReport::AddInfo(const FMetaHumanAssetReportItem& Message)
{
	Infos.Add(Message);
	UE_LOGFMT(LogMetaHumanSDK, Display, "{Message}", Message.Message.ToString());
}

void UMetaHumanAssetReport::AddWarning(const FMetaHumanAssetReportItem& Message)
{
	if (bWarningsAsErrors)
	{
		return AddError(Message);
	}
	else
	{
		Warnings.Add(Message);
		UE_LOGFMT(LogMetaHumanSDK, Warning, "{Message}", Message.Message.ToString());
	}
}

void UMetaHumanAssetReport::AddError(const FMetaHumanAssetReportItem& Message)
{
	Errors.Add(Message);
	UE_LOGFMT(LogMetaHumanSDK, Error, "{Message}", Message.Message.ToString());
}

FString FormatAsHTML(const FMetaHumanAssetReportItem& Item)
{
	TStringBuilder<1024> ItemString;
	bool bHasFileLink = !Item.SourceItem.IsEmpty();
	if (bHasFileLink)
	{
		ItemString << TEXT("<a href=\"") << Item.SourceItem << TEXT("\">");
	}

	ItemString << Item.Message.ToString();

	if (bHasFileLink)
	{
		ItemString << TEXT("</a>");
	}

	return ItemString.ToString();
}

FText FormatAsRichText(const FMetaHumanAssetReportItem& Item)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Message"), Item.Message);

	// See Source/Editor/EditorStyle/Private/SlateEditorStyle.cpp for more styles and how to roll your own.
	FText FormatString = LOCTEXT("StandardMessage", "• <RichTextBlock.Italic>{Message}</>");
	FText LinkFormatString = LOCTEXT("LinkMessage", "• <a id=\"{LinkType}\" href=\"{Href}\">{Message}</>");
	if (Item.ProjectItem)
	{
		Args.Add(TEXT("LinkType"), FText::FromString(TEXT("asset")));
		Args.Add(TEXT("Href"), FText::FromString(Item.ProjectItem->GetPathName()));
		FormatString = LinkFormatString;
	}
	else if (!Item.SourceItem.IsEmpty())
	{
		if (Item.SourceItem.StartsWith("http"))
		{
			Args.Add(TEXT("LinkType"), FText::FromString(TEXT("browser")));
			Args.Add(TEXT("Href"), FText::FromString(Item.SourceItem));
		}
		else
		{
			Args.Add(TEXT("LinkType"), FText::FromString(TEXT("file")));
			Args.Add(TEXT("Href"), FText::FromString(Item.SourceItem));
		}
		FormatString = LinkFormatString;
	}
	return FText::Format(FormatString, Args);
}

FString UMetaHumanAssetReport::GenerateHtmlReport() const
{
	TStringBuilder<1024> HtmlReport;
	HtmlReport << TEXT("<html>\n")
		<< TEXT("<head>\n")
		<< TEXT("<title>Report for ") << Subject << TEXT("</title>\n")
		<< TEXT("<style>\n")
		<< TEXT("li.info::marker { color: #0070E0FF; }\n")
		<< TEXT("li.warning::marker { color: #FFB800FF; }\n")
		<< TEXT("li.error::marker { color: #EF3535FF; }\n")
		<< TEXT("body { background: #2f2f2f; color: white; padding: 20px; font-family: sans-serif }\n")
		<< TEXT("h1 { background: #808080; padding: 20px; border-radius: 8px 8px 0 0; margin-bottom: 0}\n")
		<< TEXT("div { background: #1A1A1A; padding: 20px; border-radius: 0 0 8px 8px; margin-top: 0 }\n")
		<< TEXT("</style>\n")
		<< TEXT("</head>\n")
		<< TEXT("<body>\n")
		<< TEXT("<h1>Report for ") << Subject << TEXT("</h1>\n")
		<< TEXT("<div>\n");
	if (Errors.Num())
	{
		HtmlReport << TEXT("<h2>Errors</h2>\n")
			<< TEXT("<ul>\n");
		for (const FMetaHumanAssetReportItem& Error : Errors)
		{
			HtmlReport << TEXT("<li class=\"error\">") << FormatAsHTML(Error) << TEXT("</li>\n");
		}
		HtmlReport << TEXT("</ul>\n");
	}
	if (Warnings.Num())
	{
		HtmlReport << TEXT("<h2>Warnings</h2>\n")
			<< TEXT("<ul>\n");
		for (const FMetaHumanAssetReportItem& Warning : Warnings)
		{
			HtmlReport << TEXT("<li class=\"warning\">") << FormatAsHTML(Warning) << TEXT("</li>\n");
		}
		HtmlReport << TEXT("</ul>\n");
	}
	if (Infos.Num())
	{
		HtmlReport << TEXT("<h2>Infos</h2>\n")
			<< TEXT("<ul>\n");
		for (const FMetaHumanAssetReportItem& Info : Infos)
		{
			HtmlReport << TEXT("<li class=\"info\">") << FormatAsHTML(Info) << TEXT("</li>\n");
		}
		HtmlReport << TEXT("</ul>\n");
	}
	if (Errors.Num() + Warnings.Num() + Infos.Num() == 0)
	{
		HtmlReport << TEXT("<h2>Operation succeeded with no messages</h2>\n");
	}
	HtmlReport << TEXT("</div>\n")
		<< TEXT("</body>\n")
		<< TEXT("</html>\n");
	return HtmlReport.ToString();
}
FText UMetaHumanAssetReport::GenerateRichTextReport() const
{
	FTextBuilder RichTextReport;
	RichTextReport.AppendLineFormat(LOCTEXT("ReportSubject", "<LargeText>Report for {0}</>"), FText::FromString(Subject));
	RichTextReport.AppendLine();
	RichTextReport.AppendLine(LOCTEXT("ErrorsHeading", "<LargeText>Errors:</>"));
	for (const FMetaHumanAssetReportItem& Error : Errors)
	{
		RichTextReport.AppendLine(FormatAsRichText(Error));
	}
	RichTextReport.AppendLine();
	RichTextReport.AppendLine(LOCTEXT("WarningsHeading", "<LargeText>Warnings:</>"));
	for (const FMetaHumanAssetReportItem& Warning : Warnings)
	{
		RichTextReport.AppendLine(FormatAsRichText(Warning));
	}
	RichTextReport.AppendLine();
	RichTextReport.AppendLine(LOCTEXT("InfosHeading", "<LargeText>Infos:</>"));
	for (const FMetaHumanAssetReportItem& Info : Infos)
	{
		RichTextReport.AppendLine(FormatAsRichText(Info));
	}
	return RichTextReport.ToText();
}

FString UMetaHumanAssetReport::GenerateJsonReport() const
{
	FString JsonReport;
	FJsonObjectConverter::UStructToJsonObjectString(StaticClass(), this, JsonReport);
	return JsonReport;
}

FString UMetaHumanAssetReport::GenerateRawReport() const
{
	TStringBuilder<1024> RawReport;
	RawReport << TEXT("Subject: ") << Subject << TEXT("\n");
	for (const FMetaHumanAssetReportItem& Error : Errors)
	{
		RawReport << TEXT("Error: ") << Error.Message.ToString() << TEXT("\n");
		if (!Error.SourceItem.IsEmpty())
		{
			RawReport << TEXT("Refers to file: ") << Error.SourceItem << TEXT("\n");
		}
	}
	for (const FMetaHumanAssetReportItem& Warning : Warnings)
	{
		RawReport << TEXT("Warning: ") << Warning.Message.ToString() << TEXT("\n");
		if (!Warning.SourceItem.IsEmpty())
		{
			RawReport << TEXT("Refers to file: ") << Warning.SourceItem << TEXT("\n");
		}
	}
	for (const FMetaHumanAssetReportItem& Info : Infos)
	{
		RawReport << TEXT("Info: ") << Info.Message.ToString() << TEXT("\n");
		if (!Info.SourceItem.IsEmpty())
		{
			RawReport << TEXT("Refers to file: ") << Info.SourceItem << TEXT("\n");
		}
	}
	return RawReport.ToString();
}

EMetaHumanOperationResult UMetaHumanAssetReport::GetReportResult() const
{
	return Errors.Num() ? EMetaHumanOperationResult::Failure : EMetaHumanOperationResult::Success;
}

bool UMetaHumanAssetReport::HasWarnings() const
{
	return (Errors.Num() + Warnings.Num()) != 0;
}
void UMetaHumanAssetReport::SetWarningsAsErrors(const bool Value)
{
	bWarningsAsErrors = Value;
}
void UMetaHumanAssetReport::SetVerbose(const bool Value)
{
	bVerbose = Value;
}

#undef LOCTEXT_NAMESPACE
