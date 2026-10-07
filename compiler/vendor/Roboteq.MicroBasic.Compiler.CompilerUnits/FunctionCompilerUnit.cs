using System.Collections.Generic;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class FunctionCompilerUnit : CompilerUnit
{
	private string _functionName;

	private List<CompilerUnit> _paramList;

	private bool _isCalledAsSub;

	private SourceSpan _nameSpan;

	public string FunctionName
	{
		get
		{
			return _functionName;
		}
		set
		{
			_functionName = value;
		}
	}

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

	public bool IsCalledAsSub
	{
		get
		{
			return _isCalledAsSub;
		}
		set
		{
			_isCalledAsSub = value;
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

	public override void Init(ParsingContext context, ParseTreeNode treeNode)
	{
		base.Init(context, treeNode);
		FunctionName = treeNode.ChildNodes[0].Token.ValueString;
		NameSpan = treeNode.ChildNodes[0].Span;
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
		DeviceCommand deviceCommand = DeviceCommand.None;
		string text = FunctionName.ToLower();
		int count = ParamList.Count;
		switch (text)
		{
		case "abs":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.ABS;
			}
			break;
		case "getconfig":
			switch (count)
			{
			case 2:
				deviceCommand = DeviceCommand.GETCONF;
				break;
			case 1:
			{
				deviceCommand = DeviceCommand.GETCONF;
				NumberCompilerUnit numberCompilerUnit = new NumberCompilerUnit();
				numberCompilerUnit.Value = 0;
				ParamList.Add(numberCompilerUnit);
				break;
			}
			}
			break;
		case "getvalue":
			switch (count)
			{
			case 2:
				deviceCommand = DeviceCommand.GETVAL;
				break;
			case 1:
			{
				deviceCommand = DeviceCommand.GETVAL;
				NumberCompilerUnit numberCompilerUnit2 = new NumberCompilerUnit();
				numberCompilerUnit2.Value = 0;
				ParamList.Add(numberCompilerUnit2);
				break;
			}
			}
			break;
		case "gettimercount":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.GETTIMCNT;
			}
			break;
		case "gettimerstate":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.GETTIMSTOP;
			}
			break;
		case "tobool":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.BOOL;
			}
			break;
		case "iscannodealive":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.IsCANNodeAlive;
			}
			break;
		case "iscanconfigready":
			if (count == 0)
			{
				deviceCommand = DeviceCommand.IsCANConfigReady;
			}
			break;
		case "readcanconfig":
			if (count == 0)
			{
				deviceCommand = DeviceCommand.ReadCANConfig;
			}
			break;
		case "iscanvalueready":
			if (count == 0)
			{
				deviceCommand = DeviceCommand.IsCANValueReady;
			}
			break;
		case "readcanvalue":
			if (count == 0)
			{
				deviceCommand = DeviceCommand.ReadCANValue;
			}
			break;
		case "isscannedcanready":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.IsScannedCANReady;
			}
			break;
		case "readscannedcanvalue":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.ReadScannedCANValue;
			}
			break;
		case "sqrt":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.SQRT;
			}
			break;
		case "sin":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.SIN;
			}
			break;
		case "cos":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.COS;
			}
			break;
		case "atan":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.ATAN;
			}
			break;
		}
		if (deviceCommand == DeviceCommand.None)
		{
			context.AddError($"No function named '{FunctionName}' that takes '{ParamList.Count}' arguments", NameSpan);
			return;
		}
		foreach (CompilerUnit param in ParamList)
		{
			param.CreateTokens(context);
		}
		context.AddToken(deviceCommand);
	}
}
