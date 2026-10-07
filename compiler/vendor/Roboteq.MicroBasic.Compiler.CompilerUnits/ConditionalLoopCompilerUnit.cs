using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class ConditionalLoopCompilerUnit : CompilerUnit, ILoop
{
	private CompilerUnit _condition;

	private CompilerUnit _statements;

	private bool _isUntil;

	private bool _isConditionFirst;

	private bool _isDo;

	private string _loopName;

	private string _continueLabel;

	private string _exitLabel;

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

	public bool IsUntil
	{
		get
		{
			return _isUntil;
		}
		set
		{
			_isUntil = value;
		}
	}

	public bool IsConditionFirst
	{
		get
		{
			return _isConditionFirst;
		}
		set
		{
			_isConditionFirst = value;
		}
	}

	public bool IsDo
	{
		get
		{
			return _isDo;
		}
		set
		{
			_isDo = value;
		}
	}

	public string LoopName
	{
		get
		{
			return _loopName;
		}
		set
		{
			_loopName = value;
		}
	}

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

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		IsDo = treeNode.ChildNodes.Count < 5;
		LoopName = (IsDo ? "Do" : "While");
		if (treeNode.ChildNodes[0].AstNode is CompilerUnit)
		{
			IsConditionFirst = false;
			IsUntil = treeNode.ChildNodes[1].Token.ValueString.ToLower() == "until";
			Condition = treeNode.ChildNodes[2].AstNode as CompilerUnit;
			Statements = treeNode.ChildNodes[0].AstNode as CompilerUnit;
		}
		else
		{
			IsConditionFirst = true;
			IsUntil = treeNode.ChildNodes[0].Token.ValueString.ToLower() == "until";
			Condition = treeNode.ChildNodes[1].AstNode as CompilerUnit;
			Statements = treeNode.ChildNodes[2].AstNode as CompilerUnit;
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		ContinueLabel = context.CreateLabel(this);
		ExitLabel = context.CreateLabel(this);
		context.AddToken(new LabelToken(ContinueLabel));
		if (IsConditionFirst)
		{
			Condition.CreateTokens(context);
			if (IsUntil)
			{
				context.AddToken(new GotoToken(Span, ExitLabel, DeviceCommand.JUMPIFTRUE));
			}
			else
			{
				context.AddToken(new GotoToken(Span, ExitLabel, DeviceCommand.JUMPIFFALSE));
			}
			Statements.CreateTokens(context);
			context.AddToken(new GotoToken(Span, ContinueLabel));
		}
		else
		{
			Statements.CreateTokens(context);
			Condition.CreateTokens(context);
			if (IsUntil)
			{
				context.AddToken(new GotoToken(Span, ContinueLabel, DeviceCommand.JUMPIFFALSE));
			}
			else
			{
				context.AddToken(new GotoToken(Span, ContinueLabel, DeviceCommand.JUMPIFTRUE));
			}
		}
		context.AddToken(new LabelToken(ExitLabel));
	}
}
