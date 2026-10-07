using System.Text.Json;
using Roboteq.MicroBasic.Compiler;
using Roboteq.MicroBasic.Compiler.Common;
using RoborunPlus;
using Roboteq.MicroBasic.Simulation;
try {
 var input = Console.In.ReadToEnd();
 if (input.Length > 2*1024*1024) throw new Exception("Request too large");
 var request = JsonDocument.Parse(input);
 var source = request.RootElement.GetProperty("source").GetString().Replace("\r\n", "\n").Replace("\r", "\n");
 if (source.Length > 1024*1024) throw new Exception("Source too large");
 CompilerHelper.AddAndActivateList("RoboRun+ 3.3 offline", CompilerConstantGetSetConfig._EE, CompilerConstantSetCommand._VAR, CompilerConstantGetValue._VAR);
 var context = new CompilerContext();
 new MicroBasicCompiler().Compile(context, source);
 if (context.Errors.Count > 0) {
  Console.WriteLine(JsonSerializer.Serialize(new { ok=false, errors=context.Errors.Select(e=>e.ToString()).ToArray() })); return;
 }
 var memory=context.GetMemoryUsage();
 if(memory.NumIntegerVariables<0 || memory.NumIntegerVariables>1024 || memory.NumBooleanVariables<0 || memory.NumBooleanVariables>1024) throw new Exception("Script exceeds local limit of 1024 integer / boolean variables");
 var bytes = context.GetBinaryCode();
 if (bytes.Length == 0 || bytes.Length > 32768) throw new Exception("Bytecode outside 1..32768 bytes");
 object simulation = null;
 if (request.RootElement.TryGetProperty("simulate", out var sim) && sim.GetBoolean()) {
  var console = new LocalConsole(); var vm = new ByteCodeInterpreter {Console=console}; vm.SetCode(bytes);
  var timer = System.Diagnostics.Stopwatch.StartNew(); int steps=0; bool completed=false;
  while (steps<100000 && timer.ElapsedMilliseconds<3000) { steps++; if(!vm.ExecuteInstruction()) {completed=true;break;} }
  simulation=new {completed,steps,output=console.Output.ToString(),events=console.Events, milliseconds=vm.ElapsedTime.TotalMilliseconds};
 }
 var lines = new List<string>();
 for (int offset=0; offset<bytes.Length; offset+=16) {
  var record = new List<byte>{16,(byte)(offset>>8),(byte)offset,0};
  for(int j=0;j<16;j++) record.Add(offset+j<bytes.Length?bytes[offset+j]:(byte)255);
  record.Add(unchecked((byte)-record.Sum(b=>(int)b)));
  lines.Add(":"+Convert.ToHexString(record.ToArray()));
 }
 lines.Add(":00000001FF");
 Console.WriteLine(JsonSerializer.Serialize(new { ok=true, simulation, hex=string.Join("\n",lines)+"\n", bytecode=Convert.ToHexString(bytes), size=bytes.Length, crc32=CRC32.Compute(bytes).ToString("X8"), listing=context.DeviceCode.ToString(), warnings=context.Warnings.Select(e=>e.ToString()).ToArray(), constants="RoboRun+ 3.3 offline" }));
} catch(Exception e) { Console.WriteLine(JsonSerializer.Serialize(new {ok=false,errors=new[]{e.Message}})); }

sealed class LocalConsole : IInterpreterConsole {
 public System.Text.StringBuilder Output = new();
 public List<string> Events = new();
 private Dictionary<(int,int),int> config = new();
 public void Print(string message) {if(Output.Length+message.Length>65536) throw new Exception("Simulation output limit"); Output.Append(message);}
 public int GetValue(int constant,int index) {return 0;}
 public int GetConfig(int constant,int index) {return config.GetValueOrDefault((constant,index));}
 public void SetConfig(int constant,int index,int value) {config[(constant,index)]=value; Add($"SetConfig({constant}, {index}, {value})");}
 public void SetCommand(int constant,int index,int value) {Add($"SetCommand({constant}, {index}, {value})");}
 private void Add(string message) {if(Events.Count>=1000) throw new Exception("Simulation event limit"); Events.Add(message);}
}
