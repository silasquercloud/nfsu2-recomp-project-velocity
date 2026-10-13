#!/usr/bin/env python3
"""Compare exported Velocity CSVs; operator must keep device/scene/settings equal.

Example: --scenario 'same save, race, 720p, 30fps cap, warm caches' v4.csv v5.csv
No absent GPU timing, backend measurements, or Android FPS are synthesized.
"""
import argparse,csv,json,math,statistics,re
from pathlib import Path

def summarize(path):
    lines=path.read_text().splitlines()
    rows=list(csv.DictReader(line for line in lines if line and not line.startswith('#')))
    if not rows:raise ValueError(str(path)+': no captured frames')
    ns=sorted(int(row['wall_ns']) for row in rows)
    if min(ns)<=0:raise ValueError('Nonpositive frame interval')
    q=lambda p:ns[max(0,math.ceil(len(ns)*p)-1)]/1e6
    slow=ns[-max(1,math.ceil(len(ns)*.01)):]
    result={'file':str(path),'frames':len(ns),'fps_average':len(ns)*1e9/sum(ns),'fps_1_percent_low':1e9/statistics.mean(slow),'p50_ms':q(.5),'p95_ms':q(.95),'p99_ms':q(.99),'intervals_over_33ms':sum(n>33333333 for n in ns),'gpu_execution_ms':None}
    for key in ('cpu_process_ns','present_calling_thread_cpu_ns','draw_calls','pipeline_compile_host_samples','pipeline_object_hits','pipeline_object_misses','lock_scratch_reuse','lock_scratch_allocations','lock_read_copy_bytes'):
        if key in rows[0]:result[key+'_mean']=statistics.mean(int(row[key]) for row in rows)
    methods={int(n):name for n,name in re.findall(r'case (\d+):.*?device->(\w+)\(',
        (Path(__file__).resolve().parents[1]/'android/app/src/main/cpp/d3d9_device_methods.inc').read_text())}
    operations=[]
    for line in lines:
        if line.startswith('# d3d9_op='):
            try:
                kind,method,calls,samples,total,maximum=map(int,line.split('=',1)[1].split(','))
            except ValueError:continue
            operations.append({'name':methods.get(method,'device_special') if kind==2 else 'resource_kind_'+str(kind)+'_method_'+str(method),'kind':kind,'method':method,'calls':calls,'samples':samples,'sample_mean_ms':total/max(samples,1)/1e6,'sample_max_ms':maximum/1e6,'estimated_session_ms':total/max(samples,1)*calls/1e6})
    result['d3d9_operations_by_estimated_cost']=sorted(operations,key=lambda o:o['estimated_session_ms'],reverse=True)
    result['metadata']=[line for line in lines if line.startswith('# ') and not line.startswith('# d3d9_op=')]
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--scenario',required=True);p.add_argument('reports',nargs='+',type=Path);a=p.parse_args()
    print(json.dumps({'scenario_supplied_by_operator':a.scenario,'limits':'Last bounded capture windows; CPU process sums threads; D3D9 session samples overlap frame metrics; no causal attribution or GPU time. Compare only equal device/scene/resolution/quality/driver/cache state.','reports':[summarize(path) for path in a.reports]},indent=2,ensure_ascii=False))
if __name__=='__main__':main()
