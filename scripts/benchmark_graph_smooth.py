"""Compare graph HTTP work and independently referenced drawing errors.

python scripts/benchmark_graph_smooth.py --before old/numforge_web.exe --after new/numforge_web.exe
Timings are diagnostics, not CI thresholds. Each child server is privately owned.
"""
import argparse, csv, json, math, statistics, subprocess, time, urllib.request
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--before',type=Path,required=True)
parser.add_argument('--after',type=Path,required=True)
parser.add_argument('--output',type=Path,default=Path('build/graph-smooth-bench.csv'))
parser.add_argument('--repeats',type=int,default=3)
args=parser.parse_args()
if not 1<=args.repeats<=10: parser.error('repeats must be 1..10')
cases=[('sin(x)',math.sin),('x^3-3x',lambda x:x*x*x-3*x),
 ('exp(-(x-0.37)^2)',lambda x:math.exp(-(x-.37)**2)),
 ('sin(3x)+cos(2x)',lambda x:math.sin(3*x)+math.cos(2*x)),
 ('sin(100x)',lambda x:math.sin(100*x)),('exp(x)',math.exp),
 ('1/(x-0.1)^2',None),('sin(1000x)',None)]
rows=[]
for label,binary,port in [('before',args.before,19489),('after',args.after,19490)]:
 child=subprocess.Popen([str(binary.resolve()),'--no-browser','--port',str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
 try:
  base=f'http://127.0.0.1:{port}'
  for attempt in range(40):
   try:
    with urllib.request.urlopen(base,timeout=1):break
   except OSError:time.sleep(.1)
  else:raise RuntimeError('Server did not start')
  for expression,reference in cases:
   url=base+'/api/graph/plot?mode=fast&min=-30&max=30&ymin=-10&ymax=10&width=2048&height=440'
   if label=='after':url+='&interpolation=hermite'
   timings=[]
   for repeat in range(args.repeats+1):
    req=urllib.request.Request(url,data=expression.encode(),headers={'Content-Type':'text/plain'})
    start=time.perf_counter()
    with urllib.request.urlopen(req,timeout=10) as response:payload=response.read()
    elapsed=(time.perf_counter()-start)*1000
    if repeat:timings.append(elapsed)
   data=json.loads(payload);points=data['points'];slopes=data.get('slopes',[None]*len(points));error=0;checks=0;gaps=0
   for i in range(1,len(points)):
    a,b=points[i-1],points[i]
    if not b[2]:
     if a[3]==b[3]==0 and a[1] is not None and b[1] is not None and a[0]<10 and b[0]>-10 and max(a[1],b[1])>=-10 and min(a[1],b[1])<=10:gaps+=1
     continue
    if reference is None:continue
    dx,dy=b[0]-a[0],b[1]-a[1];alpha=beta=None
    # Same shape-preserving representation as the browser; expected values
    # come independently from Python math, never a NumForge evaluation.
    if label=='after' and dx*714/20>1 and dy and slopes[i-1] is not None and slopes[i] is not None:
     secant=dy/dx;alpha=max(0,slopes[i-1]/secant);beta=max(0,slopes[i]/secant);norm=math.hypot(alpha,beta)
     if norm>3:alpha*=3/norm;beta*=3/norm
    for t in (.125,.25,.5,.75,.875):
     x=a[0]+dx*t
     if not -10<=x<=10:continue
     actual=reference(x)
     if not -10<=actual<=10:continue
     y=a[1]+dy*t
     if alpha is not None:
      c1=a[1]+dy*alpha/3;c2=b[1]-dy*beta/3
      y=(1-t)**3*a[1]+3*(1-t)**2*t*c1+3*(1-t)*t*t*c2+t**3*b[1]
     error=max(error,abs(y-actual)*372/20);checks+=1
   row=dict(version=label,expression=expression,points=data['count'],limited=data['limited'],visible_real_gaps=gaps,median_http_ms=round(statistics.median(timings),3),min_http_ms=round(min(timings),3),max_http_ms=round(max(timings),3),response_bytes=len(payload),max_visible_error_px=round(error,6) if checks else '',reference_checks=checks)
   rows.append(row);print(json.dumps(row),flush=True)
 finally:
  child.terminate();child.wait(timeout=5)
args.output.parent.mkdir(parents=True,exist_ok=True)
with args.output.open('w',newline='',encoding='utf-8') as out:
 writer=csv.DictWriter(out,fieldnames=rows[0].keys());writer.writeheader();writer.writerows(rows)
