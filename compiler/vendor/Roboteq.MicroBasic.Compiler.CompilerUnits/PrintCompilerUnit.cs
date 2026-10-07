using System.Collections.Generic;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class PrintCompilerUnit : CompilerUnit
{
	private const int MaxStringLength = 32;

	private List<CompilerUnit> _paramList;

	public List<CompilerUnit> ParamList
	{
		get
		{
			return _paramList;
		}
		set
		{
			_paramList = value;
		}
	}

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		_paramList = new List<CompilerUnit>();
		foreach (ParseTreeNode childNode in treeNode.ChildNodes[1].ChildNodes)
		{
			if (childNode.AstNode is CompilerUnit)
			{
				_paramList.Add(childNode.AstNode as CompilerUnit);
			}
		}
	}

	public override void CreateTokens(CompilerContext context)
	{
		if (ParamList.Count == 0)
		{
			context.AddError("Print statement should have at least one parameter.", Span);
		}
		foreach (CompilerUnit param in ParamList)
		{
			if (param is StringCompilerUnit)
			{
				StringCompilerUnit stringCompilerUnit = param as StringCompilerUnit;
				if (string.IsNullOrEmpty(stringCompilerUnit.Value))
				{
					continue;
				}
				for (int i = 0; i < (stringCompilerUnit.Value.Length + 32 - 1) / 32; i++)
				{
					if (i != 0)
					{
						context.AddToken(new ConstToken(1));
						context.AddToken(DeviceCommand.WAITTIME);
					}
					int num = i * 32;
					int num2 = 32;
					if (num + num2 > stringCompilerUnit.Value.Length)
					{
						num2 = stringCompilerUnit.Value.Length - num;
					}
					context.AddToken(new PrintStringToken(stringCompilerUnit.Value.Substring(num, num2)));
				}
			}
			else if (param is ToBoolCompilerUnit)
			{
				ToBoolCompilerUnit toBoolCompilerUnit = param as ToBoolCompilerUnit;
				toBoolCompilerUnit.CreateTokens(context);
				context.AddToken(DeviceCommand.PRINTBOOL);
			}
			else if (param is VariableCompilerUnit && (param as VariableCompilerUnit).Variable.Type == CompilerVariableType.Boolean)
			{
				param.CreateTokens(context);
				context.AddToken(DeviceCommand.PRINTBOOL);
			}
			else
			{
				param.CreateTokens(context);
				context.AddToken(DeviceCommand.PRINTVAL);
			}
		}
	}
}
