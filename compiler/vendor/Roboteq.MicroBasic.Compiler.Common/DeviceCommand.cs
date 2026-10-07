namespace Roboteq.MicroBasic.Compiler.Common;

public enum DeviceCommand
{
	None = -1,
	[DeviceCommand("ENDPROG", "Exits interpreter permanently")]
	ENDPROG,
	[DeviceCommand("CONST0", "push 0 in stack")]
	CONST0,
	[DeviceCommand("CONST8", "push signed value of next byte to stack")]
	CONST8,
	[DeviceCommand("CONST16", "push signed value of next 2 bytes (LS,MS) to stack")]
	CONST16,
	[DeviceCommand("CONST32", "push signed value of next 4 bytes (LS ….MS)  to stack")]
	CONST32,
	[DeviceCommand("ILOAD", "pop address, push to stack the content of int var(address)")]
	ILOAD,
	[DeviceCommand("ISTORE", "pop address, pop value from stack and save at int var(address)")]
	ISTORE,
	[DeviceCommand("BLOAD", "pop address, push to stack the content of bool var(address)")]
	BLOAD,
	[DeviceCommand("BSTORE", "pop address, pop value from stack and save at bool var(address)")]
	BSTORE,
	[DeviceCommand("PLUS", "pop op2, pop op1,  push op1 + op2")]
	PLUS,
	[DeviceCommand("MINUS", "pop op2, pop op1,  push op1 - op2")]
	MINUS,
	[DeviceCommand("MULT", "pop op2, pop op1,  push op1 * op2")]
	MULT,
	[DeviceCommand("DIV", "pop op2, pop op1,  push op1 / op2")]
	DIV,
	[DeviceCommand("MOD", "pop op2, pop op1,  push op1 % op2")]
	MOD,
	[DeviceCommand("ABS", "pop op1,  push abs(op1)")]
	ABS,
	[DeviceCommand("NEG", "pop op1,  push -op1")]
	NEG,
	[DeviceCommand("AND", "pop op2, pop op1,  push op1 & op2")]
	AND,
	[DeviceCommand("OR", "pop op2, pop op1,  push op1 | op2")]
	OR,
	[DeviceCommand("XOR", "pop op2, pop op1,  push op1 ^ op2")]
	XOR,
	[DeviceCommand("INV", "pop op1,  push ~op1")]
	INV,
	[DeviceCommand("INC", "pop op1,  push op1++")]
	INC,
	[DeviceCommand("DEC", "pop op1,  push op1--")]
	DEC,
	[DeviceCommand("LSL", "pop op2, pop op1,  push op1 << op2")]
	LSL,
	[DeviceCommand("LSR", "pop op2, pop op1,  push op1 >> op2")]
	LSR,
	[DeviceCommand("IFEQ", "pop op2, pop op1,  if ((op1 - op2) == 0) push 1, else push 0")]
	IFEQ,
	[DeviceCommand("IFNE", "pop op2, pop op1,  if ((op1 - op2 != 0) push 1, else push 0")]
	IFNE,
	[DeviceCommand("IFGT", "pop op2, pop op1,  if ((op1 - op2) > 0) push 1, else push 0")]
	IFGT,
	[DeviceCommand("IFGTE", "pop op2, pop op1,  if ((op1 - op2) >= 0) push 1, else push 0")]
	IFGTE,
	[DeviceCommand("IFLT", "pop op2, pop op1,  if ((op1 - op2) < 0) push 1, else push 0")]
	IFLT,
	[DeviceCommand("IFLTE", "pop op2, pop op1,  if ((op1 - op2) <= 0) push 1, else push 0")]
	IFLTE,
	[DeviceCommand("JUMPIFTRUE", "pop jumpaddress, pop testresult,  jump to jumpaddress if testresule != 0")]
	JUMPIFTRUE,
	[DeviceCommand("JUMPIFFALSE", "pop jumpaddress, pop testresult,  jump to jumpaddress if testresule != 0")]
	JUMPIFFALSE,
	[DeviceCommand("JUMP", "pop jumpaddress, jump to jumpaddress")]
	JUMP,
	[DeviceCommand("CALL", "pop jumpaddress, push reurtunaddress to callstack")]
	CALL,
	[DeviceCommand("RETURN", "pop return address from callstack")]
	RETURN,
	[DeviceCommand("WAITTIME", "pop, store to timer variable, exit program with resum flag = 1")]
	WAITTIME,
	[DeviceCommand("PRINTSTRING", "print n bytes that follow print token.")]
	PRINTSTRING,
	[DeviceCommand("PRINTVAL", "pop, print integer value")]
	PRINTVAL,
	[DeviceCommand("PRINTBOOL", "pop, print boolean value")]
	PRINTBOOL,
	[DeviceCommand("SETCONF", "pop value, pop index, fetch next byte as paramnum call config function (paramnum,index, value)")]
	SETCONF,
	[DeviceCommand("GETCONF", "pop index, fetch next byte as paramnum call config function (paramnum,index), push result")]
	GETCONF,
	[DeviceCommand("SETCMD", "pop value, pop index, fetch next byte as paramnum call config function (paramnum,index, value(s))")]
	SETCMD,
	[DeviceCommand("GETVAL", "pop index, fetch next byte as paramnum call config function (paramnum,index), push result")]
	GETVAL,
	[DeviceCommand("GETTIMCNT", "pop index, push timer(index) value")]
	GETTIMCNT,
	[DeviceCommand("SETTIMCNT", "pop value, pop index, load value in timer(index)")]
	SETTIMCNT,
	[DeviceCommand("SETTIMSTOP", "pop stop/start value, pop index, load stop/start in timerss(index)")]
	SETTIMSTOP,
	[DeviceCommand("GETTIMSTOP", "pop index, push timerss(index) state")]
	GETTIMSTOP,
	[DeviceCommand("BOOL", "pop value then push 0 if the value is 0, -1 otherwise")]
	BOOL,
	IsCANNodeAlive,
	SetCANCommand,
	SetCANConfig,
	FetchCANConfig,
	IsCANConfigReady,
	ReadCANConfig,
	FetchCANValue,
	IsCANValueReady,
	ReadCANValue,
	ScanCANValue,
	IsScannedCANReady,
	ReadScannedCANValue,
	[DeviceCommand("SQRT", "pop op1,  push SQRT(op1)")]
	SQRT,
	[DeviceCommand("SIN", "pop op1,  push SIN(op1)")]
	SIN,
	[DeviceCommand("COS", "pop op1,  push COS(op1)")]
	COS,
	[DeviceCommand("ATAN", "pop op1,  push ATAN(op1)")]
	ATAN,
	SETVPM
}
