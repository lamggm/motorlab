using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class AssignmentCompilerUnit : CompilerUnit, IVariableAssignment
{
	private VariableCompilerUnit _variable;

	private string _operator;

	private CompilerUnit _rightOperand;

	private DeviceCommand _command;

	public VariableCompilerUnit Variable
	{
		get
		{
			return _variable;
		}
		set
		{
			_variable = value;
		}
	}

	public string Operator
	{
		get
		{
			return _operator;
		}
		set
		{
			_operator = value;
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
		_variable = treeNode.ChildNodes[0].AstNode as VariableCompilerUnit;
		_operator = treeNode.ChildNodes[1].Token.ValueString;
		_rightOperand = treeNode.ChildNodes[2].AstNode as CompilerUnit;
		_command = DeviceCommand.None;
		switch (Operator)
		{
		case "=":
			break;
		case "+=":
			_command = DeviceCommand.PLUS;
			break;
		case "-=":
			_command = DeviceCommand.MINUS;
			break;
		case "*=":
			_command = DeviceCommand.MULT;
			break;
		case "/=":
			_command = DeviceCommand.DIV;
			break;
		case "<<=":
			_command = DeviceCommand.LSL;
			break;
		case ">>=":
			_command = DeviceCommand.LSR;
			break;
		}
	}

	public override void InitializeUnit(CompilerContext context)
	{
		Variable.InitializeUnit(context, CompilerVariableUsage.SetValue);
		if (RightOperand != null)
		{
			RightOperand.InitializeUnit(context);
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		if (Command != DeviceCommand.None)
		{
			Variable.CreateTokens(context, CompilerVariableUsage.GetValue);
		}
		if (RightOperand != null)
		{
			RightOperand.CreateTokens(context);
		}
		if (Command != DeviceCommand.None)
		{
			context.AddToken(Command);
		}
		Variable.CreateTokens(context, CompilerVariableUsage.SetValue);
	}
}
