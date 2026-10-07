using System;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class NumberCompilerUnit : CompilerUnit
{
	private int _value;

	public int Value
	{
		get
		{
			return _value;
		}
		set
		{
			_value = value;
		}
	}

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		Value = Convert.ToInt32(treeNode.Token.Value);
	}

	public override void CreateTokens(CompilerContext context)
	{
		context.AddToken(new ConstToken(Value));
	}

	public override int ComputeValue(CompilerContext context)
	{
		return Value;
	}
}
