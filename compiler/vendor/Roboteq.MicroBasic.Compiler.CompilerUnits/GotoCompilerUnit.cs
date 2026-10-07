using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class GotoCompilerUnit : CompilerUnit
{
	private string _labelName;

	private string _option;

	private bool _isGoSub;

	private SourceSpan _labelSpan;

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

	public string Option
	{
		get
		{
			return _option;
		}
		set
		{
			_option = value;
		}
	}

	public bool IsGoSub
	{
		get
		{
			return _isGoSub;
		}
		set
		{
			_isGoSub = value;
		}
	}

	public SourceSpan LabelSpan
	{
		get
		{
			return _labelSpan;
		}
		set
		{
			_labelSpan = value;
		}
	}

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		_option = treeNode.ChildNodes[0].Token.ValueString;
		_labelName = treeNode.ChildNodes[1].Token.ValueString;
		LabelSpan = treeNode.ChildNodes[1].Span;
		_isGoSub = _option.ToLower() == "gosub";
	}

	public override void InitializeUnit(CompilerContext context)
	{
	}

	public override void CreateTokens(CompilerContext context)
	{
		if (IsGoSub)
		{
			context.AddToken(new GotoToken(LabelSpan, LabelName, DeviceCommand.CALL));
		}
		else
		{
			context.AddToken(new GotoToken(LabelSpan, LabelName));
		}
	}
}
