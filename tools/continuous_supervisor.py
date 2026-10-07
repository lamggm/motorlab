#!/usr/bin/env python3
"""Explicit operator supervisor. Never autoconnect, restart motion, or disable RWD."""
import argparse
import importlib.util
import json
from pathlib import Path
import signal
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('motorlab_bridge', ROOT/'mcp/server.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--socket')
    parser.add_argument('--channel', type=int, choices=[1, 2], required=True)
    parser.add_argument('--mode', choices=['open_loop', 'encoder_speed'], default='open_loop')
    parser.add_argument('--value', type=int, required=True, help='G bruto -100..100 em open_loop; RPM -3000..3000 em encoder_speed')
    parser.add_argument('--expected-model', required=True)
    parser.add_argument('--expected-firmware', required=True)
    parser.add_argument('--max-current-a', type=int, default=10)
    parser.add_argument('--max-speed-rpm', type=int, default=600)
    parser.add_argument('--max-power-percent', type=int, default=25)
    parser.add_argument('--max-temperature-c', type=int, default=60)
    parser.add_argument('--release', action='store_true', help='Explicitly allow !MG after preflight')
    parser.add_argument('--confirm-motion', action='store_true', required=True)
    duration = parser.add_mutually_exclusive_group(required=True)
    duration.add_argument('--seconds', type=float)
    duration.add_argument('--until-stopped', action='store_true')
    args = parser.parse_args()
    if args.seconds is not None and not 0 < args.seconds <= 3600:
        parser.error('--seconds must be positive and at most 3600')
    bridge = module.Bridge(path=args.socket, launch=False)
    stopped = False
    def interrupt(*_):
        nonlocal stopped
        stopped = True
    signal.signal(signal.SIGINT, interrupt)
    signal.signal(signal.SIGTERM, interrupt)
    def call(name, **kw):
        r = bridge.call('motorlab_'+name, kw)
        if not r.get('ok'):
            raise RuntimeError(r.get('error', str(r)))
        return r['result']
    st = call('status')
    if not st['connected'] or st['model'] != args.expected_model or st['firmware'] != args.expected_firmware:
        raise RuntimeError('Connect and verify the intended controller first')
    print('Starting supervised '+args.mode+' command; stop with Ctrl+C or GUI PARADA !EX.', flush=True)
    # A lost start response may still mean motion began. Finally always attempts
    # stop; the Qt-side lease expires independently even if this process dies.
    terminal = False
    try:
        start = call('continuous', action='start', mode=args.mode, channel=args.channel, value=args.value,
                     lease_ms=3000, confirm=True, expected_model=args.expected_model,
                     expected_firmware=args.expected_firmware, release=args.release,
                     max_current_a=args.max_current_a, max_speed_rpm=args.max_speed_rpm,
                     max_temperature_c=args.max_temperature_c, max_power_percent=args.max_power_percent)
        begin = time.monotonic()
        last_cycle = -1
        while not stopped and (args.seconds is None or time.monotonic()-begin < args.seconds):
            job = call('job', job_id=start['job_id'])
            if job['state'] != 'running':
                print(json.dumps(job, ensure_ascii=False), flush=True)
                terminal = True
                return 0 if job.get('success') else 1
            state = call('status')['continuous']
            print(json.dumps(state, ensure_ascii=False), flush=True)
            if state['stopping']:
                time.sleep(.1)
                continue
            # Only renew for fresh cycles, or while the initial preflight is
            # progressing. A frozen monitor is not a source of keepalives.
            if state['cycles'] > last_cycle or time.monotonic()-begin < 1:
                try:
                    call('continuous', action='renew', lease_token=start['lease_token'])
                except RuntimeError:
                    # A GUI/MCP stop may race this keepalive. Do not turn an
                    # already confirmed operator stop into a client failure.
                    job = call('job', job_id=start['job_id'])
                    if job['state'] != 'running':
                        print(json.dumps(job, ensure_ascii=False), flush=True)
                        terminal = True
                        return 0 if job.get('success') else 1
                    if call('status')['continuous']['stopping']:
                        continue
                    raise
                last_cycle = state['cycles']
            time.sleep(.4)
    finally:
        if not terminal:
            result = call('stop')
            if 'job_id' in result:
                end = time.monotonic()+5
                while time.monotonic() < end:
                    result = call('job', job_id=result['job_id'])
                    if result['state'] != 'running':
                        break
                    time.sleep(.1)
            print('STOP', json.dumps(result, ensure_ascii=False), flush=True)
            confirmed = result.get('result', {}).get('stop_confirmed', False)
            if result.get('kind') == 'stop' and result.get('success'):
                replies = result.get('replies', [])
                confirmed = any(r.get('command') == '?FF' and r.get('ok') and
                                int(r.get('payload', '0')) & 16 for r in replies)
            if not confirmed:
                raise RuntimeError('Stop not confirmed; inspect controller state')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(1)
