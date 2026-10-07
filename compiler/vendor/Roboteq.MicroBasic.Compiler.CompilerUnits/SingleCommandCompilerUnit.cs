using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class SingleCommandCompilerUnit : CompilerUnit
{
	private DeviceCommand _command;

	public DeviceCommand Command
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

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		Command = DeviceCommand.None;
		string text = treeNode.ChildNodes[0].Token.ValueString.ToLower();
		string text2 = text;
		string text3 = text2;
		if (!(text3 == "terminate"))
		{
			if (text3 == "return")
			{
				Command = DeviceCommand.RETURN;
			}
		}
		else
		{
			Command = DeviceCommand.ENDPROG;
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		if (Command != DeviceCommand.None)
		{
			context.AddToken(Command);
		}
	}
}
