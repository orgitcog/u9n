// Copyright Epic Games, Inc. All Rights Reserved.

#if TEST_CLI

using AndroidZenServerPlugin;

TextWriter OriginalOut = Console.Out;
using ConsoleRedirector Redirector = new ConsoleRedirector((Level, Line) => OriginalOut.WriteLine($"[{Level}] {Line}"));

Console.WriteLine("Android Port Forwarder CLI testbed");

ManualResetEvent exitEvent = new ManualResetEvent(false);

Console.CancelKeyPress += (_, eventArgs) =>
{
	Console.WriteLine("Exit requested");
	eventArgs.Cancel = true;
	exitEvent.Set();
};

using AndroidPortForwarder forwarder = new AndroidPortForwarder();

forwarder.StartMonitor(string.Empty, null, 8558);

exitEvent.WaitOne();

#endif