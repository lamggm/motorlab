using System;
using System.Collections.Generic;

namespace Roboteq.MicroBasic.Compiler.Common;

public class ConstantsList
{
	private Dictionary<string, int> _list;

	public Dictionary<string, int> List
	{
		get
		{
			if (_list == null)
			{
				_list = new Dictionary<string, int>(StringComparer.CurrentCultureIgnoreCase);
			}
			return _list;
		}
		set
		{
			_list = value;
		}
	}

	public void Add(string key, int value)
	{
		List[key] = value;
	}

	public bool ContainsKey(string key)
	{
		return List.ContainsKey(key);
	}

	public bool ContainsValue(int value)
	{
		return List.ContainsValue(value);
	}

	public int GetValue(string key)
	{
		if (!List.ContainsKey(key))
		{
			return int.MinValue;
		}
		return List[key];
	}

	public string GetKey(int value)
	{
		foreach (string key in List.Keys)
		{
			if (List[key] == value)
			{
				return key;
			}
		}
		return value.ToString();
	}
}
