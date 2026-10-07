using System;
using System.Reflection;

namespace Roboteq.MicroBasic.Compiler.Common;

public class DeviceCommandAttribute : Attribute
{
	private string _name;

	private string _hint;

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

	public string Hint
	{
		get
		{
			return _hint;
		}
		set
		{
			_hint = value;
		}
	}

	public static DeviceCommandAttribute GetAttribute(Enum value)
	{
		FieldInfo field = value.GetType().GetField(value.ToString());
		DeviceCommandAttribute[] array = (DeviceCommandAttribute[])field.GetCustomAttributes(typeof(DeviceCommandAttribute), inherit: false);
		return (array.Length != 0) ? array[0] : new DeviceCommandAttribute(value.ToString(), "");
	}

	public DeviceCommandAttribute(string commandName, string commandHint)
	{
		_name = commandName;
		_hint = commandHint;
	}
}
