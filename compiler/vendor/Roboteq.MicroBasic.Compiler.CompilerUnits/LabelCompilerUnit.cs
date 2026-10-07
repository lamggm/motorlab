using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class LabelCompilerUnit : CompilerUnit
{
	private string _labelName;

	public string LabelName
	{
		get
		{
			return _labelName;
		}
		set
		{
			_labelName = value;
		}
	}

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		_labelName = treeNode.ChildNodes[0].Token.ValueString;
	}

	public override void InitializeUnit(CompilerContext context)
	{
		DontCreateTokens = !context.AddLabel(this, LabelName);
	}

	public override void CreateTokens(CompilerContext context)
	{
		if (!DontCreateTokens)
		{
			context.AddComment("Label named '{0}':", LabelName);
			context.AddToken(new LabelToken(LabelName));
		}
	}
}
