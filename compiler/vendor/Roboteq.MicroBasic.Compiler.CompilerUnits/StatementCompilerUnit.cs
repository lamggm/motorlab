using Irony.Ast;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public class StatementCompilerUnit : CompilerUnit
{
	public override void CreateTokens(CompilerContext context)
	{
		foreach (AstNode childNode in ChildNodes)
		{
			if (childNode is CompilerUnit)
			{
				(childNode as CompilerUnit).CreateTokens(context);
				context.AddText("'=====================================================");
			}
		}
	}
}
