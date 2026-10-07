using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class ElseIfCompilerUnit : CompilerUnit
{
	private CompilerUnit _condition;

	private CompilerUnit _statements;

	private ElseIfCompilerUnit _elseIf;

	private ElseCompilerUnit _else;

	private bool _hasStatements;

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

	public ElseIfCompilerUnit ElseIf
	{
		get
		{
			return _elseIf;
		}
		set
		{
			_elseIf = value;
		}
	}

	public ElseCompilerUnit Else
	{
		get
		{
			return _else;
		}
		set
		{
			_else = value;
		}
	}

	public bool HasStatements
	{
		get
		{
			return _hasStatements;
		}
		set
		{
			_hasStatements = value;
		}
	}

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		_hasStatements = treeNode.ChildNodes.Count > 0;
		if (!_hasStatements)
		{
			return;
		}
		if (treeNode.ChildNodes[0].AstNode is ElseCompilerUnit)
		{
			Else = treeNode.ChildNodes[0].AstNode as ElseCompilerUnit;
			return;
		}
		_condition = treeNode.ChildNodes[1].AstNode as CompilerUnit;
		if (treeNode.ChildNodes.Count == 5)
		{
			_statements = treeNode.ChildNodes[3].AstNode as CompilerUnit;
			_elseIf = treeNode.ChildNodes[4].AstNode as ElseIfCompilerUnit;
		}
		else
		{
			_statements = treeNode.ChildNodes[2].AstNode as CompilerUnit;
			_elseIf = treeNode.ChildNodes[3].AstNode as ElseIfCompilerUnit;
		}
	}

	public void CreateTokens(CompilerContext context, string endLabel)
	{
		if (!HasStatements)
		{
			return;
		}
		if (Else != null)
		{
			Else.CreateTokens(context, endLabel);
			return;
		}
		string labelName = context.CreateLabel(this);
		Condition.CreateTokens(context);
		context.AddToken(new GotoToken(Span, labelName, DeviceCommand.JUMPIFFALSE));
		Statements.CreateTokens(context);
		if (ElseIf.HasStatements)
		{
			context.AddToken(new GotoToken(Span, endLabel));
		}
		context.AddToken(new LabelToken(labelName));
		if (ElseIf.HasStatements)
		{
			ElseIf.CreateTokens(context, endLabel);
		}
		context.AddToken(new LabelToken(endLabel));
	}
}
