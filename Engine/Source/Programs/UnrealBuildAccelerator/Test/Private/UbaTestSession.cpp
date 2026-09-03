// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaConfig.h"
#include "UbaFileAccessor.h"
#include "UbaNetworkBackendTcp.h"
#include "UbaProcess.h"
#include "UbaSessionServer.h"
#include "UbaSessionClient.h"
#include "UbaStorageServer.h"
#include "UbaStorageClient.h"
#include "UbaTest.h"

namespace uba
{
	u32 GetTimeoutTime()
	{
		#if PLATFORM_WINDOWS
		if (IsDebuggerPresent())
			return 10'000'000;
		#endif
		return 10'000;
	}

	bool RunLocal(LoggerWithWriter& logger, const StringBufferBase& testRootDir, const Config& config, const TestSessionFunction& testFunc, bool enableDetour = true)
	{
		LogWriter& logWriter = logger.m_writer;

		StringBuffer<MaxPath> rootDir;
		rootDir.Append(testRootDir).Append(TCV("Uba"));
		if (!DeleteAllFiles(logger, rootDir.data))
			return false;

		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, { logWriter });

		StorageCreateInfo storageInfo(rootDir.data, logWriter, server);
		storageInfo.casCapacityBytes = 1024ull * 1024 * 1024;
		storageInfo.Apply(config);
		StorageImpl storage(storageInfo);

		SessionServerCreateInfo sessionServerInfo(storage, server, logWriter);
		sessionServerInfo.checkMemory = false;
		sessionServerInfo.rootDir = rootDir.data;
		sessionServerInfo.Apply(config);

		#if UBA_DEBUG
		sessionServerInfo.logToFile = true;
		#endif

		SessionServer session(sessionServerInfo);

		StringBuffer<MaxPath> workingDir;
		workingDir.Append(testRootDir).Append(TCV("WorkingDir"));
		if (!DeleteAllFiles(logger, workingDir.data))
			return false;

		if (!storage.CreateDirectory(workingDir.data))
			return false;
		if (!DeleteAllFiles(logger, workingDir.data, false))
			return false;
		workingDir.EnsureEndsWithSlash();
		return testFunc(logger, session, workingDir.data, [&](const ProcessStartInfo& pi) { return session.RunProcess(pi, true, enableDetour); });
	}

	bool RunLocal(LoggerWithWriter& logger, const StringBufferBase& testRootDir, const TestSessionFunction& testFunc, bool enableDetour)
	{
		return RunLocal(logger, testRootDir, {}, testFunc, enableDetour);
	}

	void GetTestAppPath(LoggerWithWriter& logger, StringBufferBase& out)
	{
		GetDirectoryOfCurrentModule(logger, out);
		out.EnsureEndsWithSlash();
		out.Append(IsWindows ? TC("UbaTestApp.exe") : TC("UbaTestApp"));
	}

	bool RunTestApp(LoggerWithWriter& logger, const tchar* workingDir, const RunProcessFunction& runProcess, const tchar* arguments, ProcessHandle* outHandle = nullptr)
	{
		StringBuffer<> testApp;
		GetTestAppPath(logger, testApp);

		ProcessStartInfo processInfo;
		processInfo.application = testApp.data;
		processInfo.workingDir = workingDir;
		processInfo.arguments = arguments;
		processInfo.logLineFunc = [](void* userData, const tchar* line, u32 length, LogEntryType type)
			{
				LoggerWithWriter(g_consoleLogWriter, TC("")).Info(line);
			};

		ProcessHandle process = runProcess(processInfo);
		if (!process.WaitForExit(GetTimeoutTime()))
			return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
		if (outHandle)
			*outHandle = process;
		u32 exitCode = process.GetExitCode();
		if (exitCode == 0)
			return true;
		for (auto& logLine : process.GetLogLines())
			logger.Error(logLine.text.c_str());
		return logger.Error(TC("UbaTestApp returned exit code %u"), exitCode);
	}

	using TestServerSessionFunction = Function<bool(LoggerWithWriter& logger, const StringView& workingDir, SessionServer& sessionServer)>;
	bool SetupServerSession(LoggerWithWriter& logger, const StringBufferBase& testRootDir, bool deleteAll, bool serverShouldListen, const Config& serverConfig, const TestServerSessionFunction& testFunc)
	{
		LogWriter& logWriter = logger.m_writer;
		NetworkBackendTcpCreateInfo tcpInfo{logWriter};
		tcpInfo.Apply(serverConfig);
		NetworkBackendTcp tcpBackend(tcpInfo);

		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, { logWriter });

		StringBuffer<MaxPath> rootDir;
		rootDir.Append(testRootDir).Append(TCV("Uba"));

		StringBuffer<MaxPath> toDelete(rootDir);
		if (!deleteAll)
			toDelete.Append(PathSeparator).Append(TCV("sessions"));
		if (!DeleteAllFiles(logger, toDelete.data))
			return false;

		StorageServerCreateInfo storageServerInfo(server, rootDir.data, logWriter);
		storageServerInfo.casCapacityBytes = 1024ull * 1024 * 1024;
		storageServerInfo.Apply(serverConfig);
		auto& storageServer = *new StorageServer(storageServerInfo);
		auto ssg = MakeGuard([&]() { delete &storageServer; });

		SessionServerCreateInfo sessionServerInfo(storageServer, server, logWriter);
		sessionServerInfo.checkMemory = false;
		sessionServerInfo.rootDir = rootDir.data;
		sessionServerInfo.useUniqueId = false;

		#if UBA_DEBUG
		sessionServerInfo.logToFile = true;
		sessionServerInfo.remoteLogEnabled = true;
		#endif
		sessionServerInfo.Apply(serverConfig);

		auto& sessionServer = *new SessionServer(sessionServerInfo);
		auto ssg2 = MakeGuard([&]() { delete &sessionServer; });

		auto sg = MakeGuard([&]() { server.DisconnectClients(); });

		sessionServer.SetRemoteProcessReturnedEvent([](Process& p) { p.Cancel(true); });

		Config clientConfig;
		clientConfig.AddTable(TC("Storage")).AddValue(TC("CheckExistsOnServer"), true);
		server.SetClientsConfig(clientConfig);

		StringBuffer<MaxPath> workingDir;
		workingDir.Append(testRootDir).Append(TCV("WorkingDir"));
		if (deleteAll && !DeleteAllFiles(logger, workingDir.data))
			return false;
		if (!storageServer.CreateDirectory(workingDir.data))
			return false;
		if (deleteAll && !DeleteAllFiles(logger, workingDir.data, false))
			return false;

		workingDir.EnsureEndsWithSlash();
		return testFunc(logger, workingDir, sessionServer);
	}

	using TestClientSessionFunction = Function<bool(LoggerWithWriter& logger, SessionClient& sessionClient)>;
	bool SetupClientSession(LoggerWithWriter& logger, const StringBufferBase& testRootDir, bool deleteAll, bool serverShouldListen, NetworkServer& server, u16 port, const TestClientSessionFunction& testFunc)
	{
		Config serverConfig;


		LogWriter& logWriter = logger.m_writer;
		NetworkBackendTcpCreateInfo tcpInfo{logWriter};
		tcpInfo.Apply(serverConfig);
		NetworkBackendTcp tcpBackend(tcpInfo);

		bool ctorSuccess = true;
		NetworkClient client(ctorSuccess, { logWriter });

		if (serverShouldListen)
		{
			if (!server.StartListen(tcpBackend, port))
				return false;
			if (!client.Connect(tcpBackend, TC("127.0.0.1"), port))
				return logger.Error(TC("Failed to connect"));
			if (!client.Connect(tcpBackend, TC("127.0.0.1"), port))
				return logger.Error(TC("Failed to connect"));
		}
		else
		{
			if (!client.StartListen(tcpBackend, port))
				return logger.Error(TC("Failed to listen"));
			if (!server.AddClient(tcpBackend, TC("127.0.0.1"), port))
				return logger.Error(TC("Failed to connect"));
			while (server.HasConnectInProgress())
				Sleep(1);
		}
		auto disconnectGuard = MakeGuard([&]() { tcpBackend.StopListen(); client.Disconnect(); server.RemoveDisconnectedConnections(); });

		Config config;
		if (!client.FetchConfig(config))
			return false;

		StringBuffer<MaxPath> rootDir;
		rootDir.Append(testRootDir).Append(TCV("UbaClient")).AppendValue(port);

		StringBuffer<MaxPath> toDelete(rootDir);
		if (!deleteAll)
			toDelete.Append(PathSeparator).Append(TCV("sessions"));
		if (!DeleteAllFiles(logger, toDelete.data))
			return false;

		StorageClientCreateInfo storageClientInfo(client, rootDir.data);
		storageClientInfo.Apply(config);
		auto& storageClient = *new StorageClient(storageClientInfo);
		auto scg = MakeGuard([&]() { delete &storageClient; });

		SessionClientCreateInfo sessionClientInfo(storageClient, client, logWriter);
		sessionClientInfo.rootDir = rootDir.data;
		sessionClientInfo.useUniqueId = false;
		//sessionClientInfo.allowKeepFilesInMemory = false;

		#if UBA_DEBUG
		sessionClientInfo.logToFile = true;
		#endif

		auto& sessionClient = *new SessionClient(sessionClientInfo);
		auto scg2 = MakeGuard([&]() { delete &sessionClient; });

		auto cg = MakeGuard([&]() { sessionClient.Stop(); disconnectGuard.Execute(); });

		storageClient.Start();
		sessionClient.Start();
		return testFunc(logger, sessionClient);
	}

	using TestServerClientSessionFunction = Function<bool(LoggerWithWriter& logger, const StringView& workingDir, SessionServer& sessionServer, SessionClient& sessionClient)>;
	bool SetupServerClientSession(LoggerWithWriter& logger, const StringBufferBase& testRootDir, bool deleteAll, bool serverShouldListen, const Config& serverConfig, const TestServerClientSessionFunction& testFunc)
	{
		return SetupServerSession(logger, testRootDir, deleteAll, serverShouldListen, serverConfig, [&](LoggerWithWriter& logger, const StringView& workingDir, SessionServer& sessionServer)
			{
				return SetupClientSession(logger, testRootDir, deleteAll, serverShouldListen, sessionServer.GetServer(), 1356, [&](LoggerWithWriter& logger, SessionClient& sessionClient)
					{
						return testFunc(logger, workingDir, sessionServer, sessionClient);
					});
			});
	}

	bool RunRemote(LoggerWithWriter& logger, const StringBufferBase& testRootDir, const Config& serverConfig, const TestSessionFunction& testFunc, bool deleteAll = true, bool serverShouldListen = true)
	{
		return SetupServerClientSession(logger, testRootDir, deleteAll, serverShouldListen, serverConfig,
			[&](LoggerWithWriter& logger, const StringView& workingDir, SessionServer& sessionServer, SessionClient& sessionClient)
			{
				return testFunc(logger, sessionServer, workingDir.data, [&](const ProcessStartInfo& pi) { return sessionServer.RunProcessRemote(pi); });
			});
	}

	bool RunRemote(LoggerWithWriter& logger, const StringBufferBase& testRootDir, const TestSessionFunction& testFunc, bool deleteAll, bool serverShouldListen)
	{
		return RunRemote(logger, testRootDir, {}, testFunc, deleteAll, serverShouldListen);
	}
	bool CreateTextFile(StringBufferBase& outPath, LoggerWithWriter& logger, const tchar* workingDir, const tchar* fileName, const char* text)
	{
		outPath.Clear().Append(workingDir).EnsureEndsWithSlash().Append(fileName);
		FileAccessor fr(logger, outPath.data);
		if (!fr.CreateWrite())
			return false;
		fr.Write(text, strlen(text) + 1);
		return fr.Close();
	}

	bool RunTestAppTests(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
	{
		StringBuffer<MaxPath> fileR;
		if (!CreateTextFile(fileR, logger, workingDir, TC("FileR.h"), "Foo"))
			return false;

		{
			StringBuffer<MaxPath> dir;
			dir.Append(workingDir).Append(TCV("Dir1"));
			if (!CreateDirectoryW(dir.data))
				return logger.Error(TC("Failed to create dir %s"), dir.data);

			dir.Clear().Append(workingDir).Append(TCV("Dir2"));
			if (!CreateDirectoryW(dir.data))
				return logger.Error(TC("Failed to create dir %s"), dir.data);
			dir.EnsureEndsWithSlash().Append(TCV("Dir3"));
			if (!CreateDirectoryW(dir.data))
				return logger.Error(TC("Failed to create dir %s"), dir.data);
			dir.EnsureEndsWithSlash().Append(TCV("Dir4"));
			if (!CreateDirectoryW(dir.data))
				return logger.Error(TC("Failed to create dir %s"), dir.data);
			dir.EnsureEndsWithSlash().Append(TCV("Dir5"));
			if (!CreateDirectoryW(dir.data))
				return logger.Error(TC("Failed to create dir %s"), dir.data);
		}

		if (!CreateTestFile(logger, ToView(workingDir), TCV("File4.out"), TCV("0")))
			return false;

		if (!RunTestApp(logger, workingDir, runProcess, TC("")))
			return false;

		{
			StringBuffer<MaxPath> fileW2;
			fileW2.Append(workingDir).Append(TCV("FileW2"));
			if (!FileExists(logger, fileW2.data))
				return logger.Error(TC("Can't find file %s"), fileW2.data);
		}
		{
			StringBuffer<MaxPath> fileWF;
			fileWF.Append(workingDir).Append(TCV("FileWF"));
			if (!FileExists(logger, fileWF.data))
				return logger.Error(TC("Can't find file %s"), fileWF.data);
		}
		return true;
	}

#if PLATFORM_MAC
	bool ExecuteCommand(LoggerWithWriter& logger, const tchar* command, StringBufferBase& commandOutput)
	{
		FILE* fpCommand = popen(command, "r");
		if (fpCommand == nullptr || fgets(commandOutput.data, commandOutput.capacity, fpCommand) == nullptr || pclose(fpCommand) != 0)
		{
			logger.Warning("Failed to run '%s' or get a response");
			return false;
		}

		commandOutput.count = strlen(commandOutput.data);
		while (isspace(commandOutput.data[commandOutput.count-1]))
		{
			commandOutput.data[commandOutput.count-1] = 0;
			commandOutput.count--;
		}
		return true;
	}
#endif

	bool RunClang(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
	{
		StringBuffer<MaxPath> sourceFile;
		sourceFile.Append(workingDir).Append(TCV("Code.cpp"));
		FileAccessor codeFile(logger, sourceFile.data);
		if (!codeFile.CreateWrite())
			return false;
		char code[] = "#include <stdio.h>\n int main() { printf(\"Hello world\\n\"); return 0; }";
		if (!codeFile.Write(code, sizeof(code) - 1))
			return false;
		if (!codeFile.Close())
			return false;

#if PLATFORM_WINDOWS
		const tchar* clangPath = TC("c:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Tools\\Llvm\\bin\\clang-cl.exe");
#elif PLATFORM_MAC
		StringBuffer<MaxPath> xcodePath;
		if (!ExecuteCommand(logger, "/usr/bin/xcrun --find clang++", xcodePath))
			return true;
		const tchar* clangPath = xcodePath.data;
#else
		const tchar* clangPath = TC("/usr/bin/clang++");
#endif

		if (!FileExists(logger, clangPath)) // Skipping if clang is not installed.
			return true;

		ProcessStartInfo processInfo;
		processInfo.application = clangPath;

		StringBuffer<MaxPath> args;

#if PLATFORM_WINDOWS
		args.Append("/Brepro ");
#elif PLATFORM_MAC
		StringBuffer<MaxPath> xcodeSDKPath;
		if (!ExecuteCommand(logger, "xcrun --show-sdk-path", xcodeSDKPath))
			return true;
		args.Append("-isysroot ");
		args.Append(xcodeSDKPath.data).Append(' ');
#endif
		args.Append(TCV("-o code Code.cpp"));

		processInfo.arguments = args.data;

		processInfo.workingDir = workingDir;
		//processInfo.logFile = TC("/mnt/e/temp/ttt/RunClang.log");
		ProcessHandle process = runProcess(processInfo);
		if (!process.WaitForExit(GetTimeoutTime()))
			return logger.Error(TC("clang++ timed out"));
		u32 exitCode = process.GetExitCode();
		if (exitCode != 0)
			return logger.Error(TC("clang++ returned exit code %u"), exitCode);
		return true;
	}

	bool RunCustomService(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
	{
		bool gotMessage = false;
		session.RegisterCustomService([&](uba::Process& process, const void* recv, u32 recvSize, void* send, u32 sendCapacity)
			{
				gotMessage = true;
				//wprintf(L"GOT MESSAGE: %.*s\n", recvSize / 2, (const wchar_t*)recv);
				const wchar_t* hello = L"Hello response from server";
				u64 helloBytes = wcslen(hello) * 2;
				memcpy(send, hello, helloBytes);
				return u32(helloBytes);
			});

		if (!RunTestApp(logger, workingDir, runProcess, TC("Whatever")))
			return false;
		if (!gotMessage)
			return logger.Error(TC("Never got message from UbaTestApp"));
		return true;
	}

	// NOTE: This test is dependent on the UbaTestApp<Platform>
	// The purpose of this test is to validate that the platform specific detours are
	// working as expected.
	// Before running the actual UbaTestApp, RunLocal calls through a variety of functions
	// that sets up the various UbaSession Servers, Clients, etc. It creates some temporary
	// directories, e.g. Dir1 and eventually call ProcessImpl::InternalCreateProcess.
	// InternalCreateProcess will setup the shared memory, inject the Detour library
	// and setup any other necessary environment variables, and spawn the actual process
	// (in this case the UbaTestApp)
	// Once UbaTestApp has started, it will first check and validate that the detour library
	// is in the processes address space. With the detour in place, the test app will
	// exercise various file functions which will actually go through our detour library.
	bool TestDetouredTestApp(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, RunTestAppTests);
	}

	bool TestRemoteDetouredTestApp(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunRemote(logger, testRootDir, RunTestAppTests);
	}

	bool TestCustomService(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunRemote(logger, testRootDir, RunCustomService);
	}

	bool TestDetouredClang(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, RunClang);
	}

	bool TestRemoteDetouredClang(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		// Run twice to test LoadCasTable/SaveCasTable etc
		if (!RunRemote(logger, testRootDir, RunClang))
			return false;
		return RunRemote(logger, testRootDir, RunClang, false);
	}

	bool TestDetouredTouch(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{
				StringBuffer<> file;
				file.Append(workingDir).Append(TCV("TouchFile.h"));
				FileAccessor fr(logger, file.data);

				CHECK_TRUE(fr.CreateWrite());
				CHECK_TRUE(fr.Write("Foo", 4));
				CHECK_TRUE(fr.Close());
				FileInformation oldInfo;
				CHECK_TRUE(GetFileInformation(oldInfo, logger, file.data));

				Sleep(100);

				ProcessStartInfo processInfo;
				processInfo.application = TC("/usr/bin/touch");
				processInfo.workingDir = workingDir;
				processInfo.arguments = file.data;
				processInfo.logFile = TC("/home/honk/Touch.log");
				ProcessHandle process = runProcess(processInfo);
				if (!process.WaitForExit(GetTimeoutTime()))
					return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
				u32 exitCode = process.GetExitCode();
				if (exitCode != 0)
					return false;

				FileInformation newInfo;
				CHECK_TRUE(GetFileInformation(newInfo, logger, file.data));
				if (newInfo.lastWriteTime == oldInfo.lastWriteTime)
					return logger.Error(TC("File time not changed after touch"));
				return true;
			});
	}

	bool TestDetouredPopen(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		#if PLATFORM_LINUX
		return RunLocal(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{
				StringBuffer<> testApp;
				GetTestAppPath(logger, testApp);

				ProcessStartInfo processInfo;
				processInfo.application = testApp.data;
				processInfo.workingDir = workingDir;
				processInfo.arguments = "-popen";
				processInfo.logLineFunc = [](void* userData, const tchar* line, u32 length, LogEntryType type)
					{
						LoggerWithWriter(g_consoleLogWriter, TC("")).Info(line);
					};

				ProcessHandle process = runProcess(processInfo);
				if (!process.WaitForExit(GetTimeoutTime()))
					return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
				u32 exitCode = process.GetExitCode();

				if (exitCode != 0)
				{
					for (auto& logLine : process.GetLogLines())
						logger.Error(logLine.text.c_str());
					return logger.Error(TC("UbaTestApp returned exit code %u"), exitCode);
				}
				return true;
			});
		#else
		return true;
		#endif
	}

	const tchar* GetSystemApplication()
	{
		#if PLATFORM_WINDOWS
		return TC("c:\\windows\\system32\\ping.exe");
		#elif PLATFORM_LINUX
		return TC("/usr/bin/cat");
		#else
		return TC("/sbin/zip");
		#endif
	}

	const tchar* GetSystemArguments()
	{
		#if PLATFORM_WINDOWS
		return TC("-n 1 localhost");
		#elif PLATFORM_LINUX
		return TC("--help");
		#else
		return TC("-help");
		#endif
	}

	const tchar* GetSystemExpectedLogLine()
	{
		#if PLATFORM_WINDOWS
		return TC("Pinging ");
		#elif PLATFORM_LINUX
		return TC("cat [OPTION]");
		#else
		return TC("zip [-options]");
		#endif
	}

	bool TestMultipleDetouredProcesses(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{
				ProcessStartInfo processInfo;
				processInfo.application = GetSystemApplication();
				processInfo.workingDir = workingDir;
				processInfo.arguments = GetSystemArguments();
				//processInfo.logFile = TC("e:\\temp\\ttt\\LogFile.log");
				Vector<ProcessHandle> processes;

				for (u32 i=0; i!=50; ++i)
					processes.push_back(runProcess(processInfo));

				for (auto& process : processes)
				{
					if (!process.WaitForExit(GetTimeoutTime()))
						return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
					u32 exitCode = process.GetExitCode();
					if (exitCode != 0)
						return logger.Error(TC("UbaTestApp exited with code %u"), exitCode);
				}

				return true;
			});
	}

	bool RunSystemApplicationAndLookForLog(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
	{
		ProcessStartInfo processInfo;
		processInfo.application = GetSystemApplication();
		processInfo.workingDir = workingDir;
		processInfo.arguments = GetSystemArguments();

		bool foundPingString = false;
		processInfo.logLineUserData = &foundPingString;
		processInfo.logLineFunc = [](void* userData, const tchar* line, u32 length, LogEntryType type)
			{
				*(bool*)userData |= Contains(line, GetSystemExpectedLogLine());
			};

		ProcessHandle process = runProcess(processInfo);

		if (!process.WaitForExit(GetTimeoutTime()))
			return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
		u32 exitCode = process.GetExitCode();
		if (exitCode != 0)
			return logger.Error(TC("Got exit code %u"), exitCode);
		if (!foundPingString)
			return logger.Error(TC("Did not log string containing \"%s\""), GetSystemExpectedLogLine());
		return true;
	}

	bool TestLogLines(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, RunSystemApplicationAndLookForLog);
	}

	bool TestLogLinesNoDetour(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, RunSystemApplicationAndLookForLog, false);
	}

	bool CheckAttributes(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
	{
		StringBuffer<MaxPath> testApp;
		GetTestAppPath(logger, testApp);
		ProcessStartInfo processInfo;
		processInfo.application = testApp.data;
		processInfo.workingDir = workingDir;
		processInfo.logLineFunc = [](void* userData, const tchar* line, u32 length, LogEntryType type)
			{
				LoggerWithWriter(g_consoleLogWriter, TC("")).Info(line);
			};

		auto GetAttributes = [&](const StringView& file) -> u32
			{
				StringBuffer<> arg(TC("-GetFileAttributes="));
				arg.Append(file);
				processInfo.arguments = arg.data;
				ProcessHandle process = runProcess(processInfo);
				if (!process.WaitForExit(GetTimeoutTime()))
					return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
				u32 exitCode =  process.GetExitCode();
				return exitCode == 255 ? INVALID_FILE_ATTRIBUTES : exitCode;
			};

		MemoryBlock temp;
		DirectoryTable dirTable(temp);
		dirTable.Init(session.GetDirectoryTableMemory(), 0, 0);

		CHECK_TRUE(session.RefreshDirectory(workingDir, true));
		CHECK_TRUE(session.RefreshDirectory(workingDir));
		CHECK_TRUE(dirTable.EntryExists(ToView(workingDir)) == DirectoryTable::Exists_Maybe);
		dirTable.ParseDirectoryTable(session.GetDirectoryTableSize());
		CHECK_TRUE(dirTable.EntryExists(ToView(workingDir), true) == DirectoryTable::Exists_Yes);

		StringBuffer<MaxPath> sourceFile;
		sourceFile.Append(workingDir).Append(TCV("Code.cpp"));

		CHECK_TRUE(GetAttributes(sourceFile) == INVALID_FILE_ATTRIBUTES);
		FileAccessor codeFile(logger, sourceFile.data);
		CHECK_TRUE(codeFile.CreateWrite());
		CHECK_TRUE(codeFile.Close());
		CHECK_TRUE(session.RegisterNewFile(sourceFile.data));
		CHECK_TRUE(GetAttributes(sourceFile) != INVALID_FILE_ATTRIBUTES);

		CHECK_TRUE(dirTable.EntryExists(sourceFile) == DirectoryTable::Exists_No);
		dirTable.ParseDirectoryTable(session.GetDirectoryTableSize());
		CHECK_TRUE(dirTable.EntryExists(sourceFile) == DirectoryTable::Exists_Yes);

		StringBuffer<MaxPath> newDir;
		newDir.Append(workingDir).Append(TCV("NewDir"));
		StringBuffer<MaxPath> newDirAndSlash(newDir);
		newDirAndSlash.Append('/');

		CHECK_TRUE(GetAttributes(newDir) == INVALID_FILE_ATTRIBUTES);
		CHECK_TRUE(CreateDirectoryW(newDir.data));
		CHECK_TRUE(session.RegisterNewFile(newDir.data));
		CHECK_TRUE(dirTable.EntryExists(newDir) == DirectoryTable::Exists_No);
		dirTable.ParseDirectoryTable(session.GetDirectoryTableSize());
		CHECK_TRUE(dirTable.EntryExists(newDir) == DirectoryTable::Exists_Yes);
		CHECK_TRUE(GetAttributes(newDir) != INVALID_FILE_ATTRIBUTES);
		CHECK_TRUE(GetAttributes(newDirAndSlash) != INVALID_FILE_ATTRIBUTES);

		StringBuffer<MaxPath> newDir2;
		newDir2.Append(workingDir).Append(TCV("NewDir2"));
		CHECK_TRUE(CreateDirectoryW(newDir2.data));
		CHECK_TRUE(GetAttributes(newDir2) == INVALID_FILE_ATTRIBUTES);
		CHECK_TRUE(session.RefreshDirectory(workingDir))
		CHECK_TRUE(GetAttributes(newDir2) != INVALID_FILE_ATTRIBUTES);
		dirTable.ParseDirectoryTable(session.GetDirectoryTableSize());
		CHECK_TRUE(dirTable.EntryExists(newDir2) == DirectoryTable::Exists_Yes);

		return true;
	}

	bool TestRegisterChanges(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, CheckAttributes);
	}

	bool TestRegisterChangesRemote(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunRemote(logger, testRootDir, CheckAttributes);
	}

	bool TestSharedReservedMemory(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, [](LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
			{
				StringBuffer<MaxPath> testApp;
				GetTestAppPath(logger, testApp);

				ProcessStartInfo processInfo;
				processInfo.application = testApp.data;
				processInfo.workingDir = workingDir;
				processInfo.arguments = TC("-sleep=100000");
				Vector<ProcessHandle> processes;

				for (u32 i=0; i!=128; ++i)
					processes.push_back(runProcess(processInfo));

				for (auto& process : processes)
				{
					if (!process.WaitForExit(100000))
						return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
					u32 exitCode = process.GetExitCode();
					if (exitCode != 0)
						return false;
				}

				return true;
			});
	}


	bool TestRemoteDirectoryTable(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
#if 0
		return SetupServerClientSession(logger, testRootDir, true, true, {},
			[&](LoggerWithWriter& logger, const StringView& workingDir, SessionServer& sessionServer, SessionClient& sessionClient)
			{
				u32 attributes;
				#if PLATFORM_WINDOWS
				CHECK_TRUE(sessionClient.Exists(TCV("c:\\"), attributes));
				CHECK_TRUE(sessionClient.Exists(TCV("c:\\windows"), attributes));
				CHECK_TRUE(IsDirectory(attributes));
				CHECK_TRUE(!sessionClient.Exists(TCV("q:\\"), attributes))
				CHECK_TRUE(!sessionClient.Exists(TCV("r:\\foo"), attributes))
				#else
				CHECK_TRUE(!sessionClient.Exists(TCV("/ergewrgergreg"), attributes));
				CHECK_TRUE(!sessionClient.Exists(TCV("/ergergreg/h5r6tyh"), attributes));
				#endif
				return true;
			});
#else
		return true;
#endif
	}

	bool RunVirtualFileTest(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
	{
		StringBuffer<> inFile(workingDir);
		inFile.Append(TCV("VirtualFile.in"));
		if (!session.CreateVirtualFile(inFile.data, "FOO", 3))
			return false;
		ProcessHandle ph;
		if (!RunTestApp(logger, workingDir, runProcess, TC("-virtualFile"), &ph))
			return false;

		StringBuffer<> outFile(workingDir);
		outFile.Append(TCV("VirtualFile.out"));

		bool success = false;
		ph.TraverseOutputFiles([&](StringView file) { success = file.Equals(outFile); });
		if (!success)
			return false;

		if (!session.DeleteVirtualFile(inFile.data))
			return false;

		if (FileExists(logger, outFile.data))
			return false;
		u64 outSize;
		if (!session.GetOutputFileSize(outSize, outFile.data))
			return false;
		if (outSize != 3)
			return false;
		u8 data[3];
		if (!session.GetOutputFileData(data, outFile.data, false))
			return false;
		if (memcmp(data, "BAR", 3) != 0)
			return false;
		if (FileExists(logger, outFile.data))
			return false;
		if (!session.WriteOutputFile(outFile.data, true))
			return false;
		if (!FileExists(logger, outFile.data))
			return false;

		if (ph.IsRemote())
			if (!session.GetStorage().DeleteCasForFile(inFile.data))
				return false;

		if (session.GetOutputFileSize(outSize, outFile.data))
			return false;
		if (session.GetOutputFileData(data, outFile.data, true))
			return false;
		return true;
	}

	bool TestVirtualFile(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		Config config;
		config.AddTable(TC("Session")).AddValue(TC("ShouldWriteToDisk"), false);
		return RunLocal(logger, testRootDir, config, RunVirtualFileTest);
	}

	bool TestRemoteVirtualFile(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		Config config;
		config.AddTable(TC("Session")).AddValue(TC("ShouldWriteToDisk"), false);
		config.AddTable(TC("Storage")).AddValue(TC("CreateIndependentMappings"), true);
		return RunRemote(logger, testRootDir, config, RunVirtualFileTest);
	}

	#if PLATFORM_MAC
	bool RunXCodeSelect(LoggerWithWriter& logger, SessionServer& session, const tchar* workingDir, const RunProcessFunction& runProcess)
	{
		#if UBA_DEBUG // Failing on farm for some reason... need to revisit
		StringBuffer<> xcodeSelect;
		if (!ExecuteCommand(logger, "which xcode-select", xcodeSelect))
			return true;
		return RunTestApp(logger, workingDir, runProcess, TC("-xcode-select"));
		#else
		return true;
		#endif
	}
	bool TestXcodeSelect(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunLocal(logger, testRootDir, RunXCodeSelect);
	}
	bool TestRemoteXcodeSelect(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		return RunRemote(logger, testRootDir, RunXCodeSelect);
	}
	#endif

	bool TestRemoteProcessSpecialCase1(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		Config config;
		return SetupServerSession(logger, testRootDir, true, true, config,
			[&](LoggerWithWriter& logger, const StringView& workingDir, SessionServer& sessionServer)
			{
				auto runProcess = [&](const ProcessStartInfo& pi) { return sessionServer.RunProcessRemote(pi); };
				if (!CreateTestFile(logger, workingDir, TCV("SpecialFile1"), TCV("0")))
					return false;

				const StringView casFile(TCV("UbaClient1357/cas/4d/4d067153ac729a4a7e8220c97935ffba67487800"));

				if (!SetupClientSession(logger, testRootDir, true, false, sessionServer.GetServer(), 1357, [&](LoggerWithWriter& logger, SessionClient& sessionClient)
					{
						if (!CreateTestFile(logger, testRootDir, casFile, TCV("0")))
							return false;
						return RunTestApp(logger, workingDir.data, runProcess, TC("-readwrite=0"));
					}))
					return false;

				if (!DeleteTestFile(logger, testRootDir, casFile))
					return false;

				if (!SetupClientSession(logger, testRootDir, false, false, sessionServer.GetServer(), 1357, [&](LoggerWithWriter& logger, SessionClient& sessionClient)
					{
						if (!RunTestApp(logger, workingDir.data, runProcess, TC("-readwrite=1")))
							return false;
						return true;
					}))
					return false;


				return true;
			});
	}

	bool TestSessionSpecialCopy(LoggerWithWriter& logger, const StringBufferBase& testRootDir)
	{
		Config config;
		return SetupServerSession(logger, testRootDir, true, true, config,
			[&](LoggerWithWriter& logger, const StringView& workingDir, SessionServer& sessionServer)
			{
				if (!CreateTestFile(logger, workingDir, TCV("File.h"), TCV("0")))
					return false;

				ProcessStartInfo processInfo;
				processInfo.application = TC("cmd.exe");
				processInfo.workingDir = workingDir.data;
				processInfo.arguments = TC("/c copy /Y \"File.h\" \"File2.h\"");
				ProcessHandle process = sessionServer.RunProcess(processInfo);
				if (!process.IsValid())
					return logger.Error(TC("Failed to start process"));
				if (!process.WaitForExit(GetTimeoutTime()))
					return logger.Error(TC("UbaTestApp did not exit in 10 seconds"));
				if (!Equals(process.GetStartInfo().application, TC("ubacopy")))
					return logger.Error(TC("Special copy was not used"));
				u32 exitCode = process.GetExitCode();
				if (exitCode != 0)
					return logger.Error(TC("Special copy failed"));
				if (!FileExists(logger, workingDir, TCV("File2.h")))
					return false;
				return true;
			});
	}
}
