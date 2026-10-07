using System;
using System.Collections.Generic;
using System.Collections.Specialized;

namespace Roboteq.MicroBasic.Compiler.Common;

public static class CompilerHelper
{
	private static StringDictionary _reservedWords;

	private static CompilerConstantsList _activeConstantsList;

	private static List<CompilerConstantsList> _constantsLists;

	private static StringDictionary _configAliases;

	private static StringDictionary _commandsAliases;

	private static StringDictionary _valuesAliases;

	public static StringDictionary ReservedWords
	{
		get
		{
			if (_reservedWords == null)
			{
				InitReservedWords();
			}
			return _reservedWords;
		}
	}

	public static CompilerConstantsList ActiveConstantsList
	{
		get
		{
			return _activeConstantsList;
		}
		set
		{
			_activeConstantsList = value;
		}
	}

	public static List<CompilerConstantsList> ConstantsLists
	{
		get
		{
			return _constantsLists;
		}
		set
		{
			_constantsLists = value;
		}
	}

	public static StringDictionary ConfigAliases
	{
		get
		{
			if (_configAliases == null)
			{
				InitConfigAliases();
			}
			return _configAliases;
		}
		set
		{
			_configAliases = value;
		}
	}

	public static StringDictionary CommandsAliases
	{
		get
		{
			if (_commandsAliases == null)
			{
				InitCommandsAliases();
			}
			return _commandsAliases;
		}
		set
		{
			_commandsAliases = value;
		}
	}

	public static StringDictionary ValuesAliases
	{
		get
		{
			if (_valuesAliases == null)
			{
				InitValuesAliases();
			}
			return _valuesAliases;
		}
		set
		{
			_valuesAliases = value;
		}
	}

	public static void InitConfigAliases()
	{
		_configAliases = new StringDictionary();
	}

	public static void InitCommandsAliases()
	{
		_commandsAliases = new StringDictionary();
		_commandsAliases.Add("G", "GO");
		_commandsAliases.Add("M", "MOTCMD");
		_commandsAliases.Add("P", "MOTPOS");
		_commandsAliases.Add("S", "MOTVEL");
		_commandsAliases.Add("C", "SENCNTR");
		_commandsAliases.Add("CB", "SBLCNTR");
		_commandsAliases.Add("AC", "ACCEL");
		_commandsAliases.Add("DC", "DECEL");
		_commandsAliases.Add("DS", "DOUT");
		_commandsAliases.Add("D1", "DSET");
		_commandsAliases.Add("D0", "DRES");
		_commandsAliases.Add("R", "BRUN");
		_commandsAliases.Add("H", "HOME");
		_commandsAliases.Add("EX", "ESTOP");
		_commandsAliases.Add("MG", "MGO");
		_commandsAliases.Add("MS", "MSTOP");
		_commandsAliases.Add("PR", "MPOSREL");
		_commandsAliases.Add("PX", "NXTPOS");
		_commandsAliases.Add("PRX", "NXTPOSR");
		_commandsAliases.Add("AX", "NXTACC");
		_commandsAliases.Add("DX", "NXTDEC");
		_commandsAliases.Add("B", "BOOL");
		_commandsAliases.Add("SX", "NXTVEL");
		_commandsAliases.Add("CS", "CANSEND");
		_commandsAliases.Add("CG", "CANGO");
		_commandsAliases.Add("RC", "RCOUT");
		_commandsAliases.Add("EES", "EESAV");
		_commandsAliases.Add("BND", "BIND");
		_commandsAliases.Add("AO", "AOUT");
		_commandsAliases.Add("TX", "LEFT");
		_commandsAliases.Add("TV", "RIGHT");
		_commandsAliases.Add("CSW", "CELLSW");
		_commandsAliases.Add("PSW", "BMSPSW");
		_commandsAliases.Add("ASW", "BMSASW");
		_commandsAliases.Add("ANG", "ANGLE");
	}

	public static void InitValuesAliases()
	{
		_valuesAliases = new StringDictionary();
		_valuesAliases.Add("A", "MOTAMPS");
		_valuesAliases.Add("M", "MOTCMD");
		_valuesAliases.Add("P", "MOTPWR");
		_valuesAliases.Add("S", "ABSPEED");
		_valuesAliases.Add("C", "ABCNTR");
		_valuesAliases.Add("CB", "BLCNTR");
		_valuesAliases.Add("SR", "RELSPEED");
		_valuesAliases.Add("CR", "RELCNTR");
		_valuesAliases.Add("BCR", "BLRCNTR");
		_valuesAliases.Add("BS", "BLSPEED");
		_valuesAliases.Add("BSR", "BLRSPEED");
		_valuesAliases.Add("BA", "BATAMPS");
		_valuesAliases.Add("V", "VOLTS");
		_valuesAliases.Add("D", "DIGIN");
		_valuesAliases.Add("DI", "DIN");
		_valuesAliases.Add("AI", "ANAIN");
		_valuesAliases.Add("PI", "PLSIN");
		_valuesAliases.Add("T", "TEMP");
		_valuesAliases.Add("F", "FEEDBK");
		_valuesAliases.Add("FS", "STFLAG");
		_valuesAliases.Add("FF", "FLTFLAG");
		_valuesAliases.Add("B", "BOOL");
		_valuesAliases.Add("DO", "DIGOUT");
		_valuesAliases.Add("E", "LPERR");
		_valuesAliases.Add("CIS", "CMDSER");
		_valuesAliases.Add("CIA", "CMDANA");
		_valuesAliases.Add("CIP", "CMDPLS");
		_valuesAliases.Add("TM", "TIME");
		_valuesAliases.Add("LK", "LOCKED");
		_valuesAliases.Add("TR", "TRACK");
		_valuesAliases.Add("K", "SPEKTRUM");
		_valuesAliases.Add("DR", "DREACHED");
		_valuesAliases.Add("AIC", "ANAINC");
		_valuesAliases.Add("PIC", "PLSINC");
		_valuesAliases.Add("MA", "MEMS");
		_valuesAliases.Add("CL", "CALIVE");
		_valuesAliases.Add("MGD", "MGDET");
		_valuesAliases.Add("MGT", "MGTRACK");
		_valuesAliases.Add("MGM", "MGMRKR");
		_valuesAliases.Add("MGS", "MGSTATUS");
		_valuesAliases.Add("MGY", "MGYRO");
		_valuesAliases.Add("FM", "MOTFLAG");
		_valuesAliases.Add("HS", "HSENSE");
		_valuesAliases.Add("ASI", "RAWSENSADC");
		_valuesAliases.Add("QO", "QOUT");
		_valuesAliases.Add("EO", "EOUT");
		_valuesAliases.Add("RMA", "MACC");
		_valuesAliases.Add("RMG", "RGYRO");
		_valuesAliases.Add("RMM", "MMAG");
		_valuesAliases.Add("ML", "MALL");
		_valuesAliases.Add("TS", "LINES");
		_valuesAliases.Add("MRS", "MSENS");
		_valuesAliases.Add("MZ", "MSZER");
		_valuesAliases.Add("PK", "MPEAK");
		_valuesAliases.Add("RF", "MREF");
		_valuesAliases.Add("GY", "GYRO");
		_valuesAliases.Add("ANG", "ANGLE");
		_valuesAliases.Add("HCT", "HCOUNTER");
		_valuesAliases.Add("HDT", "HDATA");
	}

	static CompilerHelper()
	{
		_constantsLists = new List<CompilerConstantsList>();
		InitDefaultList();
	}

	private static void InitReservedWords()
	{
		_reservedWords = new StringDictionary();
		MicroBasicGrammar microBasicGrammar = new MicroBasicGrammar();
		foreach (string key in microBasicGrammar.KeyTerms.Keys)
		{
			if (microBasicGrammar.KeyTerms[key].Name.Length > 0 && char.IsLetter(microBasicGrammar.KeyTerms[key].Name[0]))
			{
				_reservedWords.Add(microBasicGrammar.KeyTerms[key].Name, microBasicGrammar.KeyTerms[key].Name);
			}
		}
	}

	public static bool IsReservedWord(string word)
	{
		return ReservedWords.ContainsKey(word);
	}

	private static void InitDefaultList()
	{
	}

	public static bool IsNamedConstant(string name)
	{
		if (ActiveConstantsList == null)
		{
			return false;
		}
		return ActiveConstantsList.ContainsKey(name);
	}

	public static int GetConstantValue(string name)
	{
		if (ActiveConstantsList == null)
		{
			return 0;
		}
		return ActiveConstantsList.GetValue(name);
	}

	public static int? GetCommandConstant(string name)
	{
		if (ActiveConstantsList == null || !ActiveConstantsList.CommandsList.ContainsKey(name))
		{
			return null;
		}
		return ActiveConstantsList.CommandsList.GetValue(name);
	}

	public static int? GetQueryConstant(string name)
	{
		if (ActiveConstantsList == null || !ActiveConstantsList.ValuesList.ContainsKey(name))
		{
			return null;
		}
		return ActiveConstantsList.ValuesList.GetValue(name);
	}

	public static int? GetConfigConstant(string name)
	{
		if (ActiveConstantsList == null || !ActiveConstantsList.ConfigList.ContainsKey(name))
		{
			return null;
		}
		return ActiveConstantsList.ConfigList.GetValue(name);
	}

	public static CompilerConstantsList AddAndActivateList<T1, T2, T3>(string name, T1 config, T2 command, T3 value)
	{
		return ActiveConstantsList = AddList(name, config, command, value);
	}

	public static CompilerConstantsList AddList<T1, T2, T3>(string name, T1 config, T2 command, T3 value)
	{
		CompilerConstantsList compilerConstantsList = new CompilerConstantsList(name);
		ConstantsLists.Add(compilerConstantsList);
		string[] names = Enum.GetNames(config.GetType());
		foreach (string text in names)
		{
			compilerConstantsList.ConfigList.Add(text, (int)Enum.Parse(config.GetType(), text));
		}
		string[] names2 = Enum.GetNames(command.GetType());
		foreach (string text2 in names2)
		{
			compilerConstantsList.CommandsList.Add(text2, (int)Enum.Parse(command.GetType(), text2));
		}
		string[] names3 = Enum.GetNames(value.GetType());
		foreach (string text3 in names3)
		{
			compilerConstantsList.ValuesList.Add(text3, (int)Enum.Parse(value.GetType(), text3));
		}
		return compilerConstantsList;
	}
}
