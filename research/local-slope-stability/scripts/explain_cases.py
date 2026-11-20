"""Reproduce two archive cases explaining sensitivity; no diagnostic labels."""
from pathlib import Path
import json
import numpy as np
import archive_study as study
import pilot


def main():
    targets=[
        ('small_reference', '2024/ЛУКРЙЛ-УНП/ППНН/АТ-1/pм 7254a/16_05_2024/Pressure_1.data', 'forward'),
        ('lost_windows', '2023/ООО ИНК/УКПГ-3/ГПА/15 часть 16 ед/3-SDV-3402/30_08_2023/Pressure_1.data', 'backward')]
    out=study.ROOT/'results/explanations';out.mkdir(parents=True,exist_ok=True)
    records=[]
    for name,relative,direction in targets:
        path=study.ARCHIVE/relative
        data=study.decode(path.read_bytes(),'q');n=len(data['names'])
        f,b=data['arrays'][n],data['arrays'][0]
        bounds=(min(f[:,1].min(),b[:,1].min()),max(f[:,1].max(),b[:,1].max()))
        points=f if direction=='forward' else b
        full=pilot.calculate(points.tolist(),bounds)
        thin=pilot.calculate(points[1::2].tolist(),bounds)
        worst=full['worst_window'];lo=worst['low_position'];hi=worst['high_position']
        remaining=points[1::2]
        remaining_count=int(((remaining[:,1]>=lo)&(remaining[:,1]<=hi)).sum())
        row=dict(case=name,path=str(path),direction=direction,x_format=data['x_format'],
                 y_formats=data['y_formats'],full=full,decimated_offset1=thin,
                 points_remaining_in_original_worst_window=remaining_count)
        if name=='small_reference':
            work=points[(points[:,1]>=full['work_low'])&(points[:,1]<=full['work_high'])]
            row['work_pressure_quantiles']=np.quantile(work[:,0],[0,.01,.5,.99,1]).tolist()
        records.append(row)
    (out/'cases.json').write_text(json.dumps(records,ensure_ascii=False,indent=2,allow_nan=False)+'\n')
    lines=['# Почему число меняется: два реальных примера','',
           'Это целенаправленный разбор крайних случаев, не оценка их распространённости. Формула не объявляется непригодной. Прореживание берёт точки с индексами 1, 3, 5… при исходных границах.', '']
    for row in records:
        a,c=row['full'],row['decimated_offset1']
        lines += [f"## {row['case']}", '', row['path'], '',
                  f"Ход: {row['direction']}; подписи осей: {row['x_format']}, {row['y_formats']}.", '',
                  '| Величина | Все точки | Каждая вторая точка |', '|---|---:|---:|',
                  f"| Общий наклон, ед. положения/бар | {a['k']:.6f} | {c['k']:.6f} |",
                  f"| Максимальная разность наклонов по модулю, ед. положения/бар | {a['delta_k_max_position_per_bar']:.6f} | {c['delta_k_max_position_per_bar']:.6f} |",
                  f"| D_max, % | {a['d_max_pct']:.3f} | {c['d_max_pct']:.3f} |",
                  f"| Пригодных окон | {len(a['windows'])} | {len(c['windows'])} |", '']
        if row['case']=='small_reference':
            lines+=['После прореживания общий наклон приблизился к нулю и поменял знак. Максимальная разность наклонов в единицах записи даже уменьшилась, но деление на почти нулевой общий наклон резко увеличило проценты. В рабочей части давление занимает узкий диапазон. Причина поведения самих измерений и физический режим этим расчётом не установлены.']
        else:
            lines += [f"В исходном худшем окне было {a['worst_window']['points']} точек; после прореживания осталось {row['points_remaining_in_original_worst_window']}. Минимум для расчёта — 8. Поэтому это окно перестало участвовать в максимуме. Меньшее итоговое число не означает улучшения оборудования: изменился набор пригодных окон."]
        lines+=['']
    lines+=['## Следствие для исследования','',
            'Нужны условия пригодности: достаточный диапазон входа и надёжность общего наклона, покрытие окон и явный статус отсутствующего результата. Их пороги ещё не обоснованы. Ширина окна задаёт масштаб анализа; сравнение разных ширин не является само по себе проверкой повторяемости.',
            'Нельзя удалять неудобные случаи после просмотра результата без регистрации причин. Следующий эксперимент должен отдельно показывать результаты для полного покрытия окон и для остальных записей.','']
    (out/'report.md').write_text('\n'.join(lines))
    for r in records:
        print(r['case'],'remaining',r['points_remaining_in_original_worst_window'],
              'original',r['full']['worst_window']['points'])


if __name__=='__main__':main()
