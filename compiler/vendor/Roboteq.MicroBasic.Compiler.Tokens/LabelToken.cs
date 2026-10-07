namespace Roboteq.MicroBasic.Compiler.Tokens;

public class LabelToken : DeviceToken
{
	private string _labelName;

	public string LabelName
	{
		get
		{
			return _labelName;
		}
		set
		{
			_labelName = value;
		}
	}

	public LabelToken(string labelName)
	{
		_labelName = labelName;
	}
}
