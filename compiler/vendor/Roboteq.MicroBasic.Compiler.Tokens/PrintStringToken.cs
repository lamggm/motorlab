using System.Collections.Generic;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.Tokens;

public class PrintStringToken : DeviceToken
{
	private string _value;

	public string Value
	{
		get
		{
			return _value;
		}
		set
		{
			_value = value;
		}
	}

	public PrintStringToken(string value)
		: base(DeviceCommand.PRINTSTRING)
	{
		_value = value;
	}

	public override byte[] ToByteArray(CompilerContext context)
	{
		List<byte> list = new List<byte>();
		list.AddRange(base.ToByteArray(context));
		list.Add((byte)Value.Length);
		string value = Value;
		foreach (char c in value)
		{
			list.Add((byte)c);
		}
		return list.ToArray();
	}
}
