using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class StringCompilerUnit : CompilerUnit
{
	private string _value;

	public string Value
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
		_value = treeNode.Token.ValueString;
	}

	public override void InitializeUnit(CompilerContext context)
	{
	}

	public override void CreateTokens(CompilerContext context)
	{
	}
}
