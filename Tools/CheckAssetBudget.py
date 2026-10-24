#!/usr/bin/env python3
"""Audit all tracked and pending source assets. No Git LFS; each file <100 MiB."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--budget-mb',type=float,default=None,help='Optional total budget; no default total cap')
 p.add_argument('--large-file-mb',type=float,default=50)
 p.add_argument('--top',type=int,default=15)
 p.add_argument('--json',action='store_true')
 args=p.parse_args()
 tracked=subprocess.check_output(['git','ls-files','-z'],cwd=ROOT).split(b'\0')
 names=set(subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=ROOT).split(b'\0'))-{b''}
 sizes=sorted([( (ROOT/n.decode('utf-8')).stat().st_size,n.decode('utf-8')) for n in names if (ROOT/n.decode('utf-8')).is_file()],reverse=True)
 tracked_names={n.decode('utf-8') for n in tracked if n}
 attributes=subprocess.run(['git','check-attr','-z','--stdin','filter'],cwd=ROOT,input=b'\0'.join(names)+b'\0',capture_output=True,check=True).stdout.split(b'\0')
 lfs=[attributes[i].decode('utf-8') for i in range(0,len(attributes)-2,3) if attributes[i+2]==b'lfs']
 result=dict(source_files=len(sizes),tracked_mib=sum(s for s,n in sizes if n in tracked_names)/2**20,
  pending_included_mib=sum(s for s,n in sizes)/2**20,content_mib=sum(s for s,n in sizes if n.startswith('Content/'))/2**20,
  git_mib=sum(p.stat().st_size for p in (ROOT/'.git').rglob('*') if p.is_file())/2**20,
  above_50_mib=[dict(path=n,mib=s/2**20) for s,n in sizes if s>=args.large_file_mb*2**20],
  above_100_mib=[n for s,n in sizes if s>=100*2**20],lfs_files=lfs,
  largest=[dict(path=n,mib=s/2**20) for s,n in sizes[:args.top]])
 failed=bool(result['above_100_mib'] or lfs or (args.budget_mb is not None and result['pending_included_mib']>args.budget_mb))
 result['passed']=not failed
 print(json.dumps(result,indent=2,ensure_ascii=False))
 return int(failed)
if __name__=='__main__':raise SystemExit(main())
