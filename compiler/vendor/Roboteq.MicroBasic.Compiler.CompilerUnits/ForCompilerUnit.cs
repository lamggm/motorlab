using System;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class ForCompilerUnit : CompilerUnit, ILoop
{
	private VariableCompilerUnit _variable;

	private CompilerUnit _initialValue;

	private CompilerUnit _endValue;

	private CompilerUnit _step;

	private CompilerUnit _statements;

	private string _endVariable;

	private string _stepVariable;

	private string _continueLabel;

	private string _exitLabel;

	private string _conditionLabel;

	private VariableCompilerUnit _nextVariable;

	private bool _isNewFor;

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

	public CompilerUnit InitialValue
	{
		get
		{
			return _initialValue;
		}
		set
		{
			_initialValue = value;
		}
	}

	public CompilerUnit EndValue
	{
		get
		{
			return _endValue;
		}
		set
		{
			_endValue = value;
		}
	}

	public CompilerUnit Step
	{
		get
		{
			return _step;
		}
		set
		{
			_step = value;
		}
	}

	public CompilerUnit Statements
	{
		get
		{
			return _statements;
		}
		set
		{
			_statements = value;
		}
	}

	public string EndVariable
	{
		get
		{
			return _endVariable;
		}
		set
		{
			_endVariable = value;
		}
	}

	public string StepVariable
	{
		get
		{
			return _stepVariable;
		}
		set
		{
			_stepVariable = value;
		}
	}

	public string LoopName => "For";

	public string ContinueLabel
	{
		get
		{
			return _continueLabel;
		}
		set
		{
			_continueLabel = value;
		}
	}

	public string ExitLabel
	{
		get
		{
			return _exitLabel;
		}
		set
		{
			_exitLabel = value;
		}
	}

	public string ConditionLabel
	{
		get
		{
			return _conditionLabel;
		}
		set
		{
			_conditionLabel = value;
		}
	}

	public VariableCompilerUnit NextVariable
	{
		get
		{
			return _nextVariable;
		}
		set
		{
			_nextVariable = value;
		}
	}

	public bool IsNewFor
	{
		get
		{
			return _isNewFor;
		}
		set
		{
			_isNewFor = value;
		}
	}

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		Variable = treeNode.ChildNodes[1].AstNode as VariableCompilerUnit;
		InitialValue = treeNode.ChildNodes[3].AstNode as CompilerUnit;
		IsNewFor = treeNode.ChildNodes[4].Token.ValueString.ToLower() == "andwhile";
		EndValue = treeNode.ChildNodes[5].AstNode as CompilerUnit;
		if (treeNode.ChildNodes[6].ChildNodes.Count > 0)
		{
			Step = treeNode.ChildNodes[6].ChildNodes[0].AstNode as CompilerUnit;
		}
		Statements = treeNode.ChildNodes[7].AstNode as CompilerUnit;
		if (treeNode.ChildNodes[8].ChildNodes.Count > 0)
		{
			NextVariable = treeNode.ChildNodes[7].ChildNodes[0].AstNode as VariableCompilerUnit;
		}
	}

	public override void InitializeUnit(CompilerContext context)
	{
		Variable.InitializeUnit(context, CompilerVariableUsage.SetValue);
		InitialValue.InitializeUnit(context);
		EndValue.InitializeUnit(context);
		if (Step != null)
		{
			Step.InitializeUnit(context);
		}
		if (Step is IVariableAssignment variableAssignment && !Variable.Variable.Name.Equals(variableAssignment.Variable.Variable.Name, StringComparison.CurrentCultureIgnoreCase))
		{
			context.AddError("Evaluate expression should contain the same variable as for statement", variableAssignment.Variable.Span);
		}
		Statements.InitializeUnit(context);
		if (!IsNewFor)
		{
			EndVariable = context.CreateVariable();
			StepVariable = context.CreateVariable();
		}
		if (NextVariable != null)
		{
			if (NextVariable.Variable.IsArray)
			{
				context.AddError("Arrays is not allowed in this context", NextVariable.Span);
			}
			else if (!NextVariable.Variable.Name.Equals(Variable.Variable.Name, StringComparison.CurrentCultureIgnoreCase))
			{
				context.AddError("For and next should use the same variable", NextVariable.Span);
			}
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		if (IsNewFor)
		{
			CreateNewForTokens(context);
		}
		else
		{
			CreateOldForTokens(context);
		}
	}

	private void CreateOldForTokens(CompilerContext context)
	{
		InitialValue.CreateTokens(context);
		Variable.CreateTokens(context, CompilerVariableUsage.SetValue);
		CreateEndTokens(context, CompilerVariableUsage.SetValue);
		CreateStepTokens(context, CompilerVariableUsage.SetValue);
		ConditionLabel = context.CreateLabel(this);
		ContinueLabel = context.CreateLabel(this);
		ExitLabel = context.CreateLabel(this);
		context.AddToken(new LabelToken(ConditionLabel));
		context.AddComment("For loop condition");
		CreateStepTokens(context, CompilerVariableUsage.GetValue);
		context.AddToken(DeviceCommand.CONST0);
		context.AddToken(DeviceCommand.IFGTE);
		Variable.CreateTokens(context, CompilerVariableUsage.GetValue);
		CreateEndTokens(context, CompilerVariableUsage.GetValue);
		context.AddToken(DeviceCommand.IFGT);
		context.AddToken(DeviceCommand.AND);
		CreateStepTokens(context, CompilerVariableUsage.GetValue);
		context.AddToken(DeviceCommand.CONST0);
		context.AddToken(DeviceCommand.IFLT);
		Variable.CreateTokens(context, CompilerVariableUsage.GetValue);
		CreateEndTokens(context, CompilerVariableUsage.GetValue);
		context.AddToken(DeviceCommand.IFLT);
		context.AddToken(DeviceCommand.AND);
		context.AddToken(DeviceCommand.OR);
		context.AddToken(new GotoToken(Span, ExitLabel, DeviceCommand.JUMPIFTRUE));
		Statements.CreateTokens(context);
		context.AddToken(new LabelToken(ContinueLabel));
		Variable.CreateTokens(context, CompilerVariableUsage.GetValue);
		CreateStepTokens(context, CompilerVariableUsage.GetValue);
		context.AddToken(DeviceCommand.PLUS);
		Variable.CreateTokens(context, CompilerVariableUsage.SetValue);
		context.AddToken(new GotoToken(Span, ConditionLabel));
		context.AddToken(new LabelToken(ExitLabel));
	}

	private void CreateEndTokens(CompilerContext context, CompilerVariableUsage usage)
	{
		if (usage == CompilerVariableUsage.GetValue)
		{
			context.AddComment("Retrieve for end value.");
		}
		else
		{
			context.AddComment("Store end expression in end variable.");
		}
		if (usage == CompilerVariableUsage.SetValue)
		{
			EndValue.CreateTokens(context);
		}
		context.AddToken(new ConstToken(context.GetVariableAddress(EndVariable)));
		DeviceCommand deviceCommand = DeviceCommand.None;
		deviceCommand = ((usage == CompilerVariableUsage.GetValue) ? DeviceCommand.ILOAD : DeviceCommand.ISTORE);
		context.AddToken(deviceCommand);
	}

	private void CreateStepTokens(CompilerContext context, CompilerVariableUsage usage)
	{
		if (usage == CompilerVariableUsage.GetValue)
		{
			context.AddComment("Retrieve for step value.");
		}
		else
		{
			context.AddComment("Store step expression in step variable.");
		}
		if (usage == CompilerVariableUsage.SetValue)
		{
			if (Step != null)
			{
				Step.CreateTokens(context);
			}
			else
			{
				context.AddToken(new ConstToken(1));
			}
		}
		context.AddToken(new ConstToken(context.GetVariableAddress(StepVariable)));
		DeviceCommand deviceCommand = DeviceCommand.None;
		deviceCommand = ((usage == CompilerVariableUsage.GetValue) ? DeviceCommand.ILOAD : DeviceCommand.ISTORE);
		context.AddToken(deviceCommand);
	}

	private void CreateNewForTokens(CompilerContext context)
	{
		InitialValue.CreateTokens(context);
		Variable.CreateTokens(context, CompilerVariableUsage.SetValue);
		ConditionLabel = context.CreateLabel(this);
		ContinueLabel = context.CreateLabel(this);
		ExitLabel = context.CreateLabel(this);
		context.AddToken(new LabelToken(ConditionLabel));
		context.AddComment("For loop condition");
		EndValue.CreateTokens(context);
		context.AddToken(new GotoToken(Span, ExitLabel, DeviceCommand.JUMPIFFALSE));
		Statements.CreateTokens(context);
		context.AddToken(new LabelToken(ContinueLabel));
		if (Step != null)
		{
			Step.CreateTokens(context);
		}
		else
		{
			Variable.CreateTokens(context, CompilerVariableUsage.GetValue);
			context.AddToken(DeviceCommand.INC);
			Variable.CreateTokens(context, CompilerVariableUsage.SetValue);
		}
		context.AddToken(new GotoToken(Span, ConditionLabel));
		context.AddToken(new LabelToken(ExitLabel));
	}
}
