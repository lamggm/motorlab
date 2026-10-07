using System;
using System.Collections.Generic;
using System.IO;
using System.Xml.Serialization;

namespace Roboteq.MicroBasic.Compiler.Common;

public class CompilerConstantsList
{
	[Serializable]
	public class SerializableCompilerConstsList
	{
		public string Name { get; set; }

		public List<CompilerConstsListItem> ConfigList { get; set; }

		public List<CompilerConstsListItem> CommandsList { get; set; }

		public List<CompilerConstsListItem> ValuesList { get; set; }
	}

	public class CompilerConstsListItem
	{
		public string Name { get; set; }

		public int Value { get; set; }

		public CompilerConstsListItem(string name, int value)
		{
			Name = name;
			Value = value;
		}

		public CompilerConstsListItem()
		{
		}
	}

	private string _name;

	private ConstantsList _configList;

	private ConstantsList _commandsList;

	private ConstantsList _valuesList;

	public string Name
	{
		get
		{
			return _name;
		}
		set
		{
			_name = value;
		}
	}

	public ConstantsList ConfigList
	{
		get
		{
			if (_configList == null)
			{
				_configList = new ConstantsList();
			}
			return _configList;
		}
		set
		{
			_configList = value;
		}
	}

	public ConstantsList CommandsList
	{
		get
		{
			if (_commandsList == null)
			{
				_commandsList = new ConstantsList();
			}
			return _commandsList;
		}
		set
		{
			_commandsList = value;
		}
	}

	public ConstantsList ValuesList
	{
		get
		{
			if (_valuesList == null)
			{
				_valuesList = new ConstantsList();
			}
			return _valuesList;
		}
		set
		{
			_valuesList = value;
		}
	}

	public CompilerConstantsList(string name)
	{
		_name = name;
	}

	public bool ContainsKey(string key)
	{
		if (ConfigList.ContainsKey(key))
		{
			return true;
		}
		if (CommandsList.ContainsKey(key))
		{
			return true;
		}
		if (ValuesList.ContainsKey(key))
		{
			return true;
		}
		return false;
	}

	public int GetValue(string key)
	{
		if (ConfigList.ContainsKey(key))
		{
			return ConfigList.GetValue(key);
		}
		if (CommandsList.ContainsKey(key))
		{
			return CommandsList.GetValue(key);
		}
		if (ValuesList.ContainsKey(key))
		{
			return ValuesList.GetValue(key);
		}
		return int.MinValue;
	}

	public List<string> GetAllConstants()
	{
		List<string> list = new List<string>();
		foreach (string key in ConfigList.List.Keys)
		{
			list.Add(key);
		}
		foreach (string key2 in CommandsList.List.Keys)
		{
			list.Add(key2);
		}
		foreach (string key3 in ValuesList.List.Keys)
		{
			list.Add(key3);
		}
		return list;
	}

	public static CompilerConstantsList FromXmlString(string text)
	{
		//IL_0027: Unknown result type (might be due to invalid IL or missing references)
		//IL_002d: Expected O, but got Unknown
		if (string.IsNullOrWhiteSpace(text))
		{
			return new CompilerConstantsList("");
		}
		try
		{
			XmlSerializer val = new XmlSerializer(typeof(SerializableCompilerConstsList));
			using StringReader stringReader = new StringReader(text);
			SerializableCompilerConstsList serializableCompilerConstsList = val.Deserialize((TextReader)stringReader) as SerializableCompilerConstsList;
			CompilerConstantsList compilerConstantsList = new CompilerConstantsList(serializableCompilerConstsList.Name);
			foreach (CompilerConstsListItem item in serializableCompilerConstsList?.ConfigList)
			{
				compilerConstantsList.ConfigList.Add(item.Name, item.Value);
			}
			foreach (CompilerConstsListItem item2 in serializableCompilerConstsList?.CommandsList)
			{
				compilerConstantsList.CommandsList.Add(item2.Name, item2.Value);
			}
			foreach (CompilerConstsListItem item3 in serializableCompilerConstsList?.ValuesList)
			{
				compilerConstantsList.ValuesList.Add(item3.Name, item3.Value);
			}
			return compilerConstantsList;
		}
		catch
		{
			return new CompilerConstantsList("");
		}
	}

	public string ToXmlString()
	{
		//IL_0147: Unknown result type (might be due to invalid IL or missing references)
		//IL_014d: Expected O, but got Unknown
		SerializableCompilerConstsList serializableCompilerConstsList = new SerializableCompilerConstsList
		{
			Name = Name,
			ConfigList = new List<CompilerConstsListItem>(),
			CommandsList = new List<CompilerConstsListItem>(),
			ValuesList = new List<CompilerConstsListItem>()
		};
		foreach (KeyValuePair<string, int> item in ConfigList.List)
		{
			serializableCompilerConstsList.ConfigList.Add(new CompilerConstsListItem(item.Key, item.Value));
		}
		foreach (KeyValuePair<string, int> item2 in CommandsList.List)
		{
			serializableCompilerConstsList.CommandsList.Add(new CompilerConstsListItem(item2.Key, item2.Value));
		}
		foreach (KeyValuePair<string, int> item3 in ValuesList.List)
		{
			serializableCompilerConstsList.ValuesList.Add(new CompilerConstsListItem(item3.Key, item3.Value));
		}
		XmlSerializer val = new XmlSerializer(serializableCompilerConstsList.GetType());
		using StringWriter stringWriter = new StringWriter();
		val.Serialize((TextWriter)stringWriter, (object)serializableCompilerConstsList);
		return stringWriter.ToString();
	}
}
