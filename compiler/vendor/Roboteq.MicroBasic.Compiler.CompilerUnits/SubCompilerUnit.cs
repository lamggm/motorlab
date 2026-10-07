using System.Collections.Generic;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class SubCompilerUnit : CompilerUnit
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
		case "wait":
			if (count == 1)
			{
				deviceCommand = DeviceCommand.WAITTIME;
			}
			break;
		case "setconfig":
			switch (count)
			{
			case 3:
				deviceCommand = DeviceCommand.SETCONF;
				break;
			case 2:
			{
				deviceCommand = DeviceCommand.SETCONF;
				NumberCompilerUnit numberCompilerUnit2 = new NumberCompilerUnit();
				numberCompilerUnit2.Value = 0;
				ParamList.Insert(1, numberCompilerUnit2);
				break;
			}
			}
			break;
		case "setcommand":
			switch (count)
			{
			case 3:
				deviceCommand = DeviceCommand.SETCMD;
				break;
			case 2:
			{
				deviceCommand = DeviceCommand.SETCMD;
				NumberCompilerUnit numberCompilerUnit = new NumberCompilerUnit();
				numberCompilerUnit.Value = 0;
				ParamList.Insert(1, numberCompilerUnit);
				break;
			}
			}
			break;
		case "settimercount":
			if (count == 2)
			{
				deviceCommand = DeviceCommand.SETTIMCNT;
			}
			break;
		case "settimerstate":
			if (count == 2)
			{
				deviceCommand = DeviceCommand.SETTIMSTOP;
			}
			break;
		case "setcancommand":
			if (count == 4)
			{
				deviceCommand = DeviceCommand.SetCANCommand;
			}
			break;
		case "setcanconfig":
			if (count == 4)
			{
				deviceCommand = DeviceCommand.SetCANConfig;
			}
			break;
		case "fetchcanconfig":
			if (count == 3)
			{
				deviceCommand = DeviceCommand.FetchCANConfig;
			}
			break;
		case "fetchcanvalue":
			if (count == 3)
			{
				deviceCommand = DeviceCommand.FetchCANValue;
			}
			break;
		case "scancanvalue":
			if (count == 5)
			{
				deviceCommand = DeviceCommand.ScanCANValue;
			}
			break;
		case "setvpm":
			if (count == 4)
			{
				deviceCommand = DeviceCommand.SETVPM;
			}
			break;
		}
		if (deviceCommand == DeviceCommand.None)
		{
			context.AddError($"No sub routine named '{FunctionName}' that takes '{ParamList.Count}' arguments", NameSpan);
			return;
		}
		foreach (CompilerUnit param in ParamList)
		{
			param.CreateTokens(context);
		}
		context.AddToken(deviceCommand);
	}
}
