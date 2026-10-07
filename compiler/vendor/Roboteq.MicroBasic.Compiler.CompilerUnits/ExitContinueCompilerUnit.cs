using Irony.Ast;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class ExitContinueCompilerUnit : CompilerUnit
{
	private bool _isExit;

	private string _loopName;

	public bool IsExit
	{
		get
		{
			return _isExit;
		}
		set
		{
			_isExit = value;
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

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		IsExit = treeNode.ChildNodes[0].Token.ValueString.ToLower() == "exit";
		if (treeNode.ChildNodes.Count > 1)
		{
			LoopName = treeNode.ChildNodes[1].Token.ValueString;
		}
		else
		{
			LoopName = "Do";
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		AstNode parent = Parent;
		ILoop loop = null;
		while (parent != null)
		{
			loop = parent as ILoop;
			if (loop != null && loop.LoopName.ToLower() == LoopName.ToLower())
			{
				break;
			}
			parent = parent.Parent;
		}
		if (parent == null || loop == null)
		{
			context.AddError("No enclosing loop out of which exit or continue", Span);
		}
		else if (IsExit)
		{
			context.AddToken(new GotoToken(Span, loop.ExitLabel));
		}
		else
		{
			context.AddToken(new GotoToken(Span, loop.ContinueLabel));
		}
	}
}
