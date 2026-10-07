using System.Text;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.CompilerUnits;

namespace Roboteq.MicroBasic.Compiler.Tokens;

public class DeviceToken
{
	private DeviceCommand _command;

	private CompilerUnit _creator;

	private int _offset;

	public virtual DeviceCommand Command
	{
		get
		{
			return _command;
		}
		set
		{
			_command = value;
		}
	}

	public CompilerUnit Creator
	{
		get
		{
			return _creator;
		}
		set
		{
			_creator = value;
		}
	}

	public int Offset
	{
		get
		{
			return _offset;
		}
		protected set
		{
			_offset = value;
		}
	}

	public static void FormatCode(StringBuilder sb, byte[] code, DeviceCommand command)
	{
		DeviceCommandAttribute attribute = DeviceCommandAttribute.GetAttribute(command);
		FormatCode(sb, code, attribute.Name, attribute.Hint);
	}

	public static void FormatCode(StringBuilder sb, byte[] code, string commandName, string comment, params object[] args)
	{
		StringBuilder stringBuilder = new StringBuilder();
		foreach (byte b in code)
		{
			stringBuilder.Append("|" + b.ToString("X2"));
		}
		sb.AppendLine(string.Format(commandName, args).PadRight(20) + stringBuilder.ToString().PadRight(20) + string.Format("'" + comment, args));
	}

	public DeviceToken()
		: this(DeviceCommand.None)
	{
	}

	public DeviceToken(CompilerUnit creator)
		: this(creator, DeviceCommand.None)
	{
	}

	public DeviceToken(DeviceCommand command)
		: this(null, command)
	{
	}

	public DeviceToken(CompilerUnit creator, DeviceCommand command)
	{
		_creator = creator;
		_command = command;
	}

	public virtual void SetOffset(CompilerContext context, int offset)
	{
		Offset = offset;
	}

	public virtual void AppendOffset(StringBuilder sb)
	{
		sb.Append($"{Offset}: ".PadLeft(6));
	}

	public virtual byte[] ToByteArray(CompilerContext context)
	{
		if (Command == DeviceCommand.None)
		{
			return new byte[0];
		}
		return new byte[1] { (byte)Command };
	}

	public virtual int GetLength(CompilerContext contxt)
	{
		return ToByteArray(contxt).Length;
	}

	public virtual void AppendCode(CompilerContext context)
	{
		if (Command != DeviceCommand.None)
		{
			AppendOffset(context.DeviceCode);
			FormatCode(context.DeviceCode, ToByteArray(context), Command);
		}
	}
}
