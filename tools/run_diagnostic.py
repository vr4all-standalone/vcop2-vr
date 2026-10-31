"""Build and run a bounded original-game diagnostic with isolated state."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

def command(engine,roms,output,seconds,script=None):
    engine=Path(engine).resolve();roms=Path(roms).resolve();output=Path(output).resolve()
    if not 1<=seconds<=600:raise ValueError('seconds must be 1 to 600')
    if output.is_relative_to(roms) or roms.is_relative_to(output):raise ValueError('Media and writable diagnostic state must be separate')
    if not engine.is_file():raise FileNotFoundError(engine)
    if not (roms/'vcop2.zip').is_file():raise FileNotFoundError(roms/'vcop2.zip')
    if output.exists():raise FileExistsError('Choose a fresh diagnostic output directory')
    output.mkdir(parents=True)
    args=[str(engine),'vcop2','-rompath',str(roms),'-skip_gameinfo','-noreadconfig','-video','none','-sound','none','-seconds_to_run',str(seconds),'-nothrottle','-noautoframeskip','-frameskip','0']
    for option,folder in [('nvram_directory','nvram'),('cfg_directory','cfg'),('diff_directory','diff'),('state_directory','state'),('snapshot_directory','snap')]:
        destination=output/folder;destination.mkdir();args.extend(['-'+option,str(destination)])
    if script:
        script=Path(script).resolve()
        if not script.is_file():raise FileNotFoundError(script)
        args.extend(['-autoboot_script',str(script),'-autoboot_delay','0'])
    return args,output

def run(engine,roms,output,seconds=45,script=None,mode='gameplay',capture_scenes=False):
    if mode not in {'attract','gameplay','service'}:raise ValueError('Unknown diagnostic mode')
    if script is None:script=Path(__file__).with_name('baseline.lua')
    args,output=command(engine,roms,output,seconds,script)
    env=os.environ.copy();env['VCOP2_RESEARCH_DIR']=str(output);env['VCOP2_DIAGNOSTIC_MODE']=mode
    if capture_scenes:env['VCOP2_SCENE_DIR']=str(output/'scenes')
    else:env.pop('VCOP2_SCENE_DIR',None)
    # New compiler/diagnostic scratch stays on the same D: volume.
    env['TMP']=env['TEMP']=str(output)
    start=time.monotonic()
    with (output/'engine.log').open('w') as log:
        process=subprocess.Popen(args,cwd=output,env=env,stdout=log,stderr=subprocess.STDOUT)
        try:code=process.wait(timeout=max(120,seconds*8))
        except subprocess.TimeoutExpired:
            process.terminate()
            try:process.wait(timeout=10)
            except subprocess.TimeoutExpired:process.kill();process.wait()
            raise RuntimeError('The diagnostic child exceeded its wall-clock deadline')
    native_log=(output/'engine.log').read_text(errors='replace')
    script_errors='[LUA ERROR]' in native_log
    capture_errors='VCOP2 capture failed:' in native_log
    result=dict(exit_code=code,diagnostic_ok=code==0 and not script_errors and not capture_errors,script_errors=script_errors,capture_errors=capture_errors,capture_scenes=capture_scenes,requested_native_seconds=seconds,wall_seconds=time.monotonic()-start,mode=mode,script=str(Path(script).resolve()))
    (output/'run.json').write_text(json.dumps(result,indent=2)+'\n')
    return code if code else int(script_errors or capture_errors)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine',type=Path,required=True);parser.add_argument('--roms',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--seconds',type=int,default=45);parser.add_argument('--script',type=Path);parser.add_argument('--mode',choices=['attract','gameplay','service'],default='gameplay')
    parser.add_argument('--capture-scenes',action='store_true')
    args=parser.parse_args();raise SystemExit(run(**vars(args)))
