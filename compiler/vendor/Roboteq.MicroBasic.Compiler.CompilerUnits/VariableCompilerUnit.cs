using System;
using System.Linq;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class VariableCompilerUnit : CompilerUnit
{
	private CompilerVariable _variable;

	private CompilerUnit _indexUnit;

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

	public CompilerUnit IndexUnit
	{
		get
		{
			return _indexUnit;
		}
		set
		{
			_indexUnit = value;
		}
	}

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		_variable = new CompilerVariable();
		_variable.Name = treeNode.ChildNodes[0].Token.ValueString;
		if (treeNode.ChildNodes.Count > 1)
		{
			_indexUnit = treeNode.ChildNodes[1].AstNode as CompilerUnit;
			_variable.IsArray = true;
			_variable.Length = 0;
		}
	}

	public override void InitializeUnit(CompilerContext context)
	{
		InitializeUnit(context, CompilerVariableUsage.GetValue);
	}

	public void InitializeUnit(CompilerContext context, CompilerVariableUsage usage)
	{
		if (!Variable.Name.StartsWith("_"))
		{
			DontCreateTokens = !context.DeclareUseVariable(this, Variable, usage);
		}
		else if (usage == CompilerVariableUsage.GetValue)
		{
			if (Variable.IsArray)
			{
				context.AddError($"Use of undeclared identifier '{Variable.Name}', arrays should be declared before use", Span);
				DontCreateTokens = true;
			}
			else if (!CompilerHelper.IsNamedConstant(Variable.Name) && !context.Directives.ContainsKey(Variable.Name))
			{
				context.AddError($"'{Variable.Name}' is not a valid named constant", Span);
				DontCreateTokens = true;
			}
		}
		else
		{
			context.AddError($"'{Variable.Name}' is a named constant and could not be assigned a value", Span);
			DontCreateTokens = true;
		}
		if (IndexUnit != null)
		{
			IndexUnit.InitializeUnit(context);
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		CreateTokens(context, CompilerVariableUsage.GetValue);
	}

	public void CreateTokens(CompilerContext context, CompilerVariableUsage usage)
	{
		if (DontCreateTokens || usage == CompilerVariableUsage.Declaration)
		{
			return;
		}
		if (context.Directives.ContainsKey(Variable.Name))
		{
			context.AddComment("Pre-processor directive: {0}", Variable.Name);
			CompilerUnit expression = context.Directives[Variable.Name].Expression;
			if (context.HashDefineStack.Contains(Variable.Name))
			{
				if (!context.RecursiveDefines.Contains(Variable.Name))
				{
					context.RecursiveDefines.Add(Variable.Name);
					context.AddError($"Recursive #define occurred at '{Variable.Name}'.", context.Directives[Variable.Name].IdentifierSpan);
				}
			}
			else
			{
				context.HashDefineStack.Add(Variable.Name);
				expression.CreateTokens(context);
				context.HashDefineStack.Remove(Variable.Name);
			}
			return;
		}
		if (CompilerHelper.IsNamedConstant(Variable.Name))
		{
			int? num = null;
			string text = (Parent?.Parent as FunctionCompilerUnit)?.FunctionName ?? (Parent?.Parent as SubCompilerUnit)?.FunctionName;
			if (text != null && Parent.ChildNodes.FirstOrDefault() == this)
			{
				switch (text.ToLower())
				{
				case "setcommand":
					num = CompilerHelper.GetCommandConstant(Variable.Name);
					break;
				case "getvalue":
					num = CompilerHelper.GetQueryConstant(Variable.Name);
					break;
				case "getconfig":
				case "setconfig":
					num = CompilerHelper.GetConfigConstant(Variable.Name);
					break;
				}
			}
			if (!num.HasValue)
			{
				num = CompilerHelper.GetConstantValue(Variable.Name);
			}
			context.AddComment("Named constant: {0}={1}", Variable.Name, num);
			context.AddToken(new ConstToken(num.Value));
			return;
		}
		int num2 = 0;
		try
		{
			num2 = context.GetVariableAddress(Variable.Name);
		}
		catch (Exception)
		{
			context.AddError($"Invalid constant/variable name '{Variable.Name}'.", Span);
			return;
		}
		DeviceCommand deviceCommand = DeviceCommand.None;
		deviceCommand = ((context.GetVariableType(Variable.Name) != CompilerVariableType.Boolean) ? ((usage == CompilerVariableUsage.GetValue) ? DeviceCommand.ILOAD : DeviceCommand.ISTORE) : ((usage == CompilerVariableUsage.GetValue) ? DeviceCommand.BLOAD : DeviceCommand.BSTORE));
		if (!Variable.IsArray)
		{
			context.AddComment("Push variable '{0}' address into stack.", Variable.Name);
			context.AddToken(new ConstToken(num2));
		}
		else
		{
			context.AddComment("Compute address of variable '{0}'.", Variable.Name);
			context.AddToken(new ConstToken(num2));
			if (IndexUnit != null)
			{
				IndexUnit.CreateTokens(context);
			}
			context.AddToken(DeviceCommand.PLUS);
		}
		context.AddToken(deviceCommand);
	}

	public override int ComputeValue(CompilerContext context)
	{
		if (CompilerHelper.IsNamedConstant(Variable.Name))
		{
			context.AddComment("Named constant: {0}", Variable.Name);
			return CompilerHelper.GetConstantValue(Variable.Name);
		}
		if (context.Directives.ContainsKey(Variable.Name))
		{
			if (context.HashDefineStack.Contains(Variable.Name))
			{
				if (context.RecursiveDefines.Contains(Variable.Name))
				{
					return 1;
				}
				context.RecursiveDefines.Add(Variable.Name);
				context.AddError($"Recursive #define occurred at '{Variable.Name}'.", context.Directives[Variable.Name].IdentifierSpan);
				return 1;
			}
			context.HashDefineStack.Add(Variable.Name);
			CompilerUnit expression = context.Directives[Variable.Name].Expression;
			int result = expression.ComputeValue(context);
			context.HashDefineStack.Remove(Variable.Name);
			return result;
		}
		return base.ComputeValue(context);
	}
}
