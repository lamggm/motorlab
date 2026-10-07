using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using Roboteq.MicroBasic.Compiler.Common;

namespace Roboteq.MicroBasic.Simulation;

public class ByteCodeInterpreter
{
	protected const int NumIntVariables = 1024;

	protected const int NumBoolVariables = 1024;

	protected const int VarStackSize = 256;

	protected const int CallStackSize = 64;

	protected const int NumTimers = 4;

	public const long TicksPerMillisecond = 10L;

	protected const int FALSE = 0;

	protected const int TRUE = -1;

	public IInterpreterConsole Console { get; set; }

	public int Pointer { get; private set; }

	public long Ticks { get; private set; }

	public TimeSpan ElapsedTime => new TimeSpan(Ticks * 10000 / 10);

	public bool IsInDelay { get; private set; }

	private long DelayDueTicks { get; set; }

	public byte[] Code { get; private set; }

	private int[] IntVars { get; set; }

	private bool[] BoolVars { get; set; }

	private Stack<int> VarStack { get; set; }

	private Stack<int> CallStack { get; set; }

	private List<InterpreterDownTimer> Timers { get; set; }

	public ByteCodeInterpreter()
	{
		Setup();
	}

	public virtual bool ExecuteInstruction()
	{
		foreach (InterpreterDownTimer timer in Timers)
		{
			if (timer.IsCounting)
			{
				long num = Ticks - timer.StartTicks;
				int num2 = Convert.ToInt32(num / 10);
				timer.Count -= num2;
				if (timer.Count < 0)
				{
					timer.Count = 0;
				}
				if (timer.Count == 0)
				{
					timer.IsCounting = false;
				}
				else
				{
					timer.StartTicks += (long)num2 * 10L;
				}
			}
		}
		if (IsInDelay)
		{
			Ticks++;
			if (Ticks >= DelayDueTicks)
			{
				IsInDelay = false;
			}
			if (DelayDueTicks > Ticks + 20)
			{
				Thread.Sleep(1);
				Ticks += 20L;
			}
			return true;
		}
		DeviceCommand deviceCommand = ReadInstruction();
		if (deviceCommand == DeviceCommand.ENDPROG)
		{
			return false;
		}
		ExecuteCommand(deviceCommand);
		Ticks++;
		return true;
	}

	protected void Setup()
	{
		Pointer = 0;
		Ticks = 0L;
		IsInDelay = false;
		IntVars = new int[1024];
		BoolVars = new bool[1024];
		VarStack = new Stack<int>(256);
		CallStack = new Stack<int>(64);
		Timers = new List<InterpreterDownTimer>();
		for (int i = 0; i < 4; i++)
		{
			Timers.Add(new InterpreterDownTimer());
		}
	}

	public virtual void SetCode(byte[] code)
	{
		if (code == null)
		{
			throw new ArgumentNullException("code");
		}
		Setup();
		Code = code;
	}

	protected virtual void PushVar(int value)
	{
		if (VarStack.Count >= 256)
		{
			throw new InterpreterException("VarStack overflow.");
		}
		VarStack.Push(value);
	}

	protected virtual int PopVar()
	{
		if (VarStack.Count == 0)
		{
			throw new InterpreterException("VarStack empty.");
		}
		return VarStack.Pop();
	}

	protected virtual void PushCall(int value)
	{
		if (CallStack.Count >= 64)
		{
			throw new InterpreterException("CallStack overflow.");
		}
		CallStack.Push(value);
	}

	protected virtual int PopCall()
	{
		if (CallStack.Count == 0)
		{
			throw new Exception("CallStack empty.");
		}
		return CallStack.Pop();
	}

	protected virtual DeviceCommand ReadInstruction()
	{
		if (Code == null)
		{
			throw new InterpreterException("Code should not be null.");
		}
		if (Pointer < 0)
		{
			throw new InterpreterException($"Invalid pointer {Pointer}.");
		}
		if (Pointer >= Code.Length)
		{
			return DeviceCommand.ENDPROG;
		}
		return (DeviceCommand)Code[Pointer++];
	}

	protected virtual int Read8()
	{
		if (Pointer >= Code.Length)
		{
			throw new InterpreterException("Execution pointer pass end of program.");
		}
		return Code[Pointer++];
	}

	protected virtual int Read16()
	{
		return Read8() | (Read8() << 8);
	}

	protected virtual int Read32()
	{
		return Read8() | (Read8() << 8) | (Read8() << 16) | (Read8() << 24);
	}

	protected virtual void LoadIntVar()
	{
		int num = PopVar();
		if (num < 0 || num >= 1024)
		{
			throw new InterpreterException($"IntVariable index '{num}' out of range.");
		}
		PushVar(IntVars[num]);
	}

	protected virtual void SaveIntVar()
	{
		int num = PopVar();
		if (num < 0 || num >= 1024)
		{
			throw new InterpreterException($"IntVariable index '{num}' out of range.");
		}
		IntVars[num] = PopVar();
	}

	protected virtual void LoadBoolVar()
	{
		int num = PopVar();
		if (num < 0 || num >= 1024)
		{
			throw new InterpreterException($"BoolVariable index '{num}' out of range.");
		}
		PushVar(BoolVars[num] ? (-1) : 0);
	}

	protected virtual void SaveBoolVar()
	{
		int num = PopVar();
		if (num < 0 || num >= 1024)
		{
			throw new InterpreterException($"BoolVariable index '{num}' out of range.");
		}
		BoolVars[num] = PopVar() != 0;
	}

	protected virtual bool ExecuteConst(DeviceCommand cmd)
	{
		switch (cmd)
		{
		case DeviceCommand.CONST0:
			PushVar(0);
			return true;
		case DeviceCommand.CONST8:
			PushVar(Read8());
			return true;
		case DeviceCommand.CONST16:
			PushVar(Read16());
			return true;
		case DeviceCommand.CONST32:
			PushVar(Read32());
			return true;
		default:
			return false;
		}
	}

	protected virtual bool ExecuteLoadStore(DeviceCommand cmd)
	{
		switch (cmd)
		{
		case DeviceCommand.ILOAD:
			LoadIntVar();
			return true;
		case DeviceCommand.ISTORE:
			SaveIntVar();
			return true;
		case DeviceCommand.BLOAD:
			LoadBoolVar();
			return true;
		case DeviceCommand.BSTORE:
			SaveBoolVar();
			return true;
		default:
			return false;
		}
	}

	protected virtual bool ExecuteOperator(DeviceCommand cmd)
	{
		switch (cmd)
		{
		case DeviceCommand.PLUS:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar(num2 + num);
			return true;
		}
		case DeviceCommand.MINUS:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar(num2 - num);
			return true;
		}
		case DeviceCommand.MULT:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar(num2 * num);
			return true;
		}
		case DeviceCommand.DIV:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar(num2 / num);
			return true;
		}
		case DeviceCommand.MOD:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar(num2 % num);
			return true;
		}
		case DeviceCommand.ABS:
			PushVar(Math.Abs(PopVar()));
			return true;
		case DeviceCommand.NEG:
			PushVar(-PopVar());
			return true;
		case DeviceCommand.AND:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar(num2 & num);
			return true;
		}
		case DeviceCommand.OR:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar(num2 | num);
			return true;
		}
		case DeviceCommand.XOR:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar(num2 ^ num);
			return true;
		}
		case DeviceCommand.INV:
			PushVar(~PopVar());
			return true;
		case DeviceCommand.INC:
			PushVar(PopVar() + 1);
			return true;
		case DeviceCommand.DEC:
			PushVar(PopVar() - 1);
			return true;
		case DeviceCommand.LSL:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar(num2 << num);
			return true;
		}
		case DeviceCommand.LSR:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar(num2 >> num);
			return true;
		}
		default:
			return false;
		}
	}

	protected virtual bool ExecuteIf(DeviceCommand cmd)
	{
		switch (cmd)
		{
		case DeviceCommand.IFEQ:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar((num2 == num) ? (-1) : 0);
			return true;
		}
		case DeviceCommand.IFNE:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar((num2 != num) ? (-1) : 0);
			return true;
		}
		case DeviceCommand.IFGT:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar((num2 > num) ? (-1) : 0);
			return true;
		}
		case DeviceCommand.IFGTE:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar((num2 >= num) ? (-1) : 0);
			return true;
		}
		case DeviceCommand.IFLT:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar((num2 < num) ? (-1) : 0);
			return true;
		}
		case DeviceCommand.IFLTE:
		{
			int num = PopVar();
			int num2 = PopVar();
			PushVar((num2 <= num) ? (-1) : 0);
			return true;
		}
		default:
			return false;
		}
	}

	protected virtual bool ExecuteJump(DeviceCommand cmd)
	{
		switch (cmd)
		{
		case DeviceCommand.JUMPIFTRUE:
		{
			int pointer = PopVar();
			if (PopVar() != 0)
			{
				Pointer = pointer;
			}
			return true;
		}
		case DeviceCommand.JUMPIFFALSE:
		{
			int pointer = PopVar();
			if (PopVar() == 0)
			{
				Pointer = pointer;
			}
			return true;
		}
		case DeviceCommand.JUMP:
			Pointer = PopVar();
			return true;
		case DeviceCommand.CALL:
		{
			int pointer = PopVar();
			PushCall(Pointer);
			Pointer = pointer;
			return true;
		}
		case DeviceCommand.RETURN:
		{
			int pointer = PopCall();
			Pointer = pointer;
			return true;
		}
		default:
			return false;
		}
	}

	protected virtual bool ExecuteWait(DeviceCommand cmd)
	{
		if (cmd == DeviceCommand.WAITTIME)
		{
			int num = PopVar();
			DelayDueTicks = Ticks + (long)num * 10L;
			IsInDelay = true;
			return true;
		}
		return false;
	}

	protected virtual void Print(string message)
	{
		if (Console != null)
		{
			Console.Print(message);
		}
	}

	protected bool IsNextCommandPrintString()
	{
		if (Pointer > Code.Length - 4)
		{
			return false;
		}
		if (Code[Pointer] != 2)
		{
			return false;
		}
		if (Code[Pointer + 2] != 35)
		{
			return false;
		}
		if (Code[Pointer + 3] != 36)
		{
			return false;
		}
		return true;
	}

	protected virtual bool ExecutePrint(DeviceCommand cmd)
	{
		switch (cmd)
		{
		case DeviceCommand.PRINTSTRING:
		{
			int num = Read8();
			StringBuilder stringBuilder = new StringBuilder();
			for (int i = 0; i < num; i++)
			{
				stringBuilder.Append((char)Read8());
			}
			while (num == 32 && IsNextCommandPrintString())
			{
				Pointer += 4;
				num = Read8();
				for (int j = 0; j < num; j++)
				{
					stringBuilder.Append((char)Read8());
				}
			}
			Print(stringBuilder.ToString());
			return true;
		}
		case DeviceCommand.PRINTVAL:
			Print($"{PopVar()}");
			return true;
		case DeviceCommand.PRINTBOOL:
			Print(string.Format("{0}", (PopVar() != 0) ? "True" : "False"));
			return true;
		default:
			return false;
		}
	}

	protected virtual bool ExecuteConvert(DeviceCommand cmd)
	{
		if (cmd == DeviceCommand.BOOL)
		{
			PushVar((PopVar() != 0) ? (-1) : 0);
			return true;
		}
		return false;
	}

	protected virtual bool ExecuteMath(DeviceCommand cmd)
	{
		switch (cmd)
		{
		case DeviceCommand.SQRT:
		{
			int num = PopVar();
			PushVar((int)(1000.0 * Math.Round(Math.Sqrt(num), 3)));
			return true;
		}
		case DeviceCommand.SIN:
		{
			double d = Math.PI * (double)PopVar() / 180.0;
			PushVar((int)(1000.0 * Math.Round(Math.Sin(d), 3)));
			return true;
		}
		case DeviceCommand.COS:
		{
			double d = Math.PI * (double)PopVar() / 180.0;
			PushVar((int)(1000.0 * Math.Round(Math.Cos(d), 3)));
			return true;
		}
		case DeviceCommand.ATAN:
			PushVar((int)(1800.0 * Math.Atan((double)PopVar() / 1000.0) / Math.PI));
			return true;
		default:
			return false;
		}
	}

	protected virtual void SetConfig(int constant, int index, int value)
	{
		if (Console != null)
		{
			Console.SetConfig(constant, index, value);
		}
	}

	protected virtual int GetConfig(int constant, int index)
	{
		if (Console != null)
		{
			return Console.GetConfig(constant, index);
		}
		return 0;
	}

	protected virtual void SetCommand(int constant, int index, int value)
	{
		if (Console != null)
		{
			Console.SetCommand(constant, index, value);
		}
	}

	protected virtual int GetValue(int constant, int index)
	{
		if (Console != null)
		{
			return Console.GetValue(constant, index);
		}
		return 0;
	}

	protected virtual bool ExecuteRoboteq(DeviceCommand cmd)
	{
		switch (cmd)
		{
		case DeviceCommand.SETCONF:
		{
			int value = PopVar();
			int index = PopVar();
			int constant = PopVar();
			SetConfig(constant, index, value);
			return true;
		}
		case DeviceCommand.GETCONF:
		{
			int index = PopVar();
			int constant = PopVar();
			int value = GetConfig(constant, index);
			PushVar(value);
			return true;
		}
		case DeviceCommand.SETCMD:
		{
			int value = PopVar();
			int index = PopVar();
			int constant = PopVar();
			SetCommand(constant, index, value);
			return true;
		}
		case DeviceCommand.GETVAL:
		{
			int index = PopVar();
			int constant = PopVar();
			int value = GetValue(constant, index);
			PushVar(value);
			return true;
		}
		default:
			return false;
		}
	}

	protected virtual bool ExecuteTimer(DeviceCommand cmd)
	{
		switch (cmd)
		{
		case DeviceCommand.GETTIMCNT:
		{
			int num = PopVar();
			if (num < 0 || num >= Timers.Count)
			{
				throw new InterpreterException($"Invalid timer index {num}, command '{cmd.ToString()}' at offset {Pointer - 1}.");
			}
			PushVar(Timers[num].Count);
			return true;
		}
		case DeviceCommand.SETTIMCNT:
		{
			int count = PopVar();
			int num = PopVar();
			if (num < 0 || num >= Timers.Count)
			{
				throw new InterpreterException($"Invalid timer index {num}, command '{cmd.ToString()}' at offset {Pointer - 1}.");
			}
			Timers[num].Count = count;
			Timers[num].IsCounting = true;
			Timers[num].StartTicks = Ticks;
			return true;
		}
		case DeviceCommand.SETTIMSTOP:
		{
			int num2 = PopVar();
			int num = PopVar();
			if (num < 0 || num >= Timers.Count)
			{
				throw new InterpreterException($"Invalid timer index {num}, command '{cmd.ToString()}' at offset {Pointer - 1}.");
			}
			Timers[num].IsCounting = num2 == 0;
			Timers[num].StartTicks = Ticks;
			return true;
		}
		case DeviceCommand.GETTIMSTOP:
		{
			int num = PopVar();
			if (num < 0 || num >= Timers.Count)
			{
				throw new InterpreterException($"Invalid timer index {num}, command '{cmd.ToString()}' at offset {Pointer - 1}.");
			}
			PushVar((!Timers[num].IsCounting) ? 1 : 0);
			return true;
		}
		default:
			return false;
		}
	}

	protected virtual bool ExecuteCan(DeviceCommand cmd)
	{
		return false;
	}

	protected virtual bool ExecuteCustom(DeviceCommand cmd)
	{
		return false;
	}

	protected virtual void ExecuteCommand(DeviceCommand cmd)
	{
		if (ExecuteConst(cmd) || ExecuteLoadStore(cmd) || ExecuteOperator(cmd) || ExecuteIf(cmd) || ExecuteJump(cmd) || ExecuteWait(cmd) || ExecutePrint(cmd) || ExecuteConvert(cmd) || ExecuteMath(cmd) || ExecuteRoboteq(cmd) || ExecuteTimer(cmd) || ExecuteCan(cmd) || ExecuteCustom(cmd))
		{
			return;
		}
		throw new InterpreterException($"Invalid command '{cmd.ToString()}' at offset {Pointer - 1}.");
	}
}
