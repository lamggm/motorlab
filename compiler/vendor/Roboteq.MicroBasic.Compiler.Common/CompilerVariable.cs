namespace Roboteq.MicroBasic.Compiler.Common;

public class CompilerVariable
{
	private string _name;

	private int _length;

	private CompilerVariableType _type;

	private bool _isArray;

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

	public int Length
	{
		get
		{
			return _length;
		}
		set
		{
			_length = value;
		}
	}

	public CompilerVariableType Type
	{
		get
		{
			return _type;
		}
		set
		{
			_type = value;
		}
	}

	public bool IsArray
	{
		get
		{
			return _isArray;
		}
		set
		{
			_isArray = value;
		}
	}

	public int Address { get; set; }

	public CompilerVariable()
		: this(string.Empty)
	{
	}

	public CompilerVariable(string name)
		: this(name, CompilerVariableType.Integer)
	{
	}

	public CompilerVariable(string name, CompilerVariableType type)
		: this(name, type, 1)
	{
	}

	public CompilerVariable(string name, CompilerVariableType type, int length)
	{
		_name = name;
		_type = type;
		_length = length;
		Address = 0;
	}
}
