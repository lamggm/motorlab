using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class ScriptCompilerUnit : CompilerUnit
{
	public override void CreateTokens(CompilerContext context)
	{
		base.CreateTokens(context);
		context.AddToken(DeviceCommand.ENDPROG);
	}
}
