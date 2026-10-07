using System;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class DirectiveCompilerUnit : CompilerUnit
{
	public string Command { get; set; }

	public string Option { get; set; }

	public string Identifier { get; set; }

	public CompilerUnit Expression { get; set; }

	public SourceSpan IdentifierSpan { get; set; }

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		if (treeNode.ChildNodes.Count == 0)
		{
			return;
		}
		Command = treeNode.ChildNodes[0].Token.ValueString;
		string text = Command.ToLower();
		string text2 = text;
		if (!(text2 == "option"))
		{
			if (text2 == "#define")
			{
				IdentifierSpan = treeNode.ChildNodes[1].Span;
				Identifier = treeNode.ChildNodes[1].Token.ValueString;
				Expression = treeNode.ChildNodes[2].AstNode as CompilerUnit;
			}
		}
		else
		{
			Option = treeNode.ChildNodes[1].Token.ValueString;
		}
	}

	public override void InitializeUnit(CompilerContext context)
	{
		if (Command == null)
		{
			return;
		}
		string text = Command.ToLower();
		string text2 = text;
		if (!(text2 == "option"))
		{
			if (text2 == "#define")
			{
				context.AddDirective(this);
			}
		}
		else
		{
			context.IsStrictDeclaration = Option.Equals("explicit", StringComparison.CurrentCultureIgnoreCase);
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		if (Command == null)
		{
			return;
		}
		string text = Command.ToLower();
		string text2 = text;
		if (!(text2 == "option"))
		{
			if (text2 == "#define")
			{
				context.AddText("'Pre-processor directive: {0}.", Identifier);
			}
		}
		else
		{
			context.AddText("'Compiler directive: {0} {1}.", Command.ToUpper(), Option.ToUpper());
		}
	}
}
