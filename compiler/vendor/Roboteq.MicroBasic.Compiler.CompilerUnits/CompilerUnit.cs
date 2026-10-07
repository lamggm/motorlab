using System;
using Irony.Ast;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class CompilerUnit : AstNode
{
	private bool _dontCreateTokens = false;

	public virtual bool DontCreateTokens
	{
		get
		{
			return _dontCreateTokens;
		}
		set
		{
			_dontCreateTokens = value;
		}
	}

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		foreach (ParseTreeNode childNode in treeNode.ChildNodes)
		{
			if (childNode.AstNode is AstNode)
			{
				(childNode.AstNode as AstNode).Parent = this;
				ChildNodes.Add(childNode.AstNode as AstNode);
			}
		}
	}

	public virtual void InitializeUnit(CompilerContext context)
	{
		foreach (AstNode childNode in ChildNodes)
		{
			if (childNode is CompilerUnit)
			{
				(childNode as CompilerUnit).InitializeUnit(context);
			}
		}
	}

	public virtual void CreateTokens(CompilerContext context)
	{
		foreach (AstNode childNode in ChildNodes)
		{
			if (childNode is CompilerUnit)
			{
				(childNode as CompilerUnit).CreateTokens(context);
			}
		}
	}

	public virtual int ComputeValue(CompilerContext context)
	{
		throw new NotImplementedException($"{GetType().Name} doesn't implement ComputeValue.");
	}
}
