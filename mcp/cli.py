#!/usr/bin/env python3
"""CLI JSON compatível com as ferramentas MCP do MotorLab; sem interface gráfica."""
import argparse
import json
import math
import sys
import time
from server import Bridge, NAMES

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--socket', help='Socket privado do backend CLI.')
    parser.add_argument('--executable', help='Caminho do motorlabd.')
    parser.add_argument('--no-launch', action='store_true')
    parser.add_argument('--wait', action='store_true', help='Aguardar o job e retornar erro se falhar.')
    parser.add_argument('--timeout', type=float, default=120)
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ['status','ports','demo','disconnect','stop']:
        sub.add_parser(name)
    query = sub.add_parser('query'); query.add_argument('query')
    read = sub.add_parser('read'); read.add_argument('queries', nargs='*')
    con = sub.add_parser('connect'); con.add_argument('--port', required=True); con.add_argument('--baud', type=int, default=115200)
    job = sub.add_parser('job'); job.add_argument('job_id')
    call = sub.add_parser('call'); call.add_argument('tool'); call.add_argument('--arguments', default='{}')
    args = parser.parse_args()
    if not math.isfinite(args.timeout) or args.timeout <= 0: parser.error('--timeout deve ser finito e positivo.')
    tool = 'motorlab_'+args.command
    values = {}
    if args.command == 'demo': tool='motorlab_connect';values={'mode':'demo'}
    if args.command == 'connect': values={'mode':'serial','port':args.port,'baud':args.baud}
    if args.command == 'query': values={'command':args.query}
    if args.command == 'read': tool='motorlab_read_parameters';values={'queries':args.queries}
    if args.command == 'job': values={'job_id':args.job_id}
    if args.command == 'call':
        tool=args.tool if args.tool.startswith('motorlab_') else 'motorlab_'+args.tool
        values=json.loads(args.arguments)
    if tool not in NAMES: parser.error('Ferramenta desconhecida: '+tool)
    bridge=Bridge(args.socket,args.executable,not args.no_launch,backend='cli')
    response=bridge.call(tool,values)
    if args.wait and response.get('ok') and response.get('result',{}).get('job_id'):
        ident=response['result']['job_id']; deadline=time.monotonic()+args.timeout
        while True:
            response=bridge.call('motorlab_job',{'job_id':ident})
            if not response.get('ok') or response['result']['state']!='running': break
            if time.monotonic() >= deadline:
                # Never retry a mutation or silently stop/expire an unrelated job.
                response={'ok':False,'error':'Espera expirou; o job pode continuar. Consulte job/status.', 'job_id':ident};break
            time.sleep(.05)
    print(json.dumps(response,ensure_ascii=False,indent=2,allow_nan=False))
    return 0 if response.get('ok') and response.get('result',{}).get('state')!='failed' else 1
if __name__=='__main__':
    try: sys.exit(main())
    except (OSError,RuntimeError,ValueError) as exc:
        print(json.dumps({'ok':False,'error':str(exc)},ensure_ascii=False),file=sys.stderr);sys.exit(1)
