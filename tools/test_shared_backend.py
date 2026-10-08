#!/usr/bin/env python3
"""GUI gateway and autostart integration, private sockets and demo only."""
import argparse
import concurrent.futures
import importlib.util
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('motorlab_mcp', ROOT / 'mcp/server.py')
server = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(server)
BUILD = ROOT / 'bin'

class SharedTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='motorlab-shared-')
        self.path = Path(self.temp.name)
        self.env = os.environ | {'QT_QPA_PLATFORM': 'offscreen'}
        self.processes = []
        self.daemon_pid = None
        self.log = open(self.path/'process.log', 'wb')

    def tearDown(self):
        for proc in reversed(self.processes):
            if proc.poll() is None:
                proc.terminate()
            proc.wait(timeout=5)
        if self.daemon_pid and not any(p.pid == self.daemon_pid for p in self.processes):
            try:
                os.kill(self.daemon_pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        self.log.close()
        self.temp.cleanup()

    def spawn(self, binary, *args):
        proc = subprocess.Popen([str(BUILD/binary), *map(str,args)], env=self.env,
                                stdin=subprocess.DEVNULL, stdout=self.log, stderr=self.log)
        self.processes.append(proc)
        return proc

    def ready(self, path, proc=None):
        bridge = server.Bridge(path, launch=False)
        until = time.monotonic()+8
        while time.monotonic() < until:
            if proc and proc.poll() is not None:
                raise AssertionError((self.path/'process.log').read_text())
            try:
                result = bridge.exchange('motorlab_status', {})
                self.assertTrue(result['ok'], result)
                return bridge, result['result']
            except (FileNotFoundError, ConnectionRefusedError):
                time.sleep(.03)
        self.fail('Endpoint não iniciou')

    def result(self, bridge, method, **args):
        reply = bridge.exchange('motorlab_'+method,args)
        self.assertTrue(reply['ok'],reply)
        return reply['result']

    def test_gui_gateway_and_restart_reuse_daemon(self):
        path=self.path/'control.sock'
        daemon=self.spawn('motorlabd','--control-socket',path)
        cli,status=self.ready(path,daemon)
        self.assertFalse(status['connected'])
        self.result(cli,'connect',mode='demo')
        gui_path=self.path/'gui.sock'
        window=self.spawn('motorlab','--backend-socket',path,'--control-socket',gui_path,'--hidden')
        gui,status=self.ready(gui_path,window)
        self.assertEqual(status['pid'],daemon.pid)
        self.assertEqual(status['backend'],'cli')
        self.assertEqual(status['version'],'0.8.0')
        self.assertTrue(status['connected'])
        self.assertFalse(status['motion_enabled'])
        self.result(cli,'stage',changes=[{'query':'~MAC 1','raw':30000}])
        params=self.result(gui,'parameters',search='MAC 1')['entries']
        self.assertEqual(params[0]['draft_raw'],30000)
        self.assertTrue(params[0]['dirty'])
        time.sleep(.4)
        image=self.path/'shared.png'
        self.result(gui,'view',page=0,screenshot_path=str(image))
        self.assertEqual(image.read_bytes()[:8],b'\x89PNG\r\n\x1a\n')
        window.terminate();window.wait(timeout=5)
        gui_path.unlink(missing_ok=True)
        self.assertTrue(self.result(cli,'status')['connected'])
        window=self.spawn('motorlab','--backend-socket',path,'--control-socket',gui_path,'--hidden')
        gui,status=self.ready(gui_path,window)
        self.assertEqual(status['pid'],daemon.pid)
        self.result(gui,'disconnect')
        self.assertFalse(self.result(cli,'status')['connected'])

    def test_two_windows_autostart_one_disconnected_daemon(self):
        backend=self.path/'backend/control.sock'
        paths=[self.path/'first.sock',self.path/'second.sock']
        windows=[self.spawn('motorlab','--backend-socket',backend,'--control-socket',p,'--hidden') for p in paths]
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            responses=list(pool.map(lambda args:self.ready(*args),zip(paths,windows)))
        statuses=[item[1] for item in responses]
        self.daemon_pid=statuses[0]['pid']
        self.assertEqual(statuses[0]['pid'],statuses[1]['pid'])
        self.assertFalse(statuses[0]['connected']);self.assertFalse(statuses[1]['connected'])
        self.assertEqual(self.result(responses[0][0],'status')['gui_clients'],2)
        for window in windows:
            window.terminate();window.wait(timeout=5)
        bridge,status=self.ready(backend)
        self.assertFalse(status['connected'])
        self.assertFalse(status['motion_enabled'])

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--build-dir',type=Path,default=BUILD)
    args,extra=parser.parse_known_args();BUILD=args.build_dir.absolute()
    unittest.main(argv=[__file__,*extra],verbosity=2)
