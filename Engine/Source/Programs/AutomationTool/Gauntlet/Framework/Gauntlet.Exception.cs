// Copyright Epic Games, Inc. All Rights Reserved.

namespace Gauntlet
{
	public class TestException : System.Exception
	{
		public TestException(string Msg)
				: base(Msg)
		{
		}

		public TestException(string Message, System.Exception InnerException)
			: base(Message, InnerException)
		{
		}

		public TestException(string Format, params object[] Args)
				: base(string.Format(Format, Args))
		{
		}
	}

	public class NonCriticalTestException : TestException
	{
		public NonCriticalTestException(string Msg) : base(Msg)
		{
		}

		public NonCriticalTestException(string Message, System.Exception InnerException)
			: base(Message, InnerException)
		{
		}

		public NonCriticalTestException(string Format, params object[] Args)
			: base(string.Format(Format, Args))
		{
		}
	}


	public class DeviceException : TestException
	{
		public DeviceException(string Msg)
				: base(Msg)
		{
		}

		public DeviceException(string Message, System.Exception InnerException)
			: base(Message, InnerException)
		{
		}

		public DeviceException(string Format, params object[] Args)
				: base(Format, Args)
		{
		}
	}
}
