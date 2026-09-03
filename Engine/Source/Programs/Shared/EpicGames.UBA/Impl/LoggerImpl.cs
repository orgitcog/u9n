// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace EpicGames.UBA.Impl
{
	internal class LoggerImpl : ILogger
	{
		delegate void BeginScopeCallback();
		delegate void EndScopeCallback();
		delegate void LogCallback(LogEntryType type, nint str, uint len);

		nint _handle = IntPtr.Zero;
		readonly Microsoft.Extensions.Logging.ILogger _logger;
		readonly BeginScopeCallback _beginScopeCallbackDelegate;
		readonly EndScopeCallback _endScopeCallbackDelegate;
		readonly LogCallback _logCallbackDelegate;
		readonly object _lock = new object();

		#region DllImport
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern nint GetDefaultLogWriter();

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern nint CreateCallbackLogWriter(BeginScopeCallback begin, EndScopeCallback end, LogCallback log);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void DestroyCallbackLogWriter(nint logger);
		#endregion

		public LoggerImpl(Microsoft.Extensions.Logging.ILogger logger)
		{
			_logger = logger;
			_beginScopeCallbackDelegate = BeginScope;
			_endScopeCallbackDelegate = EndScope;
			_logCallbackDelegate = Log;
			_handle = CreateCallbackLogWriter(_beginScopeCallbackDelegate, _endScopeCallbackDelegate, _logCallbackDelegate);
		}

		#region IDisposable
		~LoggerImpl() => Dispose(false);

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
			}

			if (_handle != IntPtr.Zero)
			{
				DestroyCallbackLogWriter(_handle);
				_handle = IntPtr.Zero;
			}
		}
		#endregion

		#region ILogger

		public nint GetHandle() => _handle;

		public void BeginScope()
		{
			Monitor.Enter(_lock);
		}

		public void EndScope()
		{
			Monitor.Exit(_lock);
		}

		public void Log(LogEntryType type, string message)
		{
			lock (_lock)
			{
				switch (type)
				{
					case LogEntryType.Error: _logger.LogError("{Message}", message); break;
					case LogEntryType.Warning: _logger.LogWarning("{Message}", message); break;
					case LogEntryType.Info: _logger.LogInformation("{Message}", message); break;
					case LogEntryType.Detail: _logger.LogDebug("{Message}", message); break;
					case LogEntryType.Debug: _logger.LogDebug("{Message}", message); break;
				}
			}
		}
		#endregion

		void Log(LogEntryType type, nint ptr, uint len) => Log(type, Marshal.PtrToStringAuto(ptr, (int)len) ?? String.Empty);
	}
}