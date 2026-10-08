#!/usr/bin/env python3
"""Headless terminal integration: only demo, private sockets and temporary files."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'bin/motorlabd'

class CliTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='motorlab-cli-')
        self.path = Path(self.temp.name)
        self.env = os.environ.copy()
        for key in ['DISPLAY','WAYLAND_DISPLAY','MOTORLAB_COMPILER']:
            self.env.pop(key,None)
        # Any accidental QGuiApplication dependency fails with this platform.
        self.env['QT_QPA_PLATFORM'] = 'nonexistent-motorlab-test-platform'
        if EXE.parent != ROOT/'bin':
            self.env['MOTORLAB_COMPILER'] = str(ROOT/'libexec/compiler/motorlab-compiler')
        self.pid = None
        state = self.cli('status')['result']
        self.pid = state['pid']
        self.assertEqual(state['backend'],'cli')
        self.assertFalse(state['connected'])

    def tearDown(self):
        if self.pid:
            try:
                os.kill(self.pid,signal.SIGTERM)
                for _ in range(100):
                    if not (self.path/'control.sock').exists(): break
                    time.sleep(.02)
                self.assertFalse((self.path/'control.sock').exists())
            except ProcessLookupError:
                pass
        self.temp.cleanup()

    def cli(self,*args,expected=0):
        p = subprocess.run([str(ROOT/'motorlabctl'),'--socket',str(self.path/'control.sock'),
                            '--executable',str(EXE),*args],capture_output=True,text=True,env=self.env,timeout=40)
        self.assertEqual(p.returncode,expected,p.stdout+p.stderr)
        return json.loads(p.stdout or p.stderr)

    def call(self,name,args,expected=0,wait=False):
        return self.cli(*(['--wait'] if wait else []),'call',name,'--arguments',json.dumps(args),expected=expected)

    def demo(self):
        self.cli('demo')
        self.cli('--wait','query','?FID')

    def test_catalog_drafts_apply_profiles_and_atomic_rejection(self):
        self.demo()
        self.cli('--wait','read','~ALIM 1','~MMOD 1','~EPPR 1')
        self.assertEqual(self.call('catalog',{'limit':1})['result']['total'],638)
        self.call('stage',{'changes':[{'query':'~ALIM 1','raw':133}]})
        self.call('apply',{},wait=True)
        alim=self.call('parameters',{'search':'ALIM 1'})['result']['entries'][0]
        self.assertEqual(alim['raw'],133)
        self.assertFalse(alim['dirty'])
        profile=str(self.path/'profile.json')
        self.call('profile',{'action':'export_json','path':profile})
        obj=json.loads(Path(profile).read_text())
        self.assertEqual(obj['schema'],1)
        self.assertEqual(obj['source'],'demo')
        invalid=dict(obj)
        invalid['parameters']=[{'query':'~ALIM 1','raw':134},{'query':'~EPPR 1','raw':4294967296}]
        bad=self.path/'bad.json';bad.write_text(json.dumps(invalid))
        self.call('profile',{'action':'import_json','path':str(bad)},expected=1)
        self.assertFalse(self.call('parameters',{'search':'ALIM 1'})['result']['entries'][0]['dirty'])
        cpr=str(self.path/'profile.cpr')
        self.call('profile',{'action':'export_cpr','path':cpr})
        self.assertTrue(self.call('profile',{'action':'inspect_cpr','path':cpr})['result']['entries'])
        self.cli('disconnect')
        other='SBLG2360T' if obj['model']=='SBLMG2360T' else 'SBLMG2360T'
        self.call('view',{'model':other})
        self.call('profile',{'action':'import_json','path':profile},expected=1)
        self.call('view',{'model':obj['model']})
        self.call('profile',{'action':'import_json','path':profile})

    def test_stream_csv_script_and_explicit_view_error(self):
        self.demo()
        self.call('stream',{'action':'start','interval_ms':20})
        time.sleep(.12)
        data=self.call('telemetry',{'limit':1})['result']['samples']
        self.assertTrue(data)
        self.assertEqual(data[-1]['rpm1_encoder'],810)
        self.assertIsNone(data[-1]['rpm1_hall'])
        self.call('stream',{'action':'stop'})
        self.call('profile',{'action':'export_csv','path':str(self.path/'telemetry.csv')})
        self.assertIn('rpm1_encoder', (self.path/'telemetry.csv').read_text())
        self.call('profile',{'action':'export_log','path':str(self.path/'traffic.log')})
        self.call('view',{'visible':True},expected=1)
        self.call('view',{'screenshot_path':str(self.path/'image.png')},expected=1)
        self.assertFalse((self.path/'image.png').exists())
        job=self.call('script',{'action':'simulate','source':'print(42)\n'},wait=True)['result']
        self.assertTrue(job['success'])
        self.assertIn('42',job['result']['simulation']['output'])

    def test_errors_exit_nonzero_and_failed_job_is_observed(self):
        self.cli('query','!G 1 100',expected=1)
        self.call('stage',{'changes':[{'query':'~ALIM 1','raw':True}]},expected=1)
        self.demo()
        job=self.call('script',{'action':'compile','source':'this is not MicroBasic @@@'},wait=True,expected=1)
        self.assertEqual(job['result']['state'],'failed')
        state=self.cli('status')['result']
        self.assertEqual(state['pid'],self.pid)
        self.assertFalse(state['active_job'])

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--executable',type=Path,default=EXE)
    args,extra=parser.parse_known_args()
    EXE=args.executable.absolute()
    unittest.main(argv=[__file__,*extra],verbosity=2)
