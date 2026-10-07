using Irony.Parsing;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.Tokens;

public class GotoToken : CompoundToken
{
	private string _labelName;

	private ConstToken _addressToken;

	private SourceSpan _labelSpan;

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

	private ConstToken AddressToken
	{
		get
		{
			return _addressToken;
		}
		set
		{
			_addressToken = value;
		}
	}

	public int LabelAddress
	{
		get
		{
			return AddressToken.Value;
		}
		set
		{
			AddressToken.Value = value;
		}
	}

	public SourceSpan LabelSpan
	{
		get
		{
			return _labelSpan;
		}
		set
		{
			_labelSpan = value;
		}
	}

	public GotoToken(SourceSpan labelSpan, string labelName)
		: this(labelSpan, labelName, DeviceCommand.JUMP)
	{
	}

	public GotoToken(SourceSpan labelSpan, string labelName, DeviceCommand jumpCommand)
		: base(null)
	{
		_labelSpan = labelSpan;
		_labelName = labelName;
		_addressToken = new ConstToken(0, force16: true);
		base.Command = jumpCommand;
		base.SubTokens.Add(AddressToken);
		base.SubTokens.Add(new DeviceToken(jumpCommand));
	}
}
