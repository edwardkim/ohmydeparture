#!/usr/bin/env python3
"""Read-only Codex board adapter. JSONL stdin/stdout; no hooks or database writes.

Prefer the *existing* daemon (never start/resume a thread). Fall back to bounded
rollout reads when it is absent or incompatible. Local records carry no live state.
"""
import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import selectors
import shutil
import subprocess
import sys
import time

PAGE = 100
TAIL = 1024 * 1024
SOURCES = ['cli', 'vscode', 'exec', 'appServer', 'subAgent', 'subAgentReview',
           'subAgentCompact', 'subAgentThreadSpawn', 'subAgentOther', 'unknown']
READ_METHODS = frozenset(['initialize', 'thread/list', 'thread/turns/list'])


def text(value, limit=12000):
    if not isinstance(value, str):
        value = json.dumps(value, ensure_ascii=False) if value is not None else ''
    # Terminal escape/control sequences must never become rich text or commands.
    return ''.join(c for c in value if c >= ' ' or c in '\n\t')[:limit]


def stamp(value):
    if isinstance(value, (int, float)):
        return int(value)
    try:
        return int(datetime.fromisoformat(str(value).replace('Z', '+00:00')).timestamp())
    except (ValueError, TypeError):
        return 0


def source_info(source):
    if isinstance(source, str):
        return source, {}
    source = source if isinstance(source, dict) else {}
    sub = source.get('subAgent', source.get('subagent'))
    if isinstance(sub, dict):
        return 'subAgent', sub.get('thread_spawn', {})
    return ('subAgent' if sub else 'unknown'), {}


def normalize_thread(raw, live=True):
    source, spawn = source_info(raw.get('source'))
    state = raw.get('status') or {}
    kind = state.get('type') if isinstance(state, dict) else state
    flags = state.get('activeFlags', []) if isinstance(state, dict) else []
    status = 'history'
    if live:
        status = {'active': 'running', 'idle': 'idle', 'systemError': 'error',
                  'notLoaded': 'history'}.get(kind, 'unknown')
        if kind == 'active':
            if 'waitingOnApproval' in flags: status = 'approval'
            elif 'waitingOnUserInput' in flags: status = 'input'
    cwd = text(raw.get('cwd'), 1000)
    thread_id = text(raw.get('id'), 200)
    fallback_title = ('Subagent' if source == 'subAgent' else 'Untitled session')
    if thread_id: fallback_title += ' · '+thread_id[-8:]
    return dict(id=thread_id, parentId=text(raw.get('parentThreadId') or
                raw.get('parent_thread_id') or spawn.get('parent_thread_id'), 200),
                title=text(raw.get('name') or raw.get('preview') or raw.get('agentNickname')
                           or fallback_title, 240).replace('\n', ' '),
                agent=text(raw.get('agentNickname') or raw.get('agent_nickname') or spawn.get('agent_nickname'), 80),
                role=text(raw.get('agentRole') or raw.get('agent_role') or spawn.get('agent_role'), 80),
                project=Path(cwd).name or cwd, cwd=cwd, source=source,
                model=text(raw.get('model'), 100), status=status,
                updatedAt=stamp(raw.get('updatedAt') or raw.get('timestamp')))


def item_summary(item):
    if not isinstance(item, dict): return None
    item = dict(item)
    kind = item.get('type', 'unknown')
    # Persisted event_msg items use internal PascalCase + snake_case, whereas
    # App Server v2 exposes camelCase. Keep this compatibility at the boundary.
    if kind and kind[0].isupper(): kind = kind[0].lower()+kind[1:]
    for old, new in [('exit_code','exitCode'),('aggregated_output','aggregatedOutput'),
                     ('agents_states','agentsStates'),('agent_path','agentPath'),
                     ('agent_thread_id','agentThreadId')]:
        if old in item: item[new] = item[old]
    status = text(item.get('status'), 80)
    label = kind
    body = ''
    if kind in ('userMessage', 'message'):
        label = 'User' if item.get('role', 'user') == 'user' else 'Agent response'
        body = '\n'.join(text(c.get('text')) for c in item.get('content', []) if isinstance(c, dict))
    elif kind in ('agentMessage', 'plan'):
        label = 'Agent response' if kind == 'agentMessage' else 'Plan'
        body = text(item.get('text')) or '\n'.join(text(c.get('text')) for c in item.get('content', []) if isinstance(c, dict))
    elif kind in ('commandExecution', 'command_execution'):
        label = 'Command'
        body = text(item.get('command'))
        if item.get('exitCode') is not None: body += '\nexit: ' + str(item['exitCode'])
        body += '\n' + text(item.get('aggregatedOutput'))
    elif kind in ('mcpToolCall', 'dynamicToolCall', 'function_call', 'custom_tool_call'):
        label = text(item.get('tool') or item.get('name') or kind, 100)
        body = text(item.get('arguments') or item.get('input'))
        if item.get('error'): body += '\n' + text(item['error'])
    elif kind in ('functionCallOutput', 'function_call_output', 'custom_tool_call_output'):
        label = 'Tool output'
        body = text(item.get('output'))
    elif kind in ('collabAgentToolCall', 'collabToolCall'):
        label = 'Agent · ' + text(item.get('tool'), 80)
        body = text(item.get('prompt')) + '\n' + text(item.get('agentsStates'))
    elif kind == 'subAgentActivity':
        label = 'Subagent · ' + text(item.get('kind'), 80)
        body = text(item.get('agentPath')) + '\n' + text(item.get('agentThreadId'))
    elif kind == 'fileChange':
        label = 'File changes'
        changes = item.get('changes', [])
        body = '\n'.join(text(c,1000) for c in changes) if isinstance(changes,dict) else '\n'.join(text(c.get('path'),1000) for c in changes if isinstance(c,dict))
    elif kind == 'webSearch':
        label, body = 'Web search', text(item.get('query'))
    elif kind == 'extension':
        label, body = 'Extension · '+text(item.get('kind'),80), text(item.get('query'))
    elif kind in ('reasoning', 'hookPrompt'):
        return None  # Do not surface internal reasoning or injected hook prompts.
    else:
        body = text(item.get('text') or item.get('summary') or item.get('kind'))
    return dict(id=text(item.get('id') or item.get('call_id'), 200), kind=kind,
                label=label, status=status, text=body.strip())


class RpcError(Exception):
    pass


class Proxy:
    def __init__(self, home):
        self.home, self.process, self.buffer, self.seq = home, None, b'', 0
        self.retry_at = 0

    def close(self):
        p, self.process = self.process, None
        if p is not None:
            p.terminate()
            try: p.wait(timeout=.5)
            except subprocess.TimeoutExpired: p.kill(); p.wait()
            for stream in (p.stdin, p.stdout):
                try: stream.close()
                except OSError: pass
        self.buffer = b''

    def connect(self):
        if self.process and self.process.poll() is None: return
        if time.monotonic() < self.retry_at: raise RpcError('daemon unavailable')
        self.close()
        sock = self.home / 'app-server-control/app-server-control.sock'
        if not sock.is_socket() or not shutil.which('codex'):
            self.retry_at = time.monotonic() + 15
            raise RpcError('daemon unavailable')
        self.process = subprocess.Popen(['codex', 'app-server', 'proxy', '--sock', str(sock)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        try:
            self.call('initialize', {'clientInfo': {'name': 'ohmydeparture_board', 'version': '1.2.0'},
                                     'capabilities': {'experimentalApi': True}})
            self.process.stdin.write(b'{"method":"initialized"}\n')
            self.process.stdin.flush()
        except (RpcError, OSError):
            self.close(); self.retry_at = time.monotonic() + 15
            raise RpcError('daemon unavailable')

    def call(self, method, params):
        if method not in READ_METHODS: raise RpcError('read-only method rejected')
        if not self.process: raise RpcError('not connected')
        self.seq += 1
        wanted = self.seq
        try:
            self.process.stdin.write((json.dumps(dict(id=wanted, method=method, params=params))+'\n').encode())
            self.process.stdin.flush()
            with selectors.DefaultSelector() as selector:
                selector.register(self.process.stdout, selectors.EVENT_READ)
                deadline = time.monotonic() + 4
                while time.monotonic() < deadline:
                    while b'\n' in self.buffer:
                        line, self.buffer = self.buffer.split(b'\n', 1)
                        try: msg = json.loads(line)
                        except (ValueError, UnicodeDecodeError): continue
                        if msg.get('id') == wanted and 'method' not in msg:
                            if 'error' in msg: raise RpcError(text(msg['error'].get('message'), 300))
                            return msg.get('result', {})
                        # No subscription and no approval handling. Never reply to server requests.
                    if not selector.select(max(0, deadline-time.monotonic())): break
                    chunk = os.read(self.process.stdout.fileno(), 65536)
                    if not chunk: raise RpcError('daemon disconnected')
                    self.buffer += chunk
                    if len(self.buffer) > 32 * 1024 * 1024: raise RpcError('response too large')
            raise RpcError('daemon timeout')
        except (OSError, RpcError):
            self.close(); self.retry_at = time.monotonic() + 15
            raise


def read_tail(path, limit=TAIL):
    with path.open('rb') as stream:
        end = os.fstat(stream.fileno()).st_size
        start = max(0, end-limit)
        stream.seek(start)
        if start: stream.readline(limit)  # discard an incomplete initial record
        data = stream.read(limit)
    # A rollout may currently be appending a line. Ignore only its incomplete end.
    if not data.endswith(b'\n'): data = data.rsplit(b'\n', 1)[0] if b'\n' in data else b''
    records = []
    for line in data.splitlines():
        try:
            record = json.loads(line)
            if isinstance(record, dict): records.append(record)
        except (ValueError, UnicodeDecodeError): pass
    return records, bool(start)


class Reader:
    def __init__(self, home):
        self.home = home
        self.proxy = Proxy(home)
        self.headers = {}
        self.paths = {}

    def local_sessions(self, archived, cursor):
        root = self.home / ('archived_sessions' if archived else 'sessions')
        paths = sorted(root.glob('**/*.jsonl'), key=lambda p: p.stat().st_mtime, reverse=True) if root.exists() else []
        titles = {}
        try:
            records, _ = read_tail(self.home/'session_index.jsonl')
            for r in records: titles[r.get('id')] = r.get('thread_name') or r.get('name')
        except OSError: pass
        offset = int(cursor or 0)
        sessions = []
        for path in paths[offset:offset+PAGE]:
            try:
                if path not in self.headers:
                    with path.open('rb') as f: line = f.readline(TAIL)
                    record = json.loads(line)
                    if record.get('type') != 'session_meta': continue
                    self.headers[path] = record.get('payload', {})
                raw = dict(self.headers[path])
                raw['name'] = titles.get(raw.get('id'))
                raw['updatedAt'] = int(path.stat().st_mtime)
                row = normalize_thread(raw, live=False)
                if not row['id']: continue
                self.paths[row['id']] = path
                sessions.append(row)
            except (OSError, ValueError, TypeError): continue
        return dict(source='local', connected=False, updatedAt=int(time.time()), sessions=sessions,
                    nextCursor=str(offset+PAGE) if len(paths)>offset+PAGE else None,
                    notice='Local history · runtime unknown · automatically connects when an existing daemon is available')

    def sessions(self, params):
        archived = bool(params.get('archived'))
        cursor = params.get('cursor')
        try:
            self.proxy.connect()
            query = dict(limit=PAGE, sortKey='updated_at', sortDirection='desc',
                         sourceKinds=SOURCES, archived=archived, useStateDbOnly=True)
            if cursor and params.get('source') == 'appServer': query['cursor'] = cursor
            result = self.proxy.call('thread/list', query)
            return dict(source='appServer', connected=True, updatedAt=int(time.time()),
                        sessions=[normalize_thread(t) for t in result.get('data', [])],
                        nextCursor=result.get('nextCursor'),
                        notice='Codex daemon · 5-second polling · runtime state is scoped to this daemon')
        except (RpcError, OSError):
            return self.local_sessions(archived, cursor if params.get('source')=='local' else None)

    def local_history(self, thread_id):
        path = self.paths.get(thread_id)
        if not path: raise ValueError('Refresh the list, then select a session.')
        records, truncated = read_tail(path)
        turns, current, canonical = [], None, any(r.get('type')=='event_msg' and
                r.get('payload', {}).get('type') in ('item_started', 'item_completed') for r in records)
        seen = set()
        for record in records:
            payload = record.get('payload', {})
            if not isinstance(payload, dict): continue
            kind, typ = record.get('type'), payload.get('type')
            turn_id = payload.get('turn_id')
            terminal = kind == 'event_msg' and typ in ('task_complete', 'task_completed',
                'turn_completed', 'turn_aborted', 'task_failed', 'turn_failed')
            if kind == 'turn_context' or (kind == 'event_msg' and typ in ('task_started', 'turn_started')):
                if current is None or (turn_id and turn_id != current['id']):
                    current = dict(id=turn_id or 'partial', status='recorded', time=stamp(record.get('timestamp')), items=[])
                    turns.append(current)
            item = None
            if kind == 'event_msg' and typ in ('item_started', 'item_completed'):
                raw = payload.get('item', {})
                item = item_summary(raw)
            elif kind == 'response_item' and (not canonical or typ in ('function_call_output','custom_tool_call_output')):
                item = item_summary(payload)
            elif kind == 'event_msg' and typ == 'user_message' and not canonical:
                item = dict(id='', kind='userMessage', label='User', status='', text=text(payload.get('message')))
            if item is None and not terminal: continue
            if current is None:
                current = dict(id=turn_id or 'partial', status='recorded', time=stamp(record.get('timestamp')), items=[])
                turns.append(current)
            if terminal:
                current['status'] = ('interrupted' if typ=='turn_aborted' else
                                     'failed' if typ in ('task_failed','turn_failed') else
                                     payload.get('status') if payload.get('status') in ('completed','failed','interrupted') else 'completed')
            if item:
                key = (item['id'], item['kind'])
                if item['id'] and key in seen:
                    for old in reversed(current['items']):
                        if (old['id'],old['kind']) == key: old.update(item);break
                    continue
                seen.add(key)
                current['items'].append(item)
                current['items'] = current['items'][-200:]
        return dict(threadId=thread_id, source='local', turns=list(reversed(turns[-30:])), nextCursor=None,
                    notice=('Partial history: latest 1 MiB · ' if truncated else 'Stored history · ')+
                    'Turn outcomes do not establish current runtime state or project completion.')

    def history(self, params):
        thread_id = text(params.get('threadId'), 200)
        if not thread_id: raise ValueError('threadId required')
        if params.get('source') == 'appServer':
            try:
                self.proxy.connect()
                query = dict(threadId=thread_id, limit=20, sortDirection='desc', itemsView='full')
                if params.get('cursor'): query['cursor'] = params['cursor']
                result = self.proxy.call('thread/turns/list', query)
                turns = []
                for t in result.get('data', []):
                    items = [item_summary(i) for i in t.get('items', [])]
                    turns.append(dict(id=text(t.get('id'),200),status=text(t.get('status'),80),
                                      time=stamp(t.get('startedAt')),items=[i for i in items if i]))
                return dict(threadId=thread_id, source='appServer', turns=turns,
                            nextCursor=result.get('nextCursor'), notice='Stored turn / tool history · a completed turn does not imply project completion.')
            except (RpcError, OSError):
                # Find rollout paths through our own metadata index, never a server-supplied path.
                self.local_sessions(False, None)
                self.local_sessions(True, None)
        return self.local_history(thread_id)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--home', type=Path, default=Path(os.environ.get('CODEX_HOME', str(Path.home()/'.codex'))))
    args = parser.parse_args()
    reader = Reader(args.home.expanduser())
    try:
        for line in sys.stdin:
            request = {}
            try:
                request = json.loads(line)
                method = request.get('method')
                if method not in ('sessions', 'history'): raise ValueError('read-only method rejected')
                result = getattr(reader, method)(request.get('params') or {})
                response = dict(id=request.get('id'), result=result)
            except (ValueError, TypeError, OSError, RpcError) as exc:
                response = dict(id=request.get('id'), error=text(str(exc), 400))
            print(json.dumps(response, ensure_ascii=False), flush=True)
    finally:
        reader.proxy.close()


if __name__ == '__main__': main()
