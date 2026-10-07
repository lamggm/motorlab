using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class BlockIfCompilerUnit : CompilerUnit
{
	private CompilerUnit _condition;

	private CompilerUnit _statements;

	private ElseIfCompilerUnit _elseIf;

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

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		_condition = treeNode.ChildNodes[1].AstNode as CompilerUnit;
		if (treeNode.ChildNodes.Count == 7)
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

	public override void CreateTokens(CompilerContext context)
	{
		string labelName = context.CreateLabel(this);
		string text = "";
		Condition.CreateTokens(context);
		context.AddToken(new GotoToken(Span, labelName, DeviceCommand.JUMPIFFALSE));
		Statements.CreateTokens(context);
		if (ElseIf.HasStatements)
		{
			text = context.CreateLabel(this);
			context.AddToken(new GotoToken(Span, text));
		}
		context.AddToken(new LabelToken(labelName));
		if (ElseIf.HasStatements)
		{
			ElseIf.CreateTokens(context, text);
		}
	}
}
