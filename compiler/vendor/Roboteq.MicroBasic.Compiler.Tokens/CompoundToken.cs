using System.Collections.Generic;
using Roboteq.MicroBasic.Compiler.Common;
using Roboteq.MicroBasic.Compiler.CompilerUnits;

namespace Roboteq.MicroBasic.Compiler.Tokens;

public class CompoundToken : DeviceToken
{
	private List<DeviceToken> _subTokens;

	public List<DeviceToken> SubTokens
	{
		get
		{
			if (_subTokens == null)
			{
				_subTokens = new List<DeviceToken>();
			}
			return _subTokens;
		}
		set
		{
			_subTokens = value;
		}
	}

	public CompoundToken(CompilerUnit creator)
		: base(creator)
	{
	}

	public virtual void UpdateSubTokens(CompilerContext context)
	{
	}

	public override void SetOffset(CompilerContext context, int offset)
	{
		base.SetOffset(context, offset);
		foreach (DeviceToken subToken in SubTokens)
		{
			subToken.SetOffset(context, offset);
			offset += subToken.GetLength(context);
		}
	}

	public override byte[] ToByteArray(CompilerContext context)
	{
		List<byte> list = new List<byte>();
		foreach (DeviceToken subToken in SubTokens)
		{
			list.AddRange(subToken.ToByteArray(context));
		}
		return list.ToArray();
	}

	public override void AppendCode(CompilerContext context)
	{
		foreach (DeviceToken subToken in SubTokens)
		{
			subToken.AppendCode(context);
		}
	}
}
