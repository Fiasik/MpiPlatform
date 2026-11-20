#!/usr/bin/env python3
"""Small, reproducible sensitivity pilot; stdlib except optional plotting."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
SOURCE = REPO / 'Src/Domain/Tests/Main/Linearity/LinearityMethods.cpp'
if not SOURCE.exists():
    SOURCE = Path('/media/vladislav-b/Tree/Library/GitHub/MpiPlatform/Src/Domain/Tests/Main/Linearity/LinearityMethods.cpp')
DEFAULT_DATA = Path('/home/vladislav-b/Documents/Математика_основы/_данные/PV-2223')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_points(path):
    with path.open(encoding='utf-8-sig', newline='') as stream:
        rows = list(csv.DictReader(stream))
    points = [(float(row['x']), float(row['y'])) for row in rows]
    if not points or not all(math.isfinite(v) for p in points for v in p):
        raise ValueError(f'Empty or non-finite measurements: {path}')
    return points


def fit(points):
    if len(points) < 2:
        return None
    mx = math.fsum(x for x, _ in points) / len(points)
    my = math.fsum(y for _, y in points) / len(points)
    sxx = math.fsum((x - mx) ** 2 for x, _ in points)
    if sxx == 0:
        return None
    k = math.fsum((x - mx) * (y - my) for x, y in points) / sxx
    b = my - k * mx
    return (k, b) if math.isfinite(k) and math.isfinite(b) else None


def calculate(points, bounds, width_fraction=0.10, step_fraction=0.025):
    low, high = bounds
    span = high - low
    result = {'valid': False, 'reason': None, 'points': len(points), 'windows': [],
              'd_max_pct': None, 'd_rms_pct': None}
    if span <= 0 or width_fraction <= 0 or step_fraction <= 0:
        result['reason'] = 'invalid_range_or_settings'
        return result
    work_low, work_high = low + 0.1 * span, high - 0.1 * span
    selected = [(x, y) for x, y in points if work_low <= y <= work_high]
    line = fit(selected)
    result.update(work_points=len(selected), work_low=work_low, work_high=work_high)
    if line is None or abs(line[0]) <= 1e-12:
        result['reason'] = 'undefined_or_numerically_zero_global_slope'
        return result
    k, b = line
    result.update(k=k, b=b)
    lo = work_low
    attempted = 0
    while lo + width_fraction * span <= work_high + 1e-9:
        hi = lo + width_fraction * span
        segment = [(x, y) for x, y in selected if lo <= y <= hi]
        local = fit(segment) if len(segment) >= 8 else None
        attempted += 1
        if local is not None:
            deviation = (local[0] - k) / abs(k) * 100
            result['windows'].append({
                'low_position': lo, 'high_position': hi, 'points': len(segment),
                'pressure_span_bar': max(x for x, _ in segment) - min(x for x, _ in segment),
                'k_position_per_bar': local[0], 'signed_deviation_pct': deviation,
                'delta_k_position_per_bar': local[0] - k,
            })
        lo += step_fraction * span
    windows = result['windows']
    result.update(attempted_windows=attempted, skipped_windows=attempted-len(windows))
    if not windows:
        result['reason'] = 'no_usable_local_windows'
        return result
    ds = [w['signed_deviation_pct'] for w in windows]
    result.update(valid=True, d_max_pct=max(abs(d) for d in ds),
                  d_rms_pct=math.sqrt(math.fsum(d*d for d in ds)/len(ds)))
    worst = max(windows, key=lambda w: abs(w['delta_k_position_per_bar']))
    result.update(worst_window=worst,
                  delta_k_max_position_per_bar=abs(worst['delta_k_position_per_bar']),
                  delta_k_rms_position_per_bar=math.sqrt(math.fsum(w['delta_k_position_per_bar']**2 for w in windows)/len(windows)))
    return result


def verify():
    """Analytical checks meaningful for slope normalization and degenerate inputs."""
    points = [(i/100, 3*i/100+2) for i in range(401)]
    baseline = calculate(points, (2, 14))
    assert baseline['valid'] and baseline['d_max_pct'] < 1e-8
    negative = calculate([(x, -y) for x, y in points], (-14, -2))
    assert negative['valid'] and negative['d_max_pct'] < 1e-8
    assert not calculate([(1, y) for _, y in points], (2, 14))['valid']
    curved = [(x, y + 0.15*x*x) for x, y in points]
    bounds = (min(y for _, y in curved), max(y for _, y in curved))
    first = calculate(curved, bounds)
    shifted = [(2*x+4, 5*y-7) for x, y in curved]
    second = calculate(shifted, (5*bounds[0]-7, 5*bounds[1]-7))
    assert first['valid'] and second['valid']
    assert math.isclose(first['d_max_pct'], second['d_max_pct'], abs_tol=1e-8)
    assert math.isclose(first['d_rms_pct'], second['d_rms_pct'], abs_tol=1e-8)


def write_csv(path, rows):
    with path.open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def plot_lesson(out, datasets, baseline):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams['font.family'] = 'DejaVu Sans'
    fig, axes = plt.subplots(1, 2, figsize=(13, 5), layout='constrained')
    colors = {'forward': '#226e9c', 'backward': '#bd6330'}
    names = {'forward': 'Прямой ход', 'backward': 'Обратный ход'}
    for direction, points in datasets.items():
        result = baseline[direction]
        axes[0].plot(*zip(*points), color=colors[direction], lw=1, label=names[direction])
        if not result['valid']:
            continue
        windows = result['windows']
        axes[1].plot([(w['low_position']+w['high_position'])/2 for w in windows],
                     [w['k_position_per_bar'] for w in windows], '.-',
                     color=colors[direction], label=names[direction])
        axes[1].axhline(result['k'], color=colors[direction], ls='--', lw=1)
    axes[0].set(xlabel='Давление, бар', ylabel='Положение, ед. записи', title='Исходные измерения PV-2223')
    axes[1].set(xlabel='Середина расчётного участка, ед. записи', ylabel='Наклон, ед. записи/бар',
                title='Локальные наклоны; пунктир — общий наклон')
    for ax in axes:
        ax.legend()
        ax.grid(alpha=0.2)
    fig.savefig(out/'lesson.png', dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--forward', type=Path, default=DEFAULT_DATA/'Pressure_1__dubl0.csv')
    parser.add_argument('--backward', type=Path, default=DEFAULT_DATA/'Pressure_1__0.csv')
    parser.add_argument('--out', type=Path, default=ROOT/'results')
    parser.add_argument('--plot', action='store_true')
    args = parser.parse_args()
    verify()
    paths = {'forward': args.forward.resolve(), 'backward': args.backward.resolve()}
    datasets = {name: read_points(path) for name, path in paths.items()}
    ys = [y for points in datasets.values() for _, y in points]
    bounds = min(ys), max(ys)
    baseline = {name: calculate(points, bounds) for name, points in datasets.items()}
    if not all(result['valid'] for result in baseline.values()):
        raise ValueError('Baseline invalid: '+str({k:v['reason'] for k,v in baseline.items()}))
    cases = []
    for name, points in datasets.items():
        for width in [0.05, 0.10, 0.15, 0.20]:
            cases.append((name, 'width', width, 1, 0, calculate(points, bounds, width)))
        for stride in [2, 4, 8]:
            for offset in range(stride):
                cases.append((name, 'decimation', 0.10, stride, offset,
                              calculate(points[offset::stride], bounds)))
    rows = []
    for name, experiment, width, stride, offset, result in cases:
        rows.append({'direction':name, 'experiment':experiment, 'width_fraction':width,
                     'stride':stride, 'offset':offset, 'valid':result['valid'],
                     'reason':result['reason'], 'points':result['points'],
                     'usable_windows':len(result['windows']),
                     'd_max_pct':result['d_max_pct'], 'd_rms_pct':result['d_rms_pct']})
    args.out.mkdir(parents=True, exist_ok=True)
    manifest = {'data': {name:{'path':str(path), 'sha256':digest(path)} for name,path in paths.items()},
                'formula_source':str(SOURCE), 'formula_source_sha256':digest(SOURCE),
                'script_sha256':digest(Path(__file__)), 'bounds_position':bounds,
                'minimum_points':8, 'step_fraction':0.025, 'baseline_width_fraction':0.10,
                'interpretation':'single-record numerical sensitivity, not independent repeated tests',
                'baseline':baseline}
    (args.out/'baseline.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2,
                                                   allow_nan=False)+'\n', encoding='utf-8')
    write_csv(args.out/'sensitivity.csv', rows)
    write_csv(args.out/'windows.csv', [{'direction':name, **w} for name,r in baseline.items()
                                      for w in r['windows']])
    lines = ['# Первый вычислительный пилот: PV-2223', '',
             'Это одна запись с двумя ходами. Независимые повторные испытания не проверялись.',
             'Проценты рассчитаны в единицах записи. Физическая единица положения не подтверждена: в .data указано deg, в учебных материалах — мм. Направления приняты по документации приложения.',
             'Диагнозов и допустимых значений по этим результатам не устанавливаем.', '',
             f'Полный размах положения по CSV: {bounds[0]:.6f}…{bounds[1]:.6f} ед. записи.', '',
             '| Ход | Точек рабочей части | Окон | Общий наклон, ед. записи/бар | D_max, % наклона | D_RMS, % наклона |',
             '|---|---:|---:|---:|---:|---:|']
    for name, r in baseline.items():
        lines.append(f"| {name} | {r['work_points']} | {len(r['windows'])} | {r['k']:.4f} | {r['d_max_pct']:.4f} | {r['d_rms_pct']:.4f} |")
    lines += ['', '## Изменение ширины участка', '',
              'Границы рабочего диапазона и шаг 2,5 % полного хода зафиксированы.', '',
              '| Ход | Ширина, % полного хода | D_max, % | D_RMS, % | Окон |', '|---|---:|---:|---:|---:|']
    for name, experiment, width, _, _, r in cases:
        if experiment == 'width' and r['valid']:
            lines.append(f"| {name} | {width*100:g} | {r['d_max_pct']:.4f} | {r['d_rms_pct']:.4f} | {len(r['windows'])} |")
    lines += ['', '## Прореживание', '',
              'Все начальные смещения; исходные границы и настройки зафиксированы. Диапазон по смещениям не является доверительным интервалом.', '',
              '| Ход | Каждая n-я точка | Пригодно / вариантов | D_max min…max, % | D_RMS min…max, % |', '|---|---:|---:|---:|---:|']
    for name in datasets:
        for stride in [2, 4, 8]:
            group = [r for n,e,_,s,_,r in cases if n==name and e=='decimation' and s==stride and r['valid']]
            def interval(key):
                return f"{min(r[key] for r in group):.4f}…{max(r[key] for r in group):.4f}" if group else 'нет результата'
            lines.append(f"| {name} | {stride} | {len(group)} / {stride} | {interval('d_max_pct')} | {interval('d_rms_pct')} |")
    lines += ['', '## Что ещё не установлено', '',
              'Нет вывода об устойчивости на архиве, физической повторяемости, соответствии C++ до последнего знака или связи со здоровьем привода. Шум, выбросы и изменение рабочего диапазона ещё не исследованы.', '',
              'Аналитические проверки скрипта пройдены: прямые обоих знаков, вырожденный вход, инвариантность к сдвигу и положительному масштабированию осей.', '']
    (args.out/'pilot.md').write_text('\n'.join(lines), encoding='utf-8')
    if args.plot:
        plot_lesson(args.out, datasets, baseline)
    print(args.out/'pilot.md')


if __name__ == '__main__':
    main()
