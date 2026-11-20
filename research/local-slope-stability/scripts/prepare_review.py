"""Prepare a non-destructive review package in /tmp; director comments start blank."""
import csv
import hashlib
import json
import shutil
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


def main():
    package=Path(tempfile.mkdtemp(prefix='mpi-slope-review-'))
    shutil.copytree(ROOT,package/'Исследование',ignore=shutil.ignore_patterns('__pycache__'))
    results=ROOT/'results/archive01'
    with (results/'per_record_decimation.csv').open() as f:
        records=[r for r in csv.DictReader(f) if r['stride']=='2' and r['max_abs_dmax_change_pp']]
    records.sort(key=lambda r:float(r['max_abs_dmax_change_pp']),reverse=True)
    with (results/'baseline.csv').open() as f:
        baselines={(r['path'],r['direction']):r for r in csv.DictReader(f)}
    with (results/'sensitivity.csv').open() as f:
        cases=[r for r in csv.DictReader(f) if r['experiment']=='decimation' and r['stride']=='2' and r['valid']=='True']
    folder=package/'Графики_для_обсуждения';folder.mkdir()
    manifest=[]
    index=['ПЕРВАЯ ПОДБОРКА ДЛЯ ОБСУЖДЕНИЯ', '',
           '10 уникальных записей с наибольшим абсолютным изменением D_max при оставлении каждой второй точки. Для записи выбран худший вариант по двум ходам и двум начальным смещениям.',
           'Это целенаправленная подборка крайних случаев. Она не характеризует долю неисправностей и не означает, что формула или устройство непригодны.',
           'Исходные .data скопированы без изменения. Прореженные графики вместо исходных не подставлялись. Смещение 0 — индексы 0,2,4…; смещение 1 — 1,3,5…',
           'В каждой папке: .data для просмотра; 01_Описание.txt с происхождением и расчётом; 02_Комментарий.txt для вашей записи.', '']
    for number,record in enumerate(records[:10],1):
        path=Path(record['path'])
        case=max((c for c in cases if c['path']==str(path)),key=lambda c:abs(float(c['change_dmax_pp'])))
        base=baselines[str(path),case['direction']]
        tag=path.parent.parent.name.replace('/','_')
        sub=folder/f'{number:02d}_{tag}_{path.parent.name}';sub.mkdir()
        target=sub/path.name;shutil.copy2(path,target)
        sha=hashlib.sha256(path.read_bytes()).hexdigest()
        assert sha==hashlib.sha256(target.read_bytes()).hexdigest()
        direction={'forward':'прямой','backward':'обратный'}[case['direction']]
        description=[f'СЛУЧАЙ {number:02d}: {tag}', '',f'Исходный файл: {path}',f'SHA-256: {sha}',
                     f'Отображённый формат единицы положения: {base["y_format"]}. Это подпись файла, не независимая проверка единицы.',
                     f'Выбранный ход: {direction}; начальное смещение прореживания: {case["offset"]}.', '',
                     'ПРИЧИНА ОТБОРА: большое изменение относительного показателя при прореживании одной и той же записи вдвое.',
                     'При просмотре .data вы увидите исходные кривые. Цифры после прореживания получены исследовательским скриптом при фиксированных исходных границах.', '',
                     'Величина | Все точки | Каждая вторая точка',
                     f'Общий наклон, ед. положения/бар | {float(base["k"]):.6f} | {float(case["k"]):.6f}',
                     f'Максимальная разность наклонов, ед. положения/бар | {float(base["delta_max_position_per_bar"]):.6f} | {float(case["delta_max_position_per_bar"]):.6f}',
                     f'D_max, % общего наклона | {float(base["d_max_pct"]):.3f} | {float(case["d_max_pct"]):.3f}',
                     f'D_RMS, % общего наклона | {float(base["d_rms_pct"]):.3f} | {float(case["d_rms_pct"]):.3f}',
                     f'Пригодных окон / всех | {base["windows"]}/{base["attempted_windows"]} | {case["windows"]}/{case["attempted_windows"]}',
                     f'Изменение D_max: {float(case["change_dmax_pp"]):+.3f} процентного пункта.', '',
                     'Нужно проверить: соответствует ли график режиму испытания; корректны ли единицы и направление; был ли полноценный проход; есть ли известные особенности оборудования или регистрации.',
                     'Причина поведения и техническое состояние заранее не установлены. Нельзя считать снижение показателя улучшением состояния.','']
        (sub/'01_Описание.txt').write_text('\n'.join(description),encoding='utf-8')
        (sub/'02_Комментарий.txt').write_text('\n'.join([
            f'КОММЕНТАРИЙ К СЛУЧАЮ {number:02d}: {tag}', '',
            'Дата обсуждения: ', 'Участники: ', '',
            '1. Что это за устройство и что известно о его состоянии?', '',
            '2. Подтверждённые единицы осей и смысл измерений:', '',
            '3. Условия испытания, скорость/выдержка, подключение датчиков:', '',
            '4. Что видно на графике? Где именно (координаты или участок)?', '',
            '5. Есть ли неполный проход, помеха, сбой регистрации или другая особенность?', '',
            '6. Предположение о причине (отдельно от установленного факта):', '',
            '7. Чем можно подтвердить объяснение: паспорт, акт, повторное испытание?', '',
            '8. Решение для исследования: оставить / исключить с причиной / пока неясно:', '',
            '9. Что ещё проверить:', '',
            '10. Можно ли использовать график и комментарий в НИР/публикации? Условия:', '',
            'Свободный комментарий:', '',
        ]),encoding='utf-8')
        manifest.append(dict(folder=sub.name,source=str(path),sha256=sha,direction=case['direction'],
                             offset=int(case['offset']),change_dmax_pp=float(case['change_dmax_pp'])))
        index.append(f'{sub.name}: {float(case["change_dmax_pp"]):+.3f} п.п.; {direction} ход.')
    (folder/'00_Как_работать.txt').write_text('\n'.join(index)+'\n',encoding='utf-8')
    (folder/'Реестр.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (package/'НАЧАТЬ_ЗДЕСЬ.txt').write_text('\n'.join([
        'ХАРАКТЕРИСТИКА ЛОКАЛЬНОЙ КРУТИЗНЫ', '',
        'Графики_для_обсуждения — первая подборка 10 крайних случаев с исходными .data и пустыми шаблонами комментариев.',
        'Исследование — материалы, формулы, расчётные скрипты и отчёты текущего исследования.',
        'Начать объяснение: Исследование/01_Что_мы_считаем.md.',
        'Сводка архива: Исследование/results/archive01/report.md.',
        'Папка в Documents становится основным местом для обсуждения и комментариев. Репозиторная версия пока сохранена как предыдущая рабочая копия.',
        'Пользовательские комментарии не перезаписывать при последующих обновлениях.',
        'Скрипты можно запускать из папки Исследование: python3 scripts/pilot.py; python3 scripts/archive_study.py. NumPy нужен для архива. Формула C++ и исходные измерения пока читаются по прежним абсолютным путям на этом компьютере.',
        'Новые подозрительные записи добавлять отдельными папками с источником, причиной отбора и шаблоном комментария. Причину отбора не подменять диагнозом.', '',
    ]),encoding='utf-8')
    print(package)


if __name__=='__main__':main()
