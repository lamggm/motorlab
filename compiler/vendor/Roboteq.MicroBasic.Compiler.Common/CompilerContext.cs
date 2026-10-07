using System;
using System.Collections.Generic;
using System.Text;
using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.CompilerUnits;
using Roboteq.MicroBasic.Compiler.Tokens;

namespace Roboteq.MicroBasic.Compiler.Common;

public class CompilerContext
{
	private bool _isStrictDeclaration;

	private ParseTree _parseTree;

	private StringBuilder _deviceCode;

	private List<CompilerError> _errors;

	private List<CompilerError> _warninigs;

	private List<DeviceToken> _tokens;

	private CompilerVariableSet varDeclareSet = new CompilerVariableSet();

	private CompilerVariableSet varUseSet = new CompilerVariableSet();

	private int iVarLastIndex = 0;

	private int bVarLastIndex = 0;

	private Dictionary<string, int> iVarIndexes = new Dictionary<string, int>(StringComparer.CurrentCultureIgnoreCase);

	private Dictionary<string, int> bVarIndexes = new Dictionary<string, int>(StringComparer.CurrentCultureIgnoreCase);

	private int iVarLastCreatedIndex = 0;

	private int lastLabelIndex = 0;

	private Dictionary<string, LabelToken> labels = new Dictionary<string, LabelToken>(StringComparer.CurrentCultureIgnoreCase);

	public bool IsStrictDeclaration
	{
		get
		{
			return _isStrictDeclaration;
		}
		set
		{
			_isStrictDeclaration = value;
		}
	}

	public ParseTree ParseTree
	{
		get
		{
			return _parseTree;
		}
		set
		{
			_parseTree = value;
		}
	}

	public StringBuilder DeviceCode
	{
		get
		{
			if (_deviceCode == null)
			{
				_deviceCode = new StringBuilder();
			}
			return _deviceCode;
		}
		set
		{
			_deviceCode = value;
		}
	}

	public List<CompilerError> Errors
	{
		get
		{
			if (_errors == null)
			{
				_errors = new List<CompilerError>();
			}
			return _errors;
		}
		set
		{
			_errors = value;
		}
	}

	public List<CompilerError> Warnings
	{
		get
		{
			if (_warninigs == null)
			{
				_warninigs = new List<CompilerError>();
			}
			return _warninigs;
		}
		set
		{
			_warninigs = value;
		}
	}

	public List<DeviceToken> Tokens
	{
		get
		{
			if (_tokens == null)
			{
				_tokens = new List<DeviceToken>();
			}
			return _tokens;
		}
		set
		{
			_tokens = value;
		}
	}

	public Dictionary<string, CompilerVariable> VariableList { get; private set; }

	public Dictionary<string, DirectiveCompilerUnit> Directives { get; private set; }

	public HashSet<string> HashDefineStack { get; private set; }

	public HashSet<string> RecursiveDefines { get; private set; }

	public CompilerContext()
	{
		VariableList = new Dictionary<string, CompilerVariable>(StringComparer.CurrentCultureIgnoreCase);
		Directives = new Dictionary<string, DirectiveCompilerUnit>(StringComparer.CurrentCultureIgnoreCase);
		HashDefineStack = new HashSet<string>(StringComparer.CurrentCultureIgnoreCase);
		RecursiveDefines = new HashSet<string>(StringComparer.CurrentCultureIgnoreCase);
	}

	public void AddText(string text, params object[] args)
	{
		AddToken(new NullToken(string.Format(text, args)));
	}

	public void AddComment(string text, params object[] args)
	{
		AddText("      @" + text, args);
	}

	private void AssignLabelTokens()
	{
		foreach (DeviceToken token in Tokens)
		{
			if (token is LabelToken labelToken && labels[labelToken.LabelName] == null)
			{
				labels[labelToken.LabelName] = labelToken;
			}
		}
	}

	private void UpdateTokensAddress()
	{
		int num = 0;
		foreach (DeviceToken token in Tokens)
		{
			token.SetOffset(this, num);
			num += token.GetLength(this);
		}
	}

	private void UpdateGotoAddress()
	{
		foreach (DeviceToken token in Tokens)
		{
			if (token is GotoToken gotoToken)
			{
				if (!labels.ContainsKey(gotoToken.LabelName) || labels[gotoToken.LabelName] == null)
				{
					AddError($"No such a label with the name '{gotoToken.LabelName}'", gotoToken.LabelSpan);
				}
				else
				{
					gotoToken.LabelAddress = labels[gotoToken.LabelName].Offset;
				}
			}
		}
	}

	public byte[] GetBinaryCode()
	{
		List<byte> list = new List<byte>();
		foreach (DeviceToken token in Tokens)
		{
			list.AddRange(token.ToByteArray(this));
		}
		return list.ToArray();
	}

	public void GenerateCode()
	{
		DeviceCode = new StringBuilder();
		AssignLabelTokens();
		UpdateTokensAddress();
		UpdateGotoAddress();
		foreach (DeviceToken token in Tokens)
		{
			token.AppendCode(this);
		}
		int num = 15;
		int num2 = 0;
		byte[] binaryCode = GetBinaryCode();
		DeviceCode.AppendLine();
		DeviceCode.AppendLine("'------------------------------------------------------------------------");
		DeviceCode.Append("{");
		byte[] array = binaryCode;
		foreach (byte b in array)
		{
			if (num2 % num == 0)
			{
				DeviceCode.AppendLine();
			}
			StringBuilder deviceCode = DeviceCode;
			int num3 = b;
			deviceCode.Append(num3.ToString().PadLeft(3) + ", ");
			num2++;
		}
		uint num4 = CRC32.Compute(binaryCode);
		DeviceCode.AppendLine();
		DeviceCode.AppendLine("}");
		DeviceCode.AppendLine("'------------------------------------------------------------------------");
		DeviceCode.AppendLine($"@ Script checksum is 0x{num4:X} => {(int)num4}.");
		DeviceCode.AppendLine($"@ Total Program Length is {binaryCode.LongLength} byte(s).");
		DeviceCode.AppendLine("'------------------------------------------------------------------------");
	}

	public void AddToken(DeviceCommand command)
	{
		AddToken(new DeviceToken(command));
	}

	public void AddToken(DeviceToken token)
	{
		Tokens.Add(token);
	}

	public bool AddDirective(DirectiveCompilerUnit directive)
	{
		if (CompilerHelper.IsReservedWord(directive.Identifier))
		{
			AddError($"'{directive.Identifier}' is a reserved word and cannot be used as a pre-processor directive.", directive.IdentifierSpan);
			return false;
		}
		if (Directives.ContainsKey(directive.Identifier))
		{
			AddError($"'{directive.Identifier}' is already declared as pre-processor directive.", directive.IdentifierSpan);
			return false;
		}
		if (CompilerHelper.IsNamedConstant(directive.Identifier))
		{
			AddWarning($"Redifinition of a named constant '{directive.Identifier}'.", directive.IdentifierSpan);
		}
		Directives.Add(directive.Identifier, directive);
		return true;
	}

	public int GetVariableAddress(string name)
	{
		if (iVarIndexes.ContainsKey(name))
		{
			return iVarIndexes[name];
		}
		return bVarIndexes[name];
	}

	public CompilerVariableType GetVariableType(string name)
	{
		if (bVarIndexes.ContainsKey(name))
		{
			return CompilerVariableType.Boolean;
		}
		return CompilerVariableType.Integer;
	}

	public void AddVarToIndex(CompilerVariable var)
	{
		if (var.Type == CompilerVariableType.Integer && !iVarIndexes.ContainsKey(var.Name))
		{
			var.Address = iVarLastIndex;
			VariableList.Add(var.Name, var);
			iVarIndexes.Add(var.Name, iVarLastIndex);
			iVarLastIndex += var.Length;
		}
		else if (var.Type == CompilerVariableType.Boolean && !bVarIndexes.ContainsKey(var.Name))
		{
			var.Address = bVarLastIndex;
			VariableList.Add(var.Name, var);
			bVarIndexes.Add(var.Name, bVarLastIndex);
			bVarLastIndex += var.Length;
		}
	}

	public bool DeclareVariable(DeclareCompilerUnit creator, CompilerVariable var)
	{
		if (CompilerHelper.IsReservedWord(var.Name))
		{
			AddError($"'{var.Name}' is a reserved word and cannot be used as variable name", creator.NameSpan);
			return false;
		}
		if (Directives.ContainsKey(var.Name))
		{
			AddError($"'{var.Name}' is already declared as pre-processor directive.", creator.NameSpan);
			return false;
		}
		if (varDeclareSet.Contains(var.Name))
		{
			AddError($"A variable named '{var.Name}' already defined in the script", creator.NameSpan);
			return false;
		}
		if (varUseSet.Contains(var.Name))
		{
			AddError($"A variable named '{var.Name}' used before declaration", creator.NameSpan);
			return false;
		}
		if (var.IsArray && var.Length < 2)
		{
			AddError($"Array variables length should be greater than 1", creator.LengthSpan);
		}
		AddVarToIndex(var);
		varDeclareSet.Add(var);
		return true;
	}

	public bool DeclareUseVariable(VariableCompilerUnit creator, CompilerVariable var, CompilerVariableUsage usage)
	{
		if (CompilerHelper.IsReservedWord(var.Name))
		{
			AddError($"'{var.Name}' is a reserved word and cannot be used as variable name", creator.Span);
			return false;
		}
		if (Directives.ContainsKey(var.Name))
		{
			if (usage == CompilerVariableUsage.Declaration || var.IsArray)
			{
				AddError($"'{var.Name}' is already declared as pre-processor directive.", creator.Span);
				return false;
			}
			if (usage == CompilerVariableUsage.SetValue)
			{
				AddError($"Can't change value of pre-processor directive '{var.Name}'.", creator.Span);
				return false;
			}
			return true;
		}
		CompilerVariable variable = varDeclareSet.GetVariable(var.Name);
		if (variable == null && (IsStrictDeclaration || var.IsArray))
		{
			if (var.IsArray)
			{
				AddError($"Use of undeclared identifier '{var.Name}', arrays should be declared before use", creator.Span);
			}
			else
			{
				AddError($"Use of undeclared identifier '{var.Name}'", creator.Span);
			}
			return false;
		}
		if (var.IsArray)
		{
			if (!variable.IsArray)
			{
				AddError($"Attempt to use normal variable '{var.Name}' like an array variable", creator.Span);
				return false;
			}
			var.Type = variable.Type;
			return true;
		}
		if (variable != null)
		{
			if (variable.IsArray)
			{
				AddError($"Attempt to use array variable '{var.Name}' like a normal variable", creator.Span);
				return false;
			}
			var.Type = variable.Type;
			return true;
		}
		AddVarToIndex(var);
		varUseSet.Add(var);
		return true;
	}

	public string CreateVariable()
	{
		iVarLastCreatedIndex++;
		CompilerVariable compilerVariable = new CompilerVariable("Variable#" + iVarLastCreatedIndex, CompilerVariableType.Integer);
		compilerVariable.IsArray = false;
		AddVarToIndex(compilerVariable);
		varDeclareSet.Add(compilerVariable);
		return compilerVariable.Name;
	}

	public MicrobasicScriptMemoryUsage GetMemoryUsage()
	{
		return new MicrobasicScriptMemoryUsage
		{
			NumIntegerVariables = iVarLastIndex,
			NumBooleanVariables = bVarLastIndex
		};
	}

	public bool AddLabel(CompilerUnit creator, string name)
	{
		if (CompilerHelper.IsReservedWord(name))
		{
			AddError($"'{name}' is a reserved word and cannot be used as label names", creator.Span);
			return false;
		}
		if (labels.ContainsKey(name))
		{
			AddError($"The label '{name}' already occurs in the script", creator.Span);
			return false;
		}
		labels.Add(name, null);
		return true;
	}

	public string CreateLabel(CompilerUnit creator)
	{
		string text = "Label#" + lastLabelIndex;
		AddLabel(creator, text);
		lastLabelIndex++;
		return text;
	}

	public void AddError(string message, SourceSpan span)
	{
		Errors.Add(new CompilerError(message, span, CompilerErrorLevel.Error));
	}

	public void AddWarning(string message, SourceSpan span)
	{
		Warnings.Add(new CompilerError(message, span, CompilerErrorLevel.Warning));
	}
}
