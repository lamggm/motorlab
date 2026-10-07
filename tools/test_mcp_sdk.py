#!/usr/bin/env python3
"""Optional interoperability test with official mcp==1.26.0 client SDK."""
import asyncio
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
ROOT=Path(__file__).resolve().parents[1]
EXE=Path(sys.argv[1]).absolute() if len(sys.argv)>1 else ROOT/'bin/motorlab'

async def main():
    with tempfile.TemporaryDirectory(prefix='motorlab-sdk-') as d:
        sock=Path(d)/'control.sock'
        env=os.environ|{'QT_QPA_PLATFORM':'offscreen','MOTORLAB_COMPILER':str(ROOT/'libexec/compiler/motorlab-compiler')}
        with open(Path(d)/'app.log','wb') as log:
            app=subprocess.Popen([str(EXE),'--hidden','--control-socket',str(sock)],env=env,stdout=log,stderr=log)
            try:
                for _ in range(100):
                    if sock.exists():break
                    await asyncio.sleep(.05)
                params=StdioServerParameters(command=sys.executable,args=[str(ROOT/'mcp/server.py'),'--socket',str(sock),'--no-launch'])
                async with stdio_client(params) as (reader,writer):
                    async with ClientSession(reader,writer) as session:
                        initialized=await session.initialize()
                        assert initialized.serverInfo.name=='motorlab'
                        tools=await session.list_tools()
                        assert len(tools.tools)==25
                        state=await session.call_tool('motorlab_status',{})
                        assert not state.isError and not state.structuredContent['connected']
                        state=await session.call_tool('motorlab_connect',{'mode':'demo'})
                        assert not state.isError
                        result=await session.call_tool('motorlab_script',{'action':'simulate','source':'print(42)\n'})
                        assert not result.isError
                        jid=result.structuredContent['job_id']
                        for _ in range(200):
                            result=await session.call_tool('motorlab_job',{'job_id':jid})
                            if result.structuredContent['state']!='running':break
                            await asyncio.sleep(.05)
                        assert result.structuredContent['success']
                        assert '42' in result.structuredContent['result']['simulation']['output']
                        invalid=await session.call_tool('motorlab_query',{'command':'!G 1 100'})
                        assert invalid.isError
                        print(f'Official SDK: protocol={initialized.protocolVersion}, tools={len(tools.tools)}, initialize/list/status/demo/compiler/job/error OK')
            finally:
                app.terminate();app.wait(timeout=5)
asyncio.run(main())
