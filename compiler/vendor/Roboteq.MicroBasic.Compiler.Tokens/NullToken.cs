using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.Tokens;

public class NullToken : DeviceToken
{
	private string _code;

	public override DeviceCommand Command
	{
		get
		{
			return DeviceCommand.None;
		}
		set
		{
		}
	}

	public string Code
	{
		get
		{
			return _code;
		}
		set
		{
			_code = value;
		}
	}

	public NullToken(string code)
		: base(DeviceCommand.None)
	{
		_code = code;
	}

	public override byte[] ToByteArray(CompilerContext context)
	{
		return new byte[0];
	}

	public override int GetLength(CompilerContext contxt)
	{
		return 0;
	}

	public override void AppendCode(CompilerContext context)
	{
		if (!string.IsNullOrEmpty(Code))
		{
			context.DeviceCode.AppendLine(Code);
		}
	}
}
