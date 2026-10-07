using System;
using System.Runtime.Serialization;
using System.Security;

namespace Roboteq.MicroBasic.Simulation;

[Serializable]
public class InterpreterException : Exception
{
	public InterpreterException()
	{
	}

	public InterpreterException(string message)
		: base(message)
	{
	}

	public InterpreterException(string message, Exception innerException)
		: base(message, innerException)
	{
	}

	[SecuritySafeCritical]
	protected InterpreterException(SerializationInfo info, StreamingContext context)
		: base(info, context)
	{
	}
}
