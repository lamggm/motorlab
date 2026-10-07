using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class ElseCompilerUnit : CompilerUnit
{
	private CompilerUnit _statements;

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

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		_statements = treeNode.ChildNodes[1].AstNode as CompilerUnit;
	}

	public void CreateTokens(CompilerContext context, string endLabel)
	{
		Statements.CreateTokens(context);
		context.AddToken(new LabelToken(endLabel));
	}
}
