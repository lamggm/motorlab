#!/usr/bin/env python3
"""Real stdio/Unix-socket integration; demo only, no hardware or global config."""
import argparse
import concurrent.futures
import signal
import importlib.util
import json
import os
from pathlib import Path
import select
import socket
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('motorlab_mcp', ROOT / 'mcp/server.py')
server = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(server)
EXE = ROOT / 'bin/motorlabd'

class Client:
    def __init__(self, path):
        self.proc = subprocess.Popen([sys.executable, str(ROOT/'mcp/server.py'), '--socket',str(path),'--no-launch'], stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        self.id = 0
    def send(self, value):
        self.proc.stdin.write(json.dumps(value).encode()+b'\n'); self.proc.stdin.flush()
    def read(self):
        if not select.select([self.proc.stdout],[],[],12)[0]: raise RuntimeError('MCP response timeout')
        line = self.proc.stdout.readline()
        if not line: raise RuntimeError('MCP exited: '+self.proc.stderr.read().decode())
        return json.loads(line)
    def rpc(self, method, params=None):
        self.id+=1; self.send({'jsonrpc':'2.0','id':self.id,'method':method,'params':params or {}})
        response=self.read(); assert response['id']==self.id; return response
    def initialize(self):
        r=self.rpc('initialize',{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'test','version':'1'}})
        self.send({'jsonrpc':'2.0','method':'notifications/initialized'}); return r
    def tool(self,name,**args): return self.rpc('tools/call',{'name':'motorlab_'+name,'arguments':args})['result']
    def close(self):
        self.proc.stdin.close()
        try: self.proc.wait(timeout=3)
        except subprocess.TimeoutExpired: self.proc.kill(); self.proc.wait()
        self.proc.stdout.close(); self.proc.stderr.close()

class McpTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='motorlab-mcp-')
        self.path=Path(self.temp.name)/'control.sock'
        env=os.environ|{'QT_QPA_PLATFORM':'offscreen','MOTORLAB_COMPILER':str(ROOT/'libexec/compiler/motorlab-compiler')}
        self.log=open(Path(self.temp.name)/'app.log','wb')
        self.app=subprocess.Popen([str(EXE),'--hidden','--control-socket',str(self.path)],env=env,stdin=subprocess.DEVNULL,stdout=self.log,stderr=self.log)
        for _ in range(100):
            if self.path.exists():break
            if self.app.poll() is not None:raise RuntimeError('App failed')
            time.sleep(.05)
        self.client=Client(self.path);self.client.initialize()
    def tearDown(self):
        self.client.close();self.app.terminate();self.app.wait(timeout=5);self.log.close();self.temp.cleanup()
    def tool(self,name,**args):
        r=self.client.tool(name,**args)
        self.assertFalse(r.get('isError'),r)
        return r['structuredContent']
    def job(self,r):
        until=time.monotonic()+45
        while time.monotonic()<until:
            j=self.tool('job',job_id=r['job_id'])
            if j['state']!='running':self.assertTrue(j['success'],j);return j
            time.sleep(.04)
        self.fail('Job timeout')
    def demo(self):
        self.tool('connect',mode='demo')
        for _ in range(100):
            if self.tool('status')['model']:return
            time.sleep(.02)
        self.fail('Demo identity timeout')
    def test_autostart_reuses_one_instance_and_never_connects(self):
        path=Path(self.temp.name)/'auto/control.sock'
        bridge=server.Bridge(path,executable=EXE,offscreen=True)
        pid=None
        try:
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                a,b=list(pool.map(lambda _: bridge.call('motorlab_status',{}),range(2)))
            self.assertTrue(a['ok']);self.assertTrue(b['ok'])
            pid=a['result']['pid']
            self.assertEqual(pid,b['result']['pid'])
            self.assertFalse(a['result']['connected'])
            self.assertFalse(b['result']['connected'])
        finally:
            if bridge.spawned_process is not None:
                bridge.spawned_process.terminate()
                bridge.spawned_process.wait(timeout=5)
    def test_discovery_and_validation(self):
        tools=self.client.rpc('tools/list')['result']['tools']
        self.assertEqual(len(tools),25)
        guards = next(t for t in tools if t["name"] == "motorlab_continuous")["inputSchema"]["properties"]
        self.assertEqual(guards["max_current_a"]["type"], "number")
        self.assertEqual(guards["max_current_a"]["default"], 11.5)
        self.assertEqual(guards["max_speed_rpm"]["default"], 3000)
        self.assertEqual(guards["max_temperature_c"]["default"], 85)
        self.assertFalse(self.tool('status')['connected'])
        self.assertIn('firmware_can',self.tool('status')['unavailable'])
        self.assertTrue(self.client.tool('connect',mode='serial',baud=12345)['isError'])
        self.assertTrue(self.client.tool('stage',changes='bad')['isError'])
        self.assertIn('error',self.client.rpc('tools/call',{'name':'invented'}))
        self.assertIn('error',self.client.rpc('undefined'))
        self.client.proc.stdin.write(b'not JSON\n');self.client.proc.stdin.flush()
        self.assertEqual(self.client.read()['error']['code'],-32700)
    def test_continuous_client_loss_stops(self):
        self.demo()
        self.job(self.tool('read_parameters',queries=['~MMOD 1']))
        self.tool('stage',changes=[{'query':'~MMOD 1','raw':0}])
        self.job(self.tool('apply'))
        owner=Client(self.path)
        try:
            owner.initialize()
            r=owner.tool('continuous',action='start',channel=1,value=100,lease_ms=1500,release=True)
            self.assertFalse(r.get('isError'),r)
            started=r['structuredContent']
        finally:
            owner.close()
        until=time.monotonic()+5
        while time.monotonic()<until:
            result=self.tool('job',job_id=started['job_id'])
            if result['state']!='running':break
            time.sleep(.05)
        self.assertEqual(result['state'],'failed')
        self.assertIn('expirou',result['message'])
        self.assertTrue(result['result']['stop_confirmed'])
        self.assertFalse(self.tool('status')['motion_enabled'])
    def test_continuous_supervisor_stops_on_duration(self):
        self.demo()
        self.job(self.tool('read_parameters',queries=['~MMOD 1']))
        self.tool('stage',changes=[{'query':'~MMOD 1','raw':0}])
        self.job(self.tool('apply'))
        state=self.tool('status')
        r=subprocess.run([sys.executable,str(ROOT/'tools/continuous_supervisor.py'),
                          '--socket',str(self.path),'--channel','1','--value','100',
                          '--expected-model',state['model'],'--expected-firmware',state['firmware'],
                          '--release','--confirm-motion','--seconds','1.5'],
                          capture_output=True,text=True,timeout=12)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('"stop_confirmed": true',r.stdout)
        self.assertFalse(self.tool('status')['motion_enabled'])
    def test_two_clients_share_gui_and_ram(self):
        self.demo();other=Client(self.path)
        try:
            other.initialize(); self.assertTrue(other.tool('status')['structuredContent']['demo'])
            self.job(self.tool('read_parameters',queries=['~ALIM 1']))
            self.tool('stage',changes=[{'query':'~ALIM 1','raw':133}])
            shared=other.tool('parameters',search='ALIM 1')['structuredContent']['entries'][0]
            self.assertEqual(shared['draft_raw'],133)
            self.job(self.tool('apply'))
            shared=other.tool('parameters',search='ALIM 1')['structuredContent']['entries'][0]
            self.assertEqual(shared['raw'],133);self.assertFalse(shared['dirty'])
            self.assertIn('^ALIM 1 133',self.tool('log')['text'])
        finally:other.close()
    def test_compilation_profiles_and_screenshot(self):
        self.demo()
        j=self.job(self.tool('script',action='simulate',source='print(42)\n'))
        self.assertIn('42',j['result']['simulation']['output'])
        self.job(self.tool('read_parameters',queries=['~ALIM 1']))
        p=str(Path(self.temp.name)/'backup.cpr')
        self.tool('profile',action='export_cpr',path=p)
        parsed=self.tool('profile',action='inspect_cpr',path=p)
        self.assertTrue(parsed['entries'])
        self.assertTrue(self.client.tool('profile',action='export_cpr',path=p)['isError'])
        png=str(Path(self.temp.name)/'ui.png')
        if self.tool('status').get('backend') == 'cli':
            self.assertTrue(self.client.tool('view',page=0,screenshot_path=png)['isError'])
            self.assertFalse(Path(png).exists())
        else:
            self.tool('view',page=0,screenshot_path=png)
            self.assertEqual(Path(png).read_bytes()[:8],b'\x89PNG\r\n\x1a\n')
    def test_full_catalog_and_bulk_reads(self):
        self.demo();self.assertEqual(self.tool('catalog',limit=1)['total'],638)
        result=self.job(self.tool('read_parameters'))
        self.assertEqual(len(result['replies']),638)
        self.assertTrue(self.tool('parameters',limit=1)['entries'][0]['readable'])
    def test_socket_private_and_bad_messages(self):
        self.assertEqual(self.path.parent.stat().st_mode & 0o077,0)
        self.assertEqual(self.path.stat().st_mode & 0o077,0)
        with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as c:
            c.connect(str(self.path));c.sendall(b'{}\n');r=json.loads(c.recv(4096));self.assertFalse(r['ok'])
        bridge=server.Bridge(self.path,launch=False)
        self.assertFalse(bridge.call('motorlab_status',{'bogus':42})['ok'])

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--executable',type=Path,default=EXE)
    args,extra=parser.parse_known_args();EXE=args.executable.absolute()
    unittest.main(argv=[sys.argv[0],*extra],verbosity=2)
