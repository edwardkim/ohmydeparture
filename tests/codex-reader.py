#!/usr/bin/env python3
"""Read-only boundary, protocol compatibility, bounded history, and failure tests."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('codex_reader', ROOT/'platform/codex/reader.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def write_log(home, name='root', source='cli', parent=None, big=False):
    path = home/'sessions/2026/10'/f'rollout-{name}.jsonl'
    path.parent.mkdir(parents=True,exist_ok=True)
    meta = dict(id=name,cwd='/demo/project',source=source,timestamp='2026-10-25T00:00:00Z')
    if parent: meta['parent_thread_id']=parent
    records = [dict(type='session_meta',payload=meta)]
    if big: records.append(dict(type='response_item',payload=dict(type='reasoning',encrypted_content='x'*(m.TAIL*2))))
    records += [dict(type='turn_context',timestamp='2026-10-25T00:00:00Z',payload=dict(turn_id='t1')),
        dict(type='event_msg',payload=dict(type='item_completed',item=dict(type='commandExecution',id='tool1',command='printf demo',status='completed',exitCode=0,aggregatedOutput='<b>literal</b>'))),
        dict(type='event_msg',payload=dict(type='task_complete'))]
    path.write_text(''.join(json.dumps(r)+'\n' for r in records))
    return path


class ReaderTests(unittest.TestCase):
    def test_cli_codex_modes_and_errors(self):
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp)/'codex.png'
            command=[str(ROOT/'ohmydeparture'),'--snapshot',str(out),'--wait','0']
            env=dict(os.environ,CODEX_HOME=tmp,XDG_CONFIG_HOME=tmp)
            for args in [ ['--view','invalid'], ['--view','codex','--size','480x240'],
                          ['--view','departures','--codex-fixture',str(ROOT/'tests/codex-fixture.json')],
                          ['--view','codex','--airport','ICN'],
                          ['--view','codex','--codex-fixture',str(Path(tmp)/'missing.json')] ]:
                r=subprocess.run(command+args,env=env,capture_output=True,timeout=5)
                self.assertEqual(r.returncode,2,(args,r.stderr));self.assertFalse(out.exists())
            r=subprocess.run(command+['--view','codex','--codex-fixture',str(ROOT/'tests/codex-fixture.json')],env=env,capture_output=True,timeout=5)
            self.assertEqual(r.returncode,0,r.stderr)
            self.assertEqual(struct.unpack('>II',out.read_bytes()[16:24]),(1600,900))
            self.assertFalse(list(Path(tmp).rglob('*.conf')))

    def test_status_and_lineage(self):
        for raw,expected in [('active','running'),('idle','idle'),('notLoaded','history'),('systemError','error'),('newState','unknown')]:
            t=dict(id='a',status=dict(type=raw),source={'subAgent':{'thread_spawn':{'parent_thread_id':'p','agent_nickname':'Ada'}}})
            r=m.normalize_thread(t)
            self.assertEqual(r['status'],expected);self.assertEqual(r['parentId'],'p');self.assertEqual(r['agent'],'Ada')
            self.assertEqual(m.normalize_thread(t,False)['status'],'history')
        for flag,status in [('waitingOnApproval','approval'),('waitingOnUserInput','input')]:
            self.assertEqual(m.normalize_thread({'status':{'type':'active','activeFlags':[flag]}})['status'],status)
        self.assertEqual(m.normalize_thread({'source':{'subagent':{'other':'guardian'}}})['source'],'subAgent')

    def test_local_bounded_read_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            home=Path(tmp);path=write_log(home,big=True)
            write_log(home,'child',{'subagent':{'thread_spawn':{'parent_thread_id':'root','agent_role':'reviewer'}}})
            (home/'session_index.jsonl').write_text(json.dumps(dict(id='root',thread_name='한글 세션'))+'\n')
            before={p:(p.stat().st_mtime_ns,p.stat().st_size) for p in home.rglob('*') if p.is_file()}
            reader=m.Reader(home);result=reader.sessions({})
            self.assertEqual(result['source'],'local');self.assertTrue(all(t['status']=='history' for t in result['sessions']))
            self.assertEqual(next(t for t in result['sessions'] if t['id']=='root')['title'],'한글 세션')
            h=reader.history(dict(threadId='root',source='local'))
            self.assertIn('1 MiB',h['notice']);self.assertEqual(h['turns'][0]['status'],'completed')
            self.assertIn('<b>literal</b>',h['turns'][0]['items'][0]['text'])
            self.assertLess(len(json.dumps(h)),2000)
            self.assertEqual(before,{p:(p.stat().st_mtime_ns,p.stat().st_size) for p in home.rglob('*') if p.is_file()})
            with path.open('ab') as f:f.write(b'{"partial":')
            self.assertTrue(reader.local_history('root')['turns'])
            with self.assertRaises(ValueError):reader.local_history('../../auth')

    def test_pagination_archived_and_schema_items(self):
        with tempfile.TemporaryDirectory() as tmp,patch.object(m,'PAGE',2):
            home=Path(tmp)
            for name in ['a','b','c']:write_log(home,name)
            reader=m.Reader(home);a=reader.sessions({});self.assertEqual(len(a['sessions']),2)
            b=reader.sessions(dict(source='local',cursor=a['nextCursor']));self.assertEqual(len(b['sessions']),1)
            self.assertFalse(set(t['id'] for t in a['sessions']) & set(t['id'] for t in b['sessions']))
            self.assertFalse(reader.sessions({'archived':True})['sessions'])
            self.assertEqual(len(reader.local_history('a')['turns']),1,'metadata must not create a fictitious turn')
            for kind in ['collabAgentToolCall','collabToolCall','subAgentActivity','fileChange','mcpToolCall']:
                self.assertIsNotNone(m.item_summary(dict(type=kind)))
            self.assertIsNone(m.item_summary(dict(type='reasoning',content=['secret'])))
            self.assertIsNone(m.item_summary(dict(type='Reasoning',raw_content=['secret'])))
            self.assertIn('exit: 7',m.item_summary(dict(type='CommandExecution',command='false',exit_code=7,aggregated_output='failed'))['text'])
            self.assertEqual(m.item_summary(dict(type='AgentMessage',content=[dict(type='text',text='응답')]))['text'],'응답')
            self.assertEqual(m.item_summary(dict(type='FileChange',changes={'a.cpp':{}}))['text'],'a.cpp')

    def test_real_stdio_protocol_allowlist_and_no_approval_reply(self):
        with tempfile.TemporaryDirectory() as tmp:
            base=Path(tmp);log=base/'wire.jsonl';binary=base/'codex'
            binary.write_text(f'''#!{sys.executable}
import sys,json
for line in sys.stdin:
 r=json.loads(line)
 with open({str(log)!r},'a') as f:f.write(line)
 method=r.get('method')
 if method=='initialized':continue
 if method=='initialize':
  print(json.dumps({{"id":900,"method":"item/commandExecution/requestApproval","params":{{}}}}),flush=True)
  result={{"userAgent":"fixture"}}
 elif method=='thread/list':result={{"data":[{{"id":"x","name":"fixture","status":{{"type":"idle"}}}}],"nextCursor":"page-2"}}
 elif method=='thread/turns/list':result={{"data":[{{"id":"t","status":"interrupted","items":[{{"id":"i","type":"agentMessage","text":"fixture"}}]}}],"nextCursor":None}}
 else:raise SystemExit('unexpected mutating request')
 print(json.dumps({{"id":r['id'],"result":result}}),flush=True)
''')
            binary.chmod(0o755)
            with patch.dict(os.environ,{'PATH':str(base)+os.pathsep+os.environ['PATH']}),patch.object(Path,'is_socket',return_value=True):
                reader=m.Reader(base);a=reader.sessions({});self.assertEqual(a['source'],'appServer');self.assertEqual(a['sessions'][0]['status'],'idle')
                b=reader.sessions(dict(source='appServer',cursor='page-2'));self.assertEqual(b['nextCursor'],'page-2')
                h=reader.history(dict(threadId='x',source='appServer'));self.assertEqual(h['turns'][0]['status'],'interrupted')
                with self.assertRaises(m.RpcError):reader.proxy.call('turn/start',{})
                reader.proxy.close()
            calls=[json.loads(s) for s in log.read_text().splitlines()]
            self.assertEqual([r['method'] for r in calls],['initialize','initialized','thread/list','thread/list','thread/turns/list'])
            self.assertTrue(all('jsonrpc' not in r for r in calls))
            self.assertTrue(all(r['params']['useStateDbOnly'] for r in calls if r['method']=='thread/list'))
            self.assertIn('subAgent',calls[2]['params']['sourceKinds'])
            self.assertEqual(calls[3]['params']['cursor'],'page-2')
            self.assertEqual(calls[-1]['params']['itemsView'],'full')

    def test_disconnect_and_unsupported_history_fallback(self):
        with tempfile.TemporaryDirectory() as tmp:
            reader=m.Reader(Path(tmp));write_log(Path(tmp))
            reader.proxy.connect=lambda:None
            reader.proxy.call=lambda *a: (_ for _ in ()).throw(m.RpcError('unsupported'))
            self.assertEqual(reader.sessions({})['source'],'local')
            self.assertEqual(reader.history(dict(threadId='root',source='appServer'))['source'],'local')
            proc=subprocess.run([sys.executable,str(ROOT/'platform/codex/reader.py'),'--home',tmp],
                input='{"id":7,"method":"turn/start"}\n{"id":8,"method":"sessions"}\n',text=True,capture_output=True,timeout=5)
            responses=[json.loads(s) for s in proc.stdout.splitlines()]
            self.assertIn('read-only',responses[0]['error']);self.assertEqual(responses[1]['id'],8)


if __name__=='__main__':unittest.main(verbosity=2)
