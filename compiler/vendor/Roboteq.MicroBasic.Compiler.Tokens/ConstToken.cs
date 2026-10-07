using System.Collections.Generic;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.Tokens;

public class ConstToken : DeviceToken
{
	private int _value;

	public int Value
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

	public ConstToken(int value)
		: this(value, force16: false)
	{
	}

	public ConstToken(int value, bool force16)
		: base(DeviceCommand.CONST32)
	{
		_value = value;
		if (force16)
		{
			base.Command = DeviceCommand.CONST16;
		}
		else if (value == 0)
		{
			base.Command = DeviceCommand.CONST0;
		}
		else if (value >= 0 && value <= 255)
		{
			base.Command = DeviceCommand.CONST8;
		}
		else if (value >= 0 && value <= 65535)
		{
			base.Command = DeviceCommand.CONST16;
		}
		else
		{
			base.Command = DeviceCommand.CONST32;
		}
	}

	public override byte[] ToByteArray(CompilerContext context)
	{
		List<byte> list = new List<byte>();
		list.AddRange(base.ToByteArray(context));
		switch (Command)
		{
		case DeviceCommand.CONST8:
			list.Add((byte)(Value & 0xFF));
			break;
		case DeviceCommand.CONST16:
			list.Add((byte)(Value & 0xFF));
			list.Add((byte)((Value & 0xFF00) >> 8));
			break;
		case DeviceCommand.CONST32:
			list.Add((byte)(Value & 0xFF));
			list.Add((byte)((Value & 0xFF00) >> 8));
			list.Add((byte)((Value & 0xFF0000) >> 16));
			list.Add((byte)((Value & 0xFF000000u) >> 24));
			break;
		}
		return list.ToArray();
	}

	public override void AppendCode(CompilerContext context)
	{
		if (Command == DeviceCommand.CONST0)
		{
			base.AppendCode(context);
			return;
		}
		AppendOffset(context.DeviceCode);
		DeviceCommandAttribute attribute = DeviceCommandAttribute.GetAttribute(Command);
		DeviceToken.FormatCode(context.DeviceCode, ToByteArray(context), attribute.Name.PadRight(7) + " {0}", attribute.Hint, Value);
	}
}
