#!/usr/bin/env python3
"""Recover model hierarchy without deserializing embedded .NET icon blobs.
Usage: generate_original_trees.py PAYLOAD_DIR OUTPUT_JSON
"""
from pathlib import Path
import xml.etree.ElementTree as E, json, sys
root=Path(sys.argv[1])/'Trees/v3.0'
lookups=json.loads((Path(__file__).resolve().parents[1]/"data/original-lookups.json").read_text())
out={}
for model in ['SBLG2360T','SBLMG2360T']:
 r=E.parse(root/(model+'.ctree')).getroot();cb=r.find('CFG/PBC').get('CB');entries=[]
 def walk(n,path,tree):
  if n.tag=='N':
   path=path+[n.get('T','')];cmd=n.find('CFG/CMD');cc=n.find('CFG/CC')
   if cmd is not None and cmd.get('T'):
    choices=[]
    if cc is not None:
     for i,li in enumerate(cc.findall('LI')):choices.append({'value':i,'label':li.text or li.get('T','')})
    if cc is not None and cc.get('TYP') in lookups:choices=[{'value':i,'label':v} for i,v in enumerate(lookups[cc.get('TYP')])]
    entries.append({'command':cmd.get('T'),'index':int(cmd.get('I','1')),'type':cmd.get('ARG','IntegerValue'),'path':path,'tree':tree,'description':n.get('D',''),'control':n.find('CFG').get('CT',''),'config':cc.attrib if cc is not None else {},'choices':choices})
  for c in n:
   if c.tag in ['N','Nodes']:walk(c,path,tree)
 for tree,xml in [('PowerTree',r),('ControlTree',E.parse(root/(cb+'.ctree')).getroot())]:
  def roots(e):
   for c in e:
    if c.tag=='N':walk(c,[],tree)
    elif c.tag not in ['CFG','Icons']:roots(c)
  roots(xml)
 out[model]={'controlTree':cb,'powerTree':'v3.0:'+model,'entries':entries}
Path(sys.argv[2]).write_text(json.dumps(out,ensure_ascii=False,indent=2))
