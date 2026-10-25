#!/usr/bin/env python3
"""Real windows: both menu directions, existing-process requests, crash recovery.
Run under Xvfb. All runtime/config/Codex paths are isolated from the desktop.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT=Path(__file__).resolve().parent.parent
APP=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT/'ohmydeparture'
OUT=ROOT/'test-output';OUT.mkdir(exist_ok=True)


def run(args,env):
    p=subprocess.run([str(x) for x in args],env=env,capture_output=True,text=True,timeout=10)
    assert p.returncode==0,(args,p.returncode,p.stdout,p.stderr)
    return p.stdout.strip()


def find(title,env,process):
    end=time.monotonic()+8
    while time.monotonic()<end:
        p=subprocess.run(['xdotool','search','--onlyvisible','--name',title],env=env,capture_output=True,text=True)
        if p.returncode==0 and p.stdout.strip():return p.stdout.splitlines()[0]
        assert process.poll() is None,('main process exited',process.returncode,(OUT/'desktop-views.log').read_text())
        time.sleep(.05)
    raise AssertionError('window did not appear: '+title)


with tempfile.TemporaryDirectory(prefix='rdepartures-views-') as tmp:
    base=Path(tmp);runtime=base/'runtime';runtime.mkdir(mode=0o700)
    env=dict(os.environ,QT_QPA_PLATFORM='xcb',QT_QPA_PLATFORMTHEME='',QT_IM_MODULE='',
             XDG_RUNTIME_DIR=str(runtime),XDG_CONFIG_HOME=str(base/'config'),
             XDG_DATA_HOME=str(base/'data'),XDG_STATE_HOME=str(base/'state'),
             XDG_CACHE_HOME=str(base/'cache'),CODEX_HOME=str(base/'codex'))
    env.pop('WAYLAND_DISPLAY',None);env.pop('HYPRLAND_INSTANCE_SIGNATURE',None)
    sessions=base/'codex/sessions';sessions.mkdir(parents=True)
    (sessions/'rollout-demo.jsonl').write_text(
        '{"type":"session_meta","payload":{"id":"native-demo","cwd":"/demo/project","source":"cli"}}\n'
        '{"type":"turn_context","payload":{"turn_id":"native-turn"}}\n'
        '{"type":"event_msg","payload":{"type":"item_completed","item":{"type":"CommandExecution","id":"tool1","command":"make test","status":"completed","exit_code":0}}}\n'
        '{"type":"event_msg","payload":{"type":"task_complete"}}\n')
    with (OUT/'desktop-views.log').open('w') as log:
        process=subprocess.Popen([str(APP),'--windowed','--airport','NRT'],env=env,stdout=log,stderr=log)
        try:
            departures=find('^Oh My Departure$',env,process)
            assert run(['xdotool','getwindowpid',departures],env)==str(process.pid)
            run([APP,'--view','codex','--windowed'],env)
            codex=find('^Oh My Departure — Codex$',env,process)
            assert run(['xdotool','getwindowpid',codex],env)==str(process.pid),'new process stole requested view'
            # Genuine native renderer row activation opens supplementary history.
            time.sleep(4.2)
            run(['magick','import','-window',codex,OUT/'codex-native.png'],env)
            run(['xdotool','windowfocus','--sync',codex],env)
            run(['xdotool','windowmove',codex,'1000','0'],env)
            run(['xdotool','mousemove','--window',codex,'330','110','click','1'],env)
            details=find('^Codex session details$',env,process)
            geometry=dict(line.split('=',1) for line in run(['xdotool','getwindowgeometry','--shell',details],env).splitlines())
            dx,dy,dw,dh=(int(geometry[k]) for k in ('X','Y','WIDTH','HEIGHT'))
            assert 0<=dx and dx+dw<=1920 and 0<=dy and dy+dh<=1080,geometry
            assert abs(dx+dw/2-960)<=2,geometry
            run(['magick','import','-window','root',OUT/'codex-centred-details.png'],env)
            run(['xdotool','windowfocus','--sync',details],env);run(['xdotool','key','Escape'],env)
            run(['xdotool','windowmove',codex,'0','0'],env)
            run([APP,'--airport','SEOUL','--windowed'],env)
            departures=find('^Oh My Departure$',env,process)
            # Existing airport popup now really includes Codex (one above Quit).
            run(['xdotool','windowfocus','--sync',departures],env)
            run(['xdotool','mousemove','--window',departures,'1530','30','click','1'],env)
            time.sleep(.1)
            run(['magick','import','-window','root',OUT/'native-menu-selected.png'],env)
            run(['xdotool','mousemove','1600','327','click','1'],env)
            codex=find('^Oh My Departure — Codex$',env,process)
            assert run(['xdotool','getwindowpid',codex],env)==str(process.pid)
            # Same menu in Codex can select an original airport (Home = ICN).
            run(['xdotool','windowfocus','--sync',codex],env)
            run(['xdotool','mousemove','--window',codex,'1530','30','click','1'],env)
            time.sleep(.1);run(['xdotool','mousemove','1600','61','click','1'],env)
            departures=find('^Oh My Departure$',env,process)
            assert 'airport=ICN' in (base/'config/edwardkim/OhMyDeparture.conf').read_text()
            # Crash recovery must reclaim only the owned stale UI lock/socket.
            process.kill();process.wait(timeout=3)
            process=subprocess.Popen([str(APP),'--view','codex','--windowed'],env=env,stdout=log,stderr=log)
            codex=find('^Oh My Departure — Codex$',env,process)
            assert run(['xdotool','getwindowpid',codex],env)==str(process.pid)
            run(['xdotool','windowfocus','--sync',codex],env);run(['xdotool','key','Escape'],env)
            assert process.wait(timeout=5)==0
        finally:
            if process.poll() is None:process.kill();process.wait(timeout=3)
print('PASS: native airport ↔ Codex menus, row details, same-process view requests, persisted selection, crash recovery, Escape')
