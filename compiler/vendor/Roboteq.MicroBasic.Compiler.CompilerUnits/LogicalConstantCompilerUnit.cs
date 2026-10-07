using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class LogicalConstantCompilerUnit : CompilerUnit
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
		string text = treeNode.ChildNodes[0].Token.ValueString.ToLower();
		string text2 = text;
		if (!(text2 == "true"))
		{
			if (text2 == "false")
			{
				Value = 0;
			}
		}
		else
		{
			Value = -1;
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		context.AddComment("{0} value.", (Value == 0) ? "False" : "True");
		context.AddToken(new ConstToken(Value));
	}

	public override int ComputeValue(CompilerContext context)
	{
		return Value;
	}
}
