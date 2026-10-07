using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class NonBlockIfCompilerUnit : CompilerUnit
{
	private CompilerUnit _condition;

	private CompilerUnit _statementTrue;

	private CompilerUnit _statementFalse;

	public CompilerUnit Condition
	{
		get
		{
			return _condition;
		}
		set
		{
			_condition = value;
		}
	}

	public CompilerUnit StatementTrue
	{
		get
		{
			return _statementTrue;
		}
		set
		{
			_statementTrue = value;
		}
	}

	public CompilerUnit StatementFalse
	{
		get
		{
			return _statementFalse;
		}
		set
		{
			_statementFalse = value;
		}
	}

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		_condition = treeNode.ChildNodes[1].AstNode as CompilerUnit;
		_statementTrue = treeNode.ChildNodes[3].AstNode as CompilerUnit;
		if (treeNode.ChildNodes[4].ChildNodes.Count > 0)
		{
			_statementFalse = treeNode.ChildNodes[4].ChildNodes[1].AstNode as CompilerUnit;
		}
	}

	public override void InitializeUnit(CompilerContext context)
	{
		base.InitializeUnit(context);
		if (StatementFalse != null)
		{
			StatementFalse.InitializeUnit(context);
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		string labelName = context.CreateLabel(this);
		string labelName2 = "";
		Condition.CreateTokens(context);
		context.AddToken(new GotoToken(Span, labelName, DeviceCommand.JUMPIFFALSE));
		StatementTrue.CreateTokens(context);
		if (StatementFalse != null)
		{
			labelName2 = context.CreateLabel(this);
			context.AddToken(new GotoToken(Span, labelName2));
		}
		context.AddToken(new LabelToken(labelName));
		if (StatementFalse != null)
		{
			StatementFalse.CreateTokens(context);
			context.AddToken(new LabelToken(labelName2));
		}
	}
}
