namespace Roboteq.MicroBasic.Compiler.CompilerUnits;

public interface ILoop
{
	string LoopName { get; }

	string ContinueLabel { get; }

	string ExitLabel { get; }
}
