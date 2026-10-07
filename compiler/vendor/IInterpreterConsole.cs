namespace Roboteq.MicroBasic.Simulation;

public interface IInterpreterConsole
{
	void Print(string message);

	void SetConfig(int constant, int index, int value);

	int GetConfig(int constant, int index);

	void SetCommand(int constant, int index, int value);

	int GetValue(int constant, int index);
}
