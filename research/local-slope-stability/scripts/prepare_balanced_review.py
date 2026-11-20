"""Stage full-triplet comparison groups and augment existing cases, without overwriting comments."""
import csv
import hashlib
import json
import shutil
import tempfile
from pathlib import Path
import numpy as np
import archive_study as study

DEST = Path('/home/vladislav-b/Documents/Характеристика локальной крутизны')


def main():
    stage=Path(tempfile.mkdtemp(prefix='mpi-balanced-review-'))
    capture=[];registry=[]
    old=json.loads((DEST/'Графики_для_обсуждения/Реестр.json').read_text())
    with (study.OUT/'baseline.csv').open() as f:
        baselines={}
        for r in csv.DictReader(f):baselines.setdefault(r['path'],[]).append(r)
    with (study.OUT/'per_record_decimation.csv').open() as f:
        changes={(r['path'],r['stride']):float(r['max_abs_dmax_change_pp']) for r in csv.DictReader(f) if r['max_abs_dmax_change_pp']}

    def add(relative,source,description,new=False):
        folder=stage/relative;folder.mkdir(parents=True)
        files=[];hashes={}
        for name in ['Pressure_1.data','Task_1.data','Friction_1.data']:
            original=source.parent/name
            if original.exists():
                target=folder/name;shutil.copy2(original,target)
                hashes[name]=hashlib.sha256(original.read_bytes()).hexdigest()
                files.append(str(target))
            else:files.append('')
        capture.append(dict(files=files,output=str(folder)))
        (folder/'03_Просмотр_трёх_графиков.txt').write_text(description+'\n\nОтсутствуют в исходной папке: '+(', '.join(n for n,p in zip(['Pressure','Task','Friction'],files) if not p) or 'нет')+'\n')
        if new:
            (folder/'02_Комментарий.txt').write_text('Дата: \nУчастники: \n\nКорректность начала и конца хода: \n\nНаблюдения по заданию: \n\nНаблюдения по давлению: \n\nНаблюдения по трению: \n\nУстановленные факты: \n\nПредположения: \n\nРешение: пригодно / исключить / требуется уточнение: \nПричина: \n\nСвободный комментарий: \n')
        registry.append(dict(folder=str(relative),source=str(source),sha256=hashes))

    excluded=[]
    for r in old:
        relative=Path('Графики_для_обсуждения')/r['folder']
        text='Исходная подборка крайних изменений показателя. Рассматривать вместе с доступными графиками задания и трения. Пригодность физического испытания не подтверждена.'
        if r['folder'].startswith('09_'):
            text+='\nСТАТУС: исключить из дальнейшего исследования по экспертному замечанию пользователя: «09-ый — он точно не валидный вообще». Конкретную причину уточнить при совместном просмотре.'
            excluded.append(dict(source=r['source'],status='excluded_by_user',reason='Пользователь признал случай 09 невалидным; причина пока не конкретизирована',previous_results='Архивный отчёт archive01 рассчитан до этой экспертной отметки'))
        add(relative,Path(r['source']),text)

    candidates={'Умеренная_устойчивость':[],'Высокая_устойчивость':[]}
    for path,rows in baselines.items():
        source=Path(path)
        if source.name!='Pressure_1.data' or any(not (source.parent/name).exists() for name in ['Task_1.data','Friction_1.data']):continue
        if any(int(r['windows'])!=29 or int(r['points'])<500 or float(r['d_max_pct'])>30 for r in rows):continue
        try:data=study.decode(source.read_bytes(),'q')
        except (ValueError,UnicodeError):continue
        f,b=data['arrays'][len(data['names'])],data['arrays'][0]
        lo=min(f[:,1].min(),b[:,1].min());hi=max(f[:,1].max(),b[:,1].max());span=hi-lo
        if span<=0:continue
        valid=True
        for t,forward in [(f,True),(b,False)]:
            y=(t[:,1]-lo)/span;start=float(np.median(y[:20]));end=float(np.median(y[-20:]))
            complete=start<.05 and end>.95 if forward else start>.95 and end<.05
            work=t[(y>=.1)&(y<=.9)]
            if not complete or len(work)<100 or abs(float(np.corrcoef(work[:,0],work[:,1])[0,1]))<.98 or np.max(np.abs(np.diff(y)))>.1:
                valid=False;break
        if not valid:continue
        c2=changes.get((path,'2'),999);c4=changes.get((path,'4'),999)
        group='Высокая_устойчивость' if c2<=.5 and c4<=1.5 else 'Умеренная_устойчивость' if 1<=c2<=3 and c4<=8 else None
        if group:candidates[group].append((source,c2,c4,rows))
    for group,items in candidates.items():
        # Distinct device directories; prefer diversity of source sites, then smaller changes.
        items.sort(key=lambda r:r[1]);selected=[];sites=set();devices=set()
        for diversity in [True,False]:
            for item in items:
                source=item[0];parts=source.relative_to(study.ARCHIVE).parts
                site=parts[1] if parts[0] in ['2023','2024'] else parts[0]
                device=str(source.parent.parent)
                if device in devices or diversity and site in sites:continue
                selected.append(item);sites.add(site);devices.add(device)
                if len(selected)==5:break
            if len(selected)==5:break
        if len(selected)!=5: raise ValueError(f'Not enough candidates in {group}: {len(selected)}')
        for number,(source,c2,c4,rows) in enumerate(selected,1):
            device=source.parent.parent.name if source.parent.name[:1].isdigit() and '_' in source.parent.name else source.parent.name
            folder=Path(group)/f'{number:02d}_{device}'
            text=f'Источник: {source}\nГруппа: {group}. Это группа вычислительной устойчивости, не диагноз.\nМаксимум изменения D_max по двум ходам и смещениям: каждая 2-я точка — {c2:.4f} п.п.; каждая 4-я — {c4:.4f} п.п.\n'
            text+='\nКритерии предварительного отбора: полная тройка файлов; >=500 точек на ход; 29 пригодных окон обоих ходов; D_max <=30%; начало/конец в крайних 5% размаха; |корреляция давления и положения в рабочей части| >=0.98; соседний скачок положения <=10% размаха. Это эвристики для просмотра, а не подтверждение качества испытания.\n'
            for r in rows:text+=f"{r['direction']}: k={float(r['k']):.4f}; D_max={float(r['d_max_pct']):.4f}%; D_RMS={float(r['d_rms_pct']):.4f}%; подпись оси {r['y_format']}.\n"
            add(folder,source,text,new=True)
        (stage/group/'00_Критерии.txt').write_text(('Высокая: изменение при прореживании вдвое <=0.5 п.п., вчетверо <=1.5 п.п.' if group.startswith('Высокая') else 'Умеренная: изменение при прореживании вдвое от 1 до 3 п.п., вчетверо <=8 п.п.')+'\nПять примеров для визуального обсуждения, выбранных целенаправленно; не репрезентативная оценка всего архива. Визуальные замечания записывать в 02_Комментарий.txt.\n')
    (stage/'Исключения_эксперта.json').write_text(json.dumps(excluded,ensure_ascii=False,indent=2)+'\n')
    (stage/'Реестр_трёх_графиков.json').write_text(json.dumps(registry,ensure_ascii=False,indent=2)+'\n')
    (stage/'capture.json').write_text(json.dumps(capture,ensure_ascii=False,indent=2)+'\n')
    (stage/'О_подборках.txt').write_text('Две новые подборки по 5 примеров: Умеренная_устойчивость и Высокая_устойчивость. Это устойчивость расчёта при прореживании, не оценка исправности.\nИсходная подборка из 10 дополнена доступными графиками задания и трения. Для 5 старых случаев Task отсутствует в исходной папке.\nСлучай 09 исключён по замечанию пользователя; исторический отчёт archive01 рассчитан до этой отметки.\nPNG получены непосредственно DataViewer: отдельный снимок каждого доступного графика и общий вид. Исходные .data не изменяются.\n')
    print(stage)


if __name__=='__main__':main()
