using System;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class UnaryCompilerUnit : CompilerUnit
{
	private CompilerUnit _rightOperand;

	private DeviceCommand _command;

	public CompilerUnit RightOperand
	{
		get
		{
			return _rightOperand;
		}
		set
		{
			_rightOperand = value;
		}
	}

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
		string valueString = treeNode.ChildNodes[0].Token.ValueString;
		_rightOperand = treeNode.ChildNodes[1].AstNode as CompilerUnit;
		_command = DeviceCommand.None;
		string text = valueString.ToLower();
		string text2 = text;
		if (!(text2 == "-"))
		{
			if (text2 == "not")
			{
				_command = DeviceCommand.INV;
			}
		}
		else
		{
			_command = DeviceCommand.NEG;
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		if (RightOperand != null)
		{
			RightOperand.CreateTokens(context);
		}
		if (Command != DeviceCommand.None)
		{
			context.AddToken(Command);
		}
	}

	public override int ComputeValue(CompilerContext context)
	{
		int num = RightOperand.ComputeValue(context);
		return Command switch
		{
			DeviceCommand.NEG => -num, 
			DeviceCommand.INV => ~num, 
			_ => throw new Exception($"{Command} not implemented."), 
		};
	}
}
