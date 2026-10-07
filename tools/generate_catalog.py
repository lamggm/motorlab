#!/usr/bin/env python3
"""Extract numeric configuration definitions from Roboteq v3.0c pdftotext -layout.
The checked-in catalog is reviewed and embedded; generation is not needed to build.
Ambiguous bounds keep the field read-only. ACIM, Ethernet and EtherCAT are excluded.
"""
import sys, re, json
from pathlib import Path
from configuration_units import apply_units
text=Path(sys.argv[1]).read_text()
pages=text.split('\f')
start=sum(len(p)+1 for p in pages[:348])
end=sum(len(p)+1 for p in pages[:468])
body=text[start:end]
heads=list(re.finditer(r'^\s*([A-Z][A-Z0-9]+)\s*[-–]\s*([^\n]+)',body,re.M))
exclude=set('BEE THLD BRV BHV BDT ILM ILLR IRR MPW MXS RFC VPH ECAT ECT EDID'.split())
# Values below resolve explicit textual ranges/model counts; no screenshot defaults.
index_counts={'CPRI':3,'EE':32,'DINL':10,'KPG':4,'KIG':4,'KDG':4,'KPF':4,'KIF':4,'HSAT':24,'ZSMC':6,'ZSMA':2,'RPDM':64,'TPDM':64,'RPDC':8,'TPDC':8,'CTPS':8,'CTT':8,'CHBT':4,'CGT':2,'CNOD':1}
value_bounds={'OVL':(100,600),'UVL':(50,600),'TNM':(0,2147483647),'TPAL':(0,4294967295),'DINL':(0,1),'EE':(-32768,32767),'DFC':(-1000,1000),'ALIM':(10,450),'ATRIG':(0,450),'NOMA':(0,450),'BZPW':(0,450),'MDIR':(0,1),'CPRI':(0,3),'RSBR':(0,4),'MMOD':(0,6),'TPDM':(0,4294967295),'RPDM':(0,4294967295),'TPDC':(0,4294967295),'RPDC':(0,4294967295),'CTPS':(0,65535),'ZSMC':(-32768,32767)}
scales={'ALIM':(10,'A RMS'),'ATRIG':(10,'A'),'NOMA':(10,'A RMS'),'BZPW':(10,'A'),'OVL':(10,'V'),'UVL':(10,'V'),'OVH':(10,'V'),'MAC':(10,'RPM/s'),'MDEC':(10,'RPM/s'),'EDEC':(10,'RPM/s'),'KPG':(1000000,'ganho'),'KIG':(1000000,'ganho'),'KDG':(1000000,'ganho'),'KPF':(10000,'ganho'),'KIF':(10000,'ganho'),'RS':(1000,'Ohm'),'LD':(1,'uH'),'LQ':(1,'uH'),'MXRPM':(1,'RPM'),'MNRPM':(1,'RPM'),'MVEL':(1,'RPM'),'TNM':(1000,'Nm/A'),'TPAL':(1,'s'),'RWD':(1,'ms'),'MXTRN':(10,'voltas'),'VELFF':(1000,'ganho')}
scales.update({'MXPW':(10,'W'),'LD':(1000,'mH'),'LQ':(1000,'mH'),'VK':(1000,'V/kRPM'),'FWVR':(10,'%'),'JR':(1000,'kg.cm²'),'BR':(10000,'mNm/(rad/s)'),'MXTRN':(100,'voltas')})
value_bounds.update({'B25':(-65000,65000),'FET':(1,30000),'FEW':(1,30000),'R25':(0,200000),'SED':(0,2),'ZSMA':(-32768,32767),'MXPW':(0,500000)})
startup=set('ACS AMS BRUN CLIN CPRI DFC ECHOF FLCL ISM PMS RS485 RSBR RWD SCRO STO TELS'.split())
general=set('OVL OVH UVL OTL BKD MXMD MLX STO FLCL'.split())
seen=set(); specs=[]
def getnum(v):
 v=v.replace(',','').strip()
 if v in ('-2147M','-2147','-2147M','-2'): return -2147483648 if 'M' in v else int(v)
 if '2147' in v and 'M' in v:return 2147483647
 try:return int(v)
 except ValueError:return None
for i,m in enumerate(heads):
 key=m[1]; name=m[2].strip(); seg=body[m.start():heads[i+1].start() if i+1<len(heads) else len(body)]
 if key in seen or key in exclude or key=='BFBK' and 'Induction' in name:continue
 syntax=re.search(r'Syntax Serial:?\s*\^'+key+r'([^\n]*)',seg)
 if not syntax and key=='SLL':syntax=re.search(r'Syntax Serial:?\s*\^SELL([^\n]*)',seg)
 if not syntax:continue
 args=re.findall(r'\[?[a-z]+\]?',syntax[1].lower())
 argument_matches=list(re.finditer(r'Argument\s+(\d+)\s*:',seg))
 narg=2 if len(args)>1 else 1
 # Prefer the actual highest declared argument; syntax in old manuals has typos.
 if argument_matches:narg=max(int(a[1]) for a in argument_matches)
 if key=='DINL':narg=2
 if narg not in (1,2):continue
 value_part=seg
 if argument_matches:
  value_part=seg[argument_matches[-1].start():]
 ranges=re.search(r'Min:\s*([+-]?[\d,]+M?)\s+Max:\s*([+-]?[\d,]+M?)',value_part)
 low,high=(getnum(ranges[1]),getnum(ranges[2])) if ranges else (None,None)
 if key in value_bounds:low,high=value_bounds[key]
 typ=re.search(r'Type:\s*((?:Un)?[Ss]igned)\s+(8|16|32)-bit',value_part)
 # If the manual omits value range but gives a storage type, reading remains available.
 if typ and high is not None:
  bits=int(typ[2]);signed=typ[1].lower()=='signed'
  high=min(high,(1<<(bits-1))-1 if signed else (1<<bits)-1)
 editable=low is not None and high is not None and low<=high
 if key == "ISM": editable=True; low=0; high=4 # v3.0 tree from RoboRun+ 3.3: 0,2,4; runtime rejects holes.
 if low is None or high is None:
  bits=int(typ[2]) if typ else 32
  signed=typ and typ[1].lower()=='signed'
  low=-(1<<(bits-1)) if signed else 0
  high=(1<<(bits-1))-1 if signed else (1<<bits)-1
 index_max=0
 if narg==2:
  ipart=seg[argument_matches[0].start():argument_matches[1].start()] if len(argument_matches)>=2 else seg
  max_match=re.search(r'Max:\s*(\d+)\b',ipart)
  index_max=int(max_match[1]) if max_match else 2
  if 'Total Number of Analog' in ipart or 'Analog input' in ipart:index_max=8
  if 'Total Number of Pulse' in ipart:index_max=8
  if 'Total Number of Digital Input' in ipart:index_max=10
  if 'Total Number of Digital Output' in ipart:index_max=4
  if 'Total Number of Motors' in ipart:index_max=2
  if 'Total Number of Encoders' in ipart:index_max=2
  index_max=index_counts.get(key,index_max)
  index_max=min(index_max,64)
 if key in startup:group='Comandos / segurança'
 elif key in general:group='Geral'
 elif key.startswith('A') and key in 'AMAX AMIN ACTR ADB ALIN AMOD AINA AMAXA AMINA APOL'.split():group='Entradas analógicas'
 elif key in 'DINL DINA DINLA DOUT DOA AUXV DOL DOT'.split():group='Entradas / saídas digitais'
 elif key in 'PINA PLIN PMAX PMAXA PMIN PMINA PMOD PPOL PCTR PDB'.split():group='Entradas de pulso'
 elif key in 'DMOD MDAL MNOD'.split():group='Modbus'
 elif key in 'CAS CBR CEN CGT CHB CHBT CHLA CLSN CNOD CSRT CTPS CTT FSA RPDC RPDM TPDC TPDM'.split():group='CAN / CANopen'
 elif key in 'EHL EHLA EHOME ELL ELLA EMOD EPPR ENCO'.split():group='Encoders'
 elif key in 'MCLE MSTA SCLE SCLK SHL SHLA SHOME SLEN SLL SLLA SMOD SSTA'.split():group='Sensores SSI'
 elif key=='EE':group='Variáveis do usuário'
 else:group='Motor / potência'
 can=re.search(r'CANOpen id:\s*(0x[\da-fA-F]+)',seg)
 page=text[:start+m.start()].count('\f')+1
 if key=='ATGA':low=0
 scale,unit=scales.get(key,(1,'bruto'))
 # Communication identity changes are explicit operations, excluded from bulk restores.
 disruptive=key in 'RSBR RS485 CBR CEN CNOD ECHOF ISM STO CPRI BRUN RWD'.split()
 specs.append(dict(key=key,name=name,group=group,index_max=index_max,min=low,max=high,editable=bool(editable),scale=scale,unit=unit,disruptive=disruptive,can_index=int(can[1],16) if can else 0,page=page))
 seen.add(key)
for key,imax,low,high,page,group in [('RPDC',8,0,4294967295,466,'CAN / CANopen'),('TPDC',8,0,4294967295,467,'CAN / CANopen')]:
 if key not in seen:specs.append(dict(key=key,name=key+' COB-ID',group=group,index_max=imax,min=low,max=high,editable=True,scale=1,unit='bruto',disruptive=False,can_index=0x310B if key=='RPDC' else 0x310C,page=page))
specs.append(dict(key='OTL',name='Over Temperature Limit',group='Geral',index_max=3,min=0,max=120,editable=True,scale=1,unit='°C',disruptive=False,can_index=0x30D1,page=417))
apply_units(specs)
Path(sys.argv[2]).write_text(json.dumps({'source':'Roboteq Controllers User Manual v3.0c, 01/09/2026','models':['SBLG2360T','SBLMG2360T'],'parameters':specs},indent=2,ensure_ascii=False)+'\n')
print(f'{len(specs)} definições; {sum(max(1,s["index_max"]) for s in specs)} campos; {sum(s["editable"] for s in specs)} definições editáveis')
print('Sem limites claros (somente leitura):',','.join(s['key'] for s in specs if not s['editable']))
