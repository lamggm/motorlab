#!/usr/bin/env python3
"""Offline regressions. Optional --oracle DOTNET ORACLE_DLL compares original DLL."""
import argparse, json, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--oracle',nargs=2);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
helper=root/'libexec/compiler/motorlab-compiler'
cases={
 'arithmetic':('dim x as integer\nx=2+3*4\nprint(x)\n','14'),
 'array_for':('dim a[4] as integer\nfor i=0 to 3\na[i]=i*2\nprint(a[i])\nnext\n','0246'),
 'if':('dim x as integer\nx=3\nif x>2 then\nprint("yes")\nelse\nprint("no")\nend if\n','yes'),
 'gosub':('gosub hello\ngoto done\nhello:\nprint("hi")\nreturn\ndone:\n','hi'),
 'constants':('dim amps as integer\namps=getvalue(_A,1)\nsetcommand(_G,1,0)\nsetconfig(_ALIM,1,200)\nprint(amps)\n','0'),
 'boolean':('dim b as boolean\nb=true\nprint(b)\n','True'),
 'invalid':('dim x as integer\nx=\n',None),
 'crlf':("' header\r\ndim x as integer\r\nx=123\r\nprint(x)\r\n",'123'),
}
def run(cmd,source,simulate=False):
 r=subprocess.run([str(x) for x in cmd],input=json.dumps({'source':source,'simulate':simulate}),text=True,capture_output=True,timeout=10,check=True)
 return json.loads(r.stdout)
for name,(source,expected) in cases.items():
 result=run([helper],source,expected is not None)
 if expected is None: assert not result['ok'],name
 else:
  assert result['ok'],(name,result)
  assert result['simulation']['completed'],name
  assert result['simulation']['output'].lower()==expected.lower(),(name,result['simulation'])
 if a.oracle:
  original=run(a.oracle,source,expected is not None)
  for key in ['ok','hex','bytecode','size','crc32','errors','warnings','simulation']:assert result.get(key)==original.get(key),(name,key)
 print('PASS',name)
for path in sorted((root/'tests/fixtures/scripts').glob('*.mbs')):
 source=path.read_text(encoding='utf-8-sig');result=run([helper],source);assert result['ok'],path.name
 if a.oracle:
  original=run(a.oracle,source)
  for key in ['hex','bytecode','size','crc32']:assert result[key]==original[key],(path.name,key)
 print('PASS',path.name,result['size'],'bytes')
for name,source in [('infinite','while true\nx=x+1\nend while\n'),('long_wait','wait(60000)\n'),('output_limit','while true\nprint("012345678901234567890123456789")\nend while\n')]:
 result=run([helper],source,True)
 assert not result['ok'] or not result['simulation']['completed'],name
 print('PASS bounded',name)

result=run([helper],"dim a[1025] as integer\na[0]=1\n")
assert not result['ok'] and '1024' in str(result),result
print('PASS variable memory limit')
