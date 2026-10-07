using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class IncCompilerUnit : IncCallCompilerUnit
{
	public override void CreateTokens(CompilerContext context)
	{
		base.Variable.CreateTokens(context, CompilerVariableUsage.GetValue);
		if (base.IsPostfix)
		{
			base.Variable.CreateTokens(context, CompilerVariableUsage.GetValue);
		}
		context.AddToken(base.Command);
		base.Variable.CreateTokens(context, CompilerVariableUsage.SetValue);
		if (!base.IsPostfix)
		{
			base.Variable.CreateTokens(context, CompilerVariableUsage.GetValue);
		}
	}
}
