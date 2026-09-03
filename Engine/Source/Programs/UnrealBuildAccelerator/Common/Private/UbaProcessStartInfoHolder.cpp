// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaProcessStartInfoHolder.h"
#include "UbaConfig.h"

namespace uba
{
	bool ProcessStartInfoHolder::Expand()
	{
	#if PLATFORM_WINDOWS
		// Special handling to avoid calling cmd.exe if not needed
		if (!Contains(application, TC("cmd.exe")))
			return false;

		const tchar* argsBegin = argumentsStr.c_str();

		// Check if application is repeated as first argument, in that case consume it
		const tchar* firstArgBegin = argsBegin;
		const tchar* firstArgEnd = nullptr;
		if (*firstArgBegin == '\"')
		{
			++firstArgBegin;
			firstArgEnd = TStrchr(firstArgBegin, '\"');
		}
		else
			firstArgEnd = TStrchr(firstArgBegin, ' ');
		if (!firstArgEnd)
			return false;
		return InternalExpand(firstArgBegin, firstArgEnd);
	#else
		return false;
	#endif
	}

	UBA_NOINLINE bool ProcessStartInfoHolder::InternalExpand(const tchar* firstArgBegin, const tchar* firstArgEnd)
	{
		// Separate function to hide away large stack object
	#if PLATFORM_WINDOWS
		const tchar* argsBegin = argumentsStr.c_str();
		const tchar* argsEnd = argsBegin + argumentsStr.size();

		StringBuffer<32*1024> commands;
		commands.Append(firstArgBegin, firstArgEnd - firstArgBegin);
		if (commands.Contains(application))
			argsBegin = firstArgEnd;
		while (argsBegin && *argsBegin == ' ')
			++argsBegin;
		commands.Clear();


		// Parse switches... only supported is /C right now
		if (!StartsWith(argsBegin, TC("/C ")))
			return false;
		argsBegin += 3;
		while (argsBegin && *argsBegin == ' ')
			++argsBegin;

		if (argsBegin && *argsBegin == '/') // Unknown switch, don't try to expand cmd
			return false;

		if (argsBegin && *argsBegin == '\"')
		{
			++argsBegin;
			--argsEnd;
		}
		while (argsBegin && *argsBegin == ' ')
			++argsBegin;

		commands.Append(argsBegin, argsEnd - argsBegin);

		// It could be that there is just a chain of commands to set working dir, in that case we strip out cmd.exe
		const tchar* andPos = nullptr;
		if (commands.Contains(TC(" && "), true, &andPos))
		{
			if (Contains(andPos + 4, TC(" && "))) // If more than one && we don't try to expand cmd.exe
				return false;
			if (!commands.StartsWith(TC("cd /D"))) // First command is not cd, don't try to expand cmd.exe
				return false;
			const tchar* workDirStart = commands.data + 6;
			workingDirStr.assign(workDirStart, andPos - workDirStart);

			StringBuffer<> fixed;
			FixPath(workingDirStr.data(), nullptr, 0, fixed);
			workingDirStr = fixed.data;
			workingDir = workingDirStr.c_str();

			const tchar* commandLine = andPos + 4;
			while (*commandLine && *commandLine == ' ')
				++commandLine;
			const tchar* applicationBegin = commandLine;
			const tchar* applicationEnd = nullptr;
			if (*applicationBegin == '\"')
			{
				++applicationBegin;
				applicationEnd = TStrchr(applicationBegin, '\"');
			}
			else
				applicationEnd = TStrchr(applicationBegin, ' ');
			if (!applicationEnd)
				applicationEnd = applicationBegin + TStrlen(applicationBegin);
			applicationStr.assign(applicationBegin, applicationEnd - applicationBegin);
			FixPath(applicationStr.data(), nullptr, 0, fixed.Clear());
			applicationStr = fixed.data;
			application = applicationStr.c_str();
			argsBegin = *applicationEnd == '\"' ? applicationEnd + 1 : applicationEnd;
			while (*argsBegin && *argsBegin == ' ')
				++argsBegin;
			argumentsStr = argsBegin;
			arguments = argumentsStr.c_str();
			return true;
		}
		else
		{
			// TODO: This is super hacky... but we don't want to spawn the cmd.exe just to copy a file since the overhead can be half a second
			// "C:\WINDOWS\system32\cmd.exe" /C "copy /Y "E:\dev\fn\Engine\Source\Runtime\RenderCore\RenderCore.natvis" "E:\dev\fn\Engine\Intermediate\Build\Win64\x64\UnrealPak\Development\RenderCore\RenderCore.natvis" 1>nul"
			if (!commands.StartsWith(TC("copy /Y \"")))
				return false;

			const tchar* fromFileBegin = commands.data + 8;
			const tchar* fromFileEnd = TStrchr(fromFileBegin+1, '\"');
			if (!fromFileEnd)
				return false;
			const tchar* toFileBegin = TStrchr(fromFileEnd + 1, '\"');
			if (!toFileBegin)
				return false;
			++toFileBegin;
			const tchar* toFileEnd = TStrchr(toFileBegin, '\"');
			if (!toFileEnd)
				return false;

			applicationStr = TC("ubacopy");
			application = applicationStr.c_str();
			argumentsStr = fromFileBegin;
			arguments = argumentsStr.c_str();
			return true;
		}
	#endif
		return false;
	}

	void ProcessStartInfoHolder::Apply(const Config& config, const uba::tchar* configTable)
	{
		const ConfigTable* tablePtr = config.GetTable(configTable);
		if (!tablePtr)
			return;
		const ConfigTable& table = *tablePtr;
		if (table.GetValueAsString(applicationStr, TC("Application")))
			application = applicationStr.c_str();
		if (table.GetValueAsString(argumentsStr, TC("Arguments")))
			arguments = argumentsStr.c_str();
		if (table.GetValueAsString(workingDirStr, TC("WorkingDir")))
			workingDir = workingDirStr.c_str();
		if (table.GetValueAsString(descriptionStr, TC("Description")))
			description = descriptionStr.c_str();
		if (table.GetValueAsString(logFileStr, TC("LogFile")))
			logFile = logFileStr.c_str();
		if (table.GetValueAsString(breadcrumbsStr, TC("Breadcrumbs")))
			breadcrumbs = breadcrumbsStr.c_str();

		table.GetValueAsU32(priorityClass, TC("PriorityClass"));
		table.GetValueAsBool(trackInputs, TC("TrackInputs"));
		table.GetValueAsBool(useCustomAllocator, TC("UseCustomAllocator"));
		table.GetValueAsBool(writeOutputFilesOnFail, TC("WriteOutputFilesOnFail"));
		table.GetValueAsBool(startSuspended, TC("StartSuspended"));
		table.GetValueAsU64(rootsHandle, TC("RootsHandle"));
	}
}
