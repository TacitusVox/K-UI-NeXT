import concurrent.futures
from pathlib import Path
import re
import shlex
import subprocess
import sys

lines = Path('../toy37-preflight.log').read_text().replace('\\\n',' ').splitlines()
link = next(shlex.split(line) for line in lines if ' -Wl,-Map,build/toy-slow-retry/resident-sci.map ' in line)
print('Original optimizer:', [x for x in link if x in ('-Os','-Oz')], flush=True)
configs = {
    'os': ['-Os'],
    'oz': ['-Oz'],
    'oz-noalign': ['-Oz','-fno-align-labels','-fno-align-jumps','-fno-align-loops','-fno-align-functions'],
    'os-ipa': ['-Os','-fipa-pta'],
    'oz-ipa': ['-Oz','-fipa-pta'],
    'os-no-once': ['-Os','-fno-inline-functions-called-once'],
    'oz-no-once': ['-Oz','-fno-inline-functions-called-once'],
    'oz-no-small': ['-Oz','-fno-inline-small-functions'],
    'os-branch': ['-Os','-mbranch-cost=2'],
    'oz-branch': ['-Oz','-mbranch-cost=2'],
    'oz-merge': ['-Oz','-fmerge-all-constants','-fipa-icf','-fira-region=one'],
    'oz-lto-part': ['-Oz','-flto-partition=none'],
}

def run(item):
    name, flags = item
    path = Path('build/toy37-link-search') / name
    path.mkdir(parents=True, exist_ok=True)
    command = [str(path/'resident-sci.elf') if x.endswith('/resident-sci.elf') else
               f'-Wl,-Map,{path}/resident-sci.map' if x.startswith('-Wl,-Map,') else x
               for x in link if x not in ('-Os','-Oz')]
    command[1:1] = flags[:1]
    command += flags[1:]
    with (path/'build.log').open('w') as out:
        r = subprocess.run(command, stdout=out, stderr=subprocess.STDOUT)
    try:
        addr = re.search(r'(0x[0-9a-f]+)\s+__retail_resident_bss_end =', (path/'resident-sci.map').read_text()).group(1)
    except (FileNotFoundError, AttributeError):
        addr = '?'
    return name, addr, r.returncode

with concurrent.futures.ThreadPoolExecutor(max_workers=6) as workers:
    for result in workers.map(run, configs.items()):
        print(*result, flush=True)
