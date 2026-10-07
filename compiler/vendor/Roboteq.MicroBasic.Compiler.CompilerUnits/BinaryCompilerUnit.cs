using System;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class BinaryCompilerUnit : CompilerUnit
{
	private CompilerUnit _leftOperand;

	private CompilerUnit _rightOperand;

	private DeviceCommand _command;

	public CompilerUnit LeftOperand
	{
		get
		{
			return _leftOperand;
		}
		set
		{
			_leftOperand = value;
		}
	}

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
		_leftOperand = treeNode.ChildNodes[0].AstNode as CompilerUnit;
		string valueString = treeNode.ChildNodes[1].Token.ValueString;
		_rightOperand = treeNode.ChildNodes[2].AstNode as CompilerUnit;
		_command = DeviceCommand.None;
		switch (valueString.ToLower())
		{
		case "or":
			_command = DeviceCommand.OR;
			break;
		case "xor":
			_command = DeviceCommand.XOR;
			break;
		case "and":
			_command = DeviceCommand.AND;
			break;
		case "=":
			_command = DeviceCommand.IFEQ;
			break;
		case "<>":
			_command = DeviceCommand.IFNE;
			break;
		case "<":
			_command = DeviceCommand.IFLT;
			break;
		case "<=":
			_command = DeviceCommand.IFLTE;
			break;
		case ">":
			_command = DeviceCommand.IFGT;
			break;
		case ">=":
			_command = DeviceCommand.IFGTE;
			break;
		case "<<":
			_command = DeviceCommand.LSL;
			break;
		case ">>":
			_command = DeviceCommand.LSR;
			break;
		case "+":
			_command = DeviceCommand.PLUS;
			break;
		case "-":
			_command = DeviceCommand.MINUS;
			break;
		case "mod":
			_command = DeviceCommand.MOD;
			break;
		case "*":
			_command = DeviceCommand.MULT;
			break;
		case "/":
			_command = DeviceCommand.DIV;
			break;
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		if (LeftOperand != null)
		{
			LeftOperand.CreateTokens(context);
		}
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
		int num = LeftOperand.ComputeValue(context);
		int num2 = RightOperand.ComputeValue(context);
		return Command switch
		{
			DeviceCommand.PLUS => num + num2, 
			DeviceCommand.MINUS => num - num2, 
			DeviceCommand.MULT => num * num2, 
			DeviceCommand.DIV => num / num2, 
			DeviceCommand.MOD => num % num2, 
			DeviceCommand.AND => num & num2, 
			DeviceCommand.OR => num | num2, 
			DeviceCommand.XOR => num ^ num2, 
			DeviceCommand.LSL => num << num2, 
			DeviceCommand.LSR => num >> num2, 
			DeviceCommand.IFEQ => (num == num2) ? (-1) : 0, 
			DeviceCommand.IFNE => (num != num2) ? (-1) : 0, 
			DeviceCommand.IFGT => (num > num2) ? (-1) : 0, 
			DeviceCommand.IFGTE => (num >= num2) ? (-1) : 0, 
			DeviceCommand.IFLT => (num < num2) ? (-1) : 0, 
			DeviceCommand.IFLTE => (num <= num2) ? (-1) : 0, 
			_ => throw new Exception($"{Command} not implemented."), 
		};
	}
}
