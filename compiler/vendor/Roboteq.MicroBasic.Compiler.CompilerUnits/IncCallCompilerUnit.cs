using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class IncCallCompilerUnit : CompilerUnit, IVariableAssignment
{
	private VariableCompilerUnit _variable;

	private bool _isPostfix;

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

	public bool IsPostfix
	{
		get
		{
			return _isPostfix;
		}
		set
		{
			_isPostfix = value;
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
		_isPostfix = treeNode.ChildNodes[0].AstNode is VariableCompilerUnit;
		string valueString;
		if (_isPostfix)
		{
			Variable = treeNode.ChildNodes[0].AstNode as VariableCompilerUnit;
			valueString = treeNode.ChildNodes[1].Token.ValueString;
		}
		else
		{
			Variable = treeNode.ChildNodes[1].AstNode as VariableCompilerUnit;
			valueString = treeNode.ChildNodes[0].Token.ValueString;
		}
		_command = ((valueString == "--") ? DeviceCommand.DEC : DeviceCommand.INC);
	}

	public override void InitializeUnit(CompilerContext context)
	{
		Variable.InitializeUnit(context, CompilerVariableUsage.SetValue);
	}

	public override void CreateTokens(CompilerContext context)
	{
		Variable.CreateTokens(context, CompilerVariableUsage.GetValue);
		context.AddToken(Command);
		Variable.CreateTokens(context, CompilerVariableUsage.SetValue);
	}
}
