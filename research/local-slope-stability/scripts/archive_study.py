#!/usr/bin/env python3
"""Inventory legacy Qt chart streams and evaluate numerical sensitivity, without diagnoses."""
import csv
import hashlib
import json
import math
import struct
from collections import Counter
from pathlib import Path
import numpy as np
import pilot

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = Path('/media/vladislav-b/Tree/Library/рез')
OUT = ROOT/'results/archive01'


def decode(data, count_fmt):
    pos = 0
    def take(fmt):
        nonlocal pos
        if pos+struct.calcsize('>'+fmt)>len(data):
            raise ValueError('truncated')
        v=struct.unpack_from('>'+fmt,data,pos)[0]
        pos+=struct.calcsize('>'+fmt)
        return v
    def string():
        nonlocal pos
        n=take('I')
        if n==0xffffffff:
            return ''
        if n>len(data)-pos or n%2:
            raise ValueError('string_length')
        s=data[pos:pos+n].decode('utf-16-be');pos+=n
        return s
    title=string();numeric=take('B')
    if numeric not in (0,1):raise ValueError('axis_type')
    xf=string();ny=take(count_fmt)
    if not 1<=ny<=10:raise ValueError('axis_count')
    yf=[string() for _ in range(ny)]
    ns=take(count_fmt)
    if not 1<=ns<=20:raise ValueError('series_count')
    names=[]
    for _ in range(ns):
        axis=take('B');name=string();color=take('B')
        if axis>=ny or color!=1:raise ValueError('unsupported_axis_or_color')
        for _ in range(5):take('H')
        names.append(name)
    arrays=[]
    for _ in range(2*ns):
        n=take('I')
        if n>(len(data)-pos)//16:raise ValueError('point_count')
        arr=np.frombuffer(data,dtype='>f8',count=2*n,offset=pos).reshape(n,2).astype(float)
        pos+=16*n
        arrays.append(arr)
    if take('B') not in (0,1):raise ValueError('visibility')
    # Optional axis titles appended by recent versions of the application.
    if pos<len(data):
        string();nt=take('I')
        if nt>10:raise ValueError('title_count')
        for _ in range(nt):string()
    if pos!=len(data):raise ValueError('trailing_bytes')
    return dict(chart=title,numeric_x=bool(numeric),x_format=xf,y_formats=yf,
                names=names,arrays=arrays,count_format=count_fmt)


def calculate(points,bounds,width=.1):
    """Vectorized equivalent of the pilot; same floating window progression."""
    low,high=bounds;span=high-low
    a=points[(points[:,1]>=low+.1*span)&(points[:,1]<=high-.1*span)]
    def slope(p):
        if len(p)<2:return None
        x=p[:,0]-p[:,0].mean();y=p[:,1]-p[:,1].mean();den=x@x
        return float((x@y)/den) if den>0 else None
    k=slope(a)
    if k is None or not math.isfinite(k) or abs(k)<=1e-12:return None
    slopes=[];lo=low+.1*span;attempted=0
    while lo+width*span<=high-.1*span+1e-9:
        p=a[(a[:,1]>=lo)&(a[:,1]<=lo+width*span)];attempted+=1
        kj=slope(p) if len(p)>=8 else None
        if kj is not None and math.isfinite(kj):slopes.append(kj)
        lo+=.025*span
    if not slopes:return None
    delta=np.array(slopes)-k
    return dict(k=k,windows=len(slopes),attempted_windows=attempted,work_points=len(a),
                d_max_pct=float(np.abs(delta).max()/abs(k)*100),
                d_rms_pct=float(np.sqrt(np.mean(delta**2))/abs(k)*100),
                delta_max_position_per_bar=float(np.abs(delta).max()))


def write_csv(name,rows):
    keys=list(dict.fromkeys(k for row in rows for k in row))
    with (OUT/name).open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rows)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    pilot.verify()
    # Independent vectorization must match the existing scalar implementation.
    pp=pilot.read_points(pilot.DEFAULT_DATA/'Pressure_1__dubl0.csv')
    bounds=min(y for _,y in pp),max(y for _,y in pp)
    for width in [.05,.1,.15,.2]:
        ref=pilot.calculate(pp,bounds,width);got=calculate(np.array(pp),bounds,width)
        for key in ['k','d_max_pct','d_rms_pct']:
            assert math.isclose(ref[key],got[key],rel_tol=1e-9,abs_tol=1e-8),(key,ref[key],got[key])
    inventory=[];baseline=[];cases=[];seen={}
    files=sorted(ARCHIVE.rglob('*.data'))
    for i,path in enumerate(files):
        raw=path.read_bytes();row={'path':str(path),'sha256':hashlib.sha256(raw).hexdigest()}
        parsed=None;errors=[]
        for fmt in ['q','i']:
            try:parsed=decode(raw,fmt);break
            except (ValueError,struct.error,UnicodeError) as e:errors.append(str(e))
        if parsed is None:
            row.update(status='unsupported_format',detail=' | '.join(errors));inventory.append(row);continue
        row.update(chart=parsed['chart'],x_format=parsed['x_format'],y_formats=' | '.join(parsed['y_formats']),
                   series=' | '.join(parsed['names']),count_format=parsed['count_format'])
        # Include by serialized semantic chart title, never by filename alone.
        if parsed['chart'].casefold() not in ['pressure','давление']:
            row['status']='other_chart';inventory.append(row);continue
        names=parsed['names'];arrays=parsed['arrays'];n=len(names)
        if not parsed['numeric_x'] or not names or not ('давлен' in names[0].casefold()):
            row['status']='unconfirmed_axes';inventory.append(row);continue
        if 'bar' not in parsed['x_format'].lower():
            row['status']='unconfirmed_pressure_unit';inventory.append(row);continue
        backward,forward=arrays[0],arrays[n]
        if not len(backward) or not len(forward):
            row['status']='missing_direction';inventory.append(row);continue
        if not np.isfinite(forward).all() or not np.isfinite(backward).all():
            row['status']='nonfinite';inventory.append(row);continue
        low=min(forward[:,1].min(),backward[:,1].min());high=max(forward[:,1].max(),backward[:,1].max())
        if high<=low:
            row['status']='zero_position_span';inventory.append(row);continue
        signature=hashlib.sha256(forward.tobytes()+b'|'+backward.tobytes()).hexdigest()
        row['measurement_sha256']=signature
        if signature in seen:
            row.update(status='duplicate_measurements',duplicate_of=seen[signature]);inventory.append(row);continue
        seen[signature]=str(path)
        pair={d:calculate(p,(low,high)) for d,p in [('forward',forward),('backward',backward)]}
        if not all(pair.values()):
            row['status']='invalid_baseline';inventory.append(row);continue
        row['status']='included';inventory.append(row)
        for direction,points in [('forward',forward),('backward',backward)]:
            base=pair[direction]
            baseline.append(dict(path=str(path),measurement_sha256=signature,direction=direction,
                                 y_format=row['y_formats'],points=len(points),position_min=float(low),position_max=float(high),**base))
            for width in [.05,.1,.15,.2]:
                variants=[('width',width,1,0,points)] if width!=.1 else [('width',width,1,0,points)]+[
                    ('decimation',width,stride,offset,points[offset::stride])
                    for stride in [2,4,8] for offset in range(stride)]
                for experiment,w,stride,offset,p in variants:
                    result=calculate(p,(low,high),w)
                    case=dict(path=str(path),direction=direction,experiment=experiment,width=w,
                              stride=stride,offset=offset,valid=result is not None)
                    if result:
                        case.update(result)
                        case['complete_window_coverage']=result['windows']==result['attempted_windows']
                        case['change_dmax_pp']=result['d_max_pct']-base['d_max_pct']
                        case['change_drms_pp']=result['d_rms_pct']-base['d_rms_pct']
                    cases.append(case)
        if len(baseline)%200==0:print(f'Included {len(baseline)//2} unique records',flush=True)
    write_csv('inventory.csv',inventory);write_csv('baseline.csv',baseline);write_csv('sensitivity.csv',cases)
    statuses=Counter(r['status'] for r in inventory)
    lines=['# Архив: вычислительная устойчивость', '',f'Просмотрено файлов .data: {len(files)}.',
           f'Включено уникальных пар измерений: {len(baseline)//2}; ходов: {len(baseline)}.', '',
           'Это не число независимых устройств. Повторные прогоны, разные условия и связи с ремонтом не размечены.',
           'Единицы положения не подтверждены: сохранён исходный формат оси. Проценты сравниваются внутри одной записи.',
           'Направления приняты по структуре слоёв приложения. Полнота физического прохода не подтверждена.', '',
           '## Учёт файлов','', '| Статус | Файлов |','|---|---:|']
    lines += [f'| {s} | {v} |' for s,v in sorted(statuses.items())]
    lines+=['','## Изменения показателей относительно окна 10 % и всех точек','',
            'Медиана и 90-й процентиль абсолютного изменения в процентных пунктах. Это описательная статистика по ходам, не доверительные интервалы.', '',
            '| Воздействие | Пригодных / всего вариантов | Медиана изменения D_max, п.п. | P90 | Медиана изменения D_RMS, п.п. | P90 |',
            '|---|---:|---:|---:|---:|---:|']
    for experiment,values in [('width',[.05,.15,.2]),('decimation',[2,4,8])]:
        for value in values:
            group=[r for r in cases if r['experiment']==experiment and r['width' if experiment=='width' else 'stride']==value]
            valid=[r for r in group if r['valid']]
            if not valid:continue
            stats=[float(np.quantile([abs(r[key]) for r in valid],q)) for key in ['change_dmax_pp','change_drms_pp'] for q in [.5,.9]]
            label=f'Окно {100*value:g} %' if experiment=='width' else f'Каждая {value}-я точка, все смещения'
            lines.append(f'| {label} | {len(valid)} / {len(group)} | '+' | '.join(f'{v:.3f}' for v in stats)+' |')
    lines+=['','При прореживании варианты одной записи зависимы; сводка выше взвешивает все смещения одинаково. Для выводов о независимых устройствах нужна отдельная группировка.',
            'Границы, шаг 2,5 % полного размаха и минимум 8 точек фиксированы. Показатель отсутствует при вырожденном расчёте; нулём не заменяется.',
            'Некоторые окна могут пропускаться: см. windows и attempted_windows. Изменение состава окон способно менять максимальный показатель.',
            'Не установлены диагностические пороги, причина изменений, валидность для привода или новизна формулы. C++-сверка и проверка всех версий формата остаются отдельными задачами.','']
    lines+=['## Контроль полноты расчётных окон при прореживании','',
            'Сравниваем только варианты, где все окна пригодны и в базовом, и в прореженном расчёте. Это отдельное подмножество, а не исправленная оценка всего архива.', '',
            '| Каждая n-я точка | Вариантов с полным покрытием | Медиана изменения D_max, п.п. | P90, п.п. |',
            '|---|---:|---:|---:|']
    by_key={(r['path'],r['direction']):r for r in baseline}
    for stride in [2,4,8]:
        group=[r for r in cases if r['experiment']=='decimation' and r['stride']==stride
               and r['valid'] and r['complete_window_coverage']
               and by_key[r['path'],r['direction']]['windows']==by_key[r['path'],r['direction']]['attempted_windows']]
        if group:
            vals=[abs(r['change_dmax_pp']) for r in group]
            lines.append(f'| {stride} | {len(group)} | {np.median(vals):.3f} | {np.quantile(vals,.9):.3f} |')
    records=[]
    for path in sorted({r['path'] for r in baseline}):
        for stride in [2,4,8]:
            group=[r for r in cases if r['path']==path and r['experiment']=='decimation' and r['stride']==stride]
            valid=[r for r in group if r['valid']]
            records.append(dict(path=path,stride=stride,valid_variants=len(valid),total_variants=len(group),
                                max_abs_dmax_change_pp=max((abs(r['change_dmax_pp']) for r in valid),default=None),
                                max_abs_drms_change_pp=max((abs(r['change_drms_pp']) for r in valid),default=None)))
    write_csv('per_record_decimation.csv',records)
    lines+=['','## Сводка по записям','',
            'Для каждой записи берём наибольшее абсолютное изменение среди двух ходов и всех смещений прореживания. Затем считаем медиану и P90 этих максимумов. Каждая пара измерений имеет одинаковый вес; это всё ещё не выборка независимых устройств.', '',
            '| Каждая n-я точка | Записей с результатом | Медиана максимума изменения D_max, п.п. | P90, п.п. |', '|---|---:|---:|---:|']
    for stride in [2,4,8]:
        vals=[r['max_abs_dmax_change_pp'] for r in records if r['stride']==stride and r['max_abs_dmax_change_pp'] is not None]
        lines.append(f'| {stride} | {len(vals)} | {np.median(vals):.3f} | {np.quantile(vals,.9):.3f} |')
    (OUT/'report.md').write_text('\n'.join(lines))
    (OUT/'manifest.json').write_text(json.dumps(dict(archive=str(ARCHIVE),status_counts=statuses,
        script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),numpy=np.__version__,
        formula_source_sha256=hashlib.sha256(pilot.SOURCE.read_bytes()).hexdigest(),
        pilot_sha256=hashlib.sha256(Path(pilot.__file__).read_bytes()).hexdigest(),
        comparisons_to_scalar='4 widths of PV-2223, tolerance 1e-8 absolute / 1e-9 relative',
        inclusion='parsed semantic Pressure chart; numeric pressure axis in bar; two finite directions; nonzero span; dedup exact coordinate arrays; both baseline computations valid'),ensure_ascii=False,indent=2)+'\n')
    print(OUT/'report.md',flush=True)


if __name__=='__main__':main()
