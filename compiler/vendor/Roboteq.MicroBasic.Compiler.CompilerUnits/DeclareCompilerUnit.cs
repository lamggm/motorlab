using System;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class DeclareCompilerUnit : CompilerUnit
{
	private CompilerVariable _variable;

	private SourceSpan _nameSpan;

	private SourceSpan _lengthSpan;

	public CompilerVariable Variable
	{
		get
		{
			return _variable;
		}
		set
		{
			_variable = value;
		}
	}

	public SourceSpan NameSpan
	{
		get
		{
			return _nameSpan;
		}
		set
		{
			_nameSpan = value;
		}
	}

	public SourceSpan LengthSpan
	{
		get
		{
			return _lengthSpan;
		}
		set
		{
			_lengthSpan = value;
		}
	}

	public CompilerUnit LengthCompilerUnit { get; set; }

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		_variable = new CompilerVariable();
		_variable.Name = treeNode.ChildNodes[0].Token.ValueString;
		_nameSpan = treeNode.ChildNodes[0].Span;
		if (treeNode.ChildNodes.Count == 2)
		{
			_variable.Type = (CompilerVariableType)Enum.Parse(typeof(CompilerVariableType), treeNode.ChildNodes[1].Token.ValueString, ignoreCase: true);
			_variable.Length = 1;
			return;
		}
		_variable.IsArray = true;
		_lengthSpan = treeNode.ChildNodes[1].Span;
		_variable.Type = (CompilerVariableType)Enum.Parse(typeof(CompilerVariableType), treeNode.ChildNodes[2].Token.ValueString, ignoreCase: true);
		LengthCompilerUnit = treeNode.ChildNodes[1].AstNode as CompilerUnit;
	}

	public override void InitializeUnit(CompilerContext context)
	{
		if (CompilerHelper.IsNamedConstant(Variable.Name))
		{
			context.AddError($"'{Variable.Name}' is a named constant and could not be used as variable name", NameSpan);
			return;
		}
		if (Variable.Name.StartsWith("_"))
		{
			context.AddError($"'{Variable.Name}' is not a valid variable name, variables should not start with _ character.", NameSpan);
			return;
		}
		if (Variable.IsArray)
		{
			try
			{
				Variable.Length = LengthCompilerUnit.ComputeValue(context);
			}
			catch (Exception ex)
			{
				context.AddError($"Array length should be constant expression -> {ex.Message}", LengthSpan);
				Variable.Length = 2;
			}
		}
		context.DeclareVariable(this, Variable);
	}

	public override void CreateTokens(CompilerContext context)
	{
		context.AddText("'DECLARE {0}{1} AS {2}", Variable.Name, Variable.IsArray ? ("[" + Variable.Length + "]") : "", Variable.Type);
	}
}
