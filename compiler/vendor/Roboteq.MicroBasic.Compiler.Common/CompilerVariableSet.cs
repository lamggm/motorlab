using System;
using System.Collections.Generic;

namespace Roboteq.MicroBasic.Compiler.Common;

public class CompilerVariableSet
{
	private Dictionary<string, CompilerVariable> _variables;

	public Dictionary<string, CompilerVariable> Variables
	{
		get
		{
			if (_variables == null)
			{
				_variables = new Dictionary<string, CompilerVariable>(StringComparer.CurrentCultureIgnoreCase);
			}
			return _variables;
		}
		set
		{
			_variables = value;
		}
	}

	public bool Contains(string name)
	{
		return Variables.ContainsKey(name);
	}

	public CompilerVariable GetVariable(string name)
	{
		if (!Variables.ContainsKey(name))
		{
			return null;
		}
		return Variables[name];
	}

	public void Add(CompilerVariable variable)
	{
		if (!Variables.ContainsKey(variable.Name))
		{
			Variables[variable.Name] = variable;
		}
	}

	public void Add(string name)
	{
		Add(name, CompilerVariableType.Integer);
	}

	public void Add(string name, CompilerVariableType type)
	{
		Add(name, type, 1);
	}

	public void Add(string name, CompilerVariableType type, int length)
	{
		Add(new CompilerVariable(name, type, length));
	}
}
