#!/usr/bin/env python3

import argparse
import os
import sys
import psutil
import json
import re

info = 'Alters CPU masks of running processes with different arguments, based on a configuration file.'
parser = argparse.ArgumentParser(description=info)
parser.add_argument('--pinfile', '-p', type=str, required=True, help='file with process to CPU mask list')
try:
  args = parser.parse_args()
except:
  parser.print_help()
  sys.exit(0)
pinfile = args.pinfile

print('### Basic information...')
lcpu_count = len(os.sched_getaffinity(0))
pcpu_count = psutil.cpu_count(logical=False)
vmem = psutil.virtual_memory()
print('  -> Logical CPU count: ', lcpu_count)
print('  -> Physical CPU count: ', pcpu_count)
print('  -> Memory status: ', vmem) 
print('\n')

### Parses CPU mask strings and lists
def parse_cpumask(mask):
  cpu_mask = set()
  if type(mask) == list:
    cpu_mask = set(mask)
    return list(cpu_mask)
  elif type(mask) == str:
    for region_str in mask.split(','):
      try:
        region = region_str.replace(" ", "").split('-')
        if len(region) == 1:
          cpu_mask.add(int(region[0]))
        elif len(region) == 2:
          cpu_from = int(region[0])
          cpu_to = int(region[1])
          for cpu in range(cpu_from, cpu_to+1):
            cpu_mask.add(cpu)
        else:
          raise Exception('This is neither a single CPU or a range of CPUs:', mask)
      except Exception as e:
        raise Exception('Corrcupt CPU mask region! Mask string:', mask, 'Exception:', e)
  else:
    raise Exception('CPU mask needs to be string or list. Current:', mask)
  return list(cpu_mask)

### Parse affinity json
print('### Parsing CPU mask file...')
affinity_dict = {}
try:
  with open(pinfile, 'r') as f:
    affinity_json = json.load(f)
    for proc in affinity_json:
      if proc == '_comment':
        continue
      else:
        affinity_dict[proc] = {}
      for proc_opts in affinity_json[proc]:
        affinity_dict[proc][proc_opts] = {}
        for aff_field in affinity_json[proc][proc_opts]:
          if aff_field == 'parent':
            aff_value = affinity_json[proc][proc_opts][aff_field]
            affinity_dict[proc][proc_opts]['parent'] = parse_cpumask(aff_value)
          elif aff_field == 'threads':
            affinity_dict[proc][proc_opts]['threads'] = {}
            for thr_aff_field in affinity_json[proc][proc_opts][aff_field]:
              aff_value = affinity_json[proc][proc_opts][aff_field][thr_aff_field]
              affinity_dict[proc][proc_opts]['threads'][thr_aff_field] = {
                'regex':  re.compile(thr_aff_field),
                'cpu_list': parse_cpumask(aff_value)
              }
          else: 
            print('Expected affinity fields are \'parent\' or \'threads\'! Ignoring field: ' + aff_field)
            continue
except Exception as e:
  print(e)
  sys.exit(0)
print('\n')

### Apply CPU masks
proc_names = affinity_dict.keys()
procs = []
print('### Attempt to find and apply cpu mask for the following processes:', proc_names)
for proc in psutil.process_iter():
  if proc.name() in proc_names:
    procs.append(proc)

for proc in procs:
  cmdline_dict = affinity_dict[proc.name()].keys()
  proc_cmdline = ' '.join(proc.cmdline())
  for cmdl in cmdline_dict:
    if cmdl in proc_cmdline:
      print('   -> Found process to mask!')
      print('      + Command line:', proc_cmdline)
      print('      + Process ID (PID):', proc.pid)
      print('      + Detailed memory info:', proc.memory_info())
      rss = 'Resident Set Size (RSS): ' + str('{:.2f}'.format(proc.memory_info().rss/1024/1024)) + ' [MB]'
      vms = 'Virtual Memory Size (VMS): ' + str('{:.2f}'.format(proc.memory_info().vms/1024/1024)) + ' [MB]'
      print('      + Memory info:', rss, '|', vms)
      connections = proc.connections()
      print('      + Network connection count:', len(connections))
      children = proc.children(recursive=True)
      print('      + Children count:', len(children))
      threads = proc.threads()
      print('      + Thread count:', len(threads))

      if 'parent' in affinity_dict[proc.name()][cmdl].keys():
        mask = affinity_dict[proc.name()][cmdl]['parent']
        print('      + Parent mask specified! Applying mask for every children and thread!')
        print('        - mask:', mask)
        if max(mask) > lcpu_count:
          print('WARNING! CPU Mask contains higher CPU IDs than logical CPU count visible by the kernel!')
          #raise Exception('Error! The CPU mask contains higher CPU IDs than logical CPU count on system!')
         
        for child in children:
          cid = psutil.Process(child.id)
          cid.cpu_affinity(mask)
        for thread in threads:
          tid = psutil.Process(thread.id)
          tid.cpu_affinity(mask)

      if 'threads' in affinity_dict[proc.name()][cmdl]:
        print('      + Thread masks specified! Applying thread specific masks!')
        for thread in threads:
          tid = psutil.Process(thread.id)
          thread_masks = affinity_dict[proc.name()][cmdl]['threads']
          # print(thread_masks)

          # Match thread names with the mask regex
          mask_matches = [ m for m in [(th_mask['regex'].fullmatch(tid.name()),th_mask['cpu_list']) for th_mask in thread_masks.values()] if m[0] is not None]
          if len(mask_matches) > 1:
            raise ValueError(f"Thread {tid.name()} mask_matches multiple masks {mask_matches}")
          elif len(mask_matches) == 0:
            continue

          # Extract the cpu list
          ((_,cpu_list),) = mask_matches
          # if tid.name() in affinity_dict[proc.name()][cmdl]['threads'].keys():
          # tmask = affinity_dict[proc.name()][cmdl]['threads'][tid.name()]
          # print('        - For thread', tid.name(), 'applying mask', cpu_list)
          print(f'        - For thread {tid.name()} applying mask {cpu_list}')
          if max(cpu_list) > lcpu_count:
            print('WARNING! CPU Mask contains higher CPU IDs than logical CPU count visible by the kernel!')
            #raise Exception('Error! The CPU mask contains higher CPU IDs than logical CPU count on system!')
 
          tid.cpu_affinity(cpu_list)

          # if tid.name() in affinity_dict[proc.name()][cmdl]['threads'].keys():
          #   tmask = affinity_dict[proc.name()][cmdl]['threads'][tid.name()]
          #   print('        - For thread', tid.name(), 'applying mask', tmask)
          #   tid.cpu_affinity(tmask)
      print('\n')

print('### CPU affinity applied!')
exit(0)

