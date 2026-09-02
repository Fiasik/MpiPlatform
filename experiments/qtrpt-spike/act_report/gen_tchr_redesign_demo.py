#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ДЕМО-ВАРИАНТ ТЧР, НЕ замена gen_tchr_xml.py — показывает, как выглядела
бы страница с учётом критики структуры таблицы (см. обсуждение). Три
конкретные правки, все на реальных числах из report Илим.xlsx:

1. "Отклонение, %" — новая строка, |Факт - Задание|, честно посчитана из
   двух уже известных реальных чисел (это НЕ то же самое, что "Ошибка на
   шаг, %" — та колонка уже готовое отношение к какому-то допуску,
   формулу которого я НЕ смог восстановить обратным счётом: допуск,
   посчитанный как отклонение/(ошибка/100), даёт 2.5/6.7/4.9/1.0 на
   восходящем и 8.9/6.7/14.7/6.6 на нисходящем — никакой связи со
   величиной шага или направлением не нашлось. Так что честно: "Ошибка
   на шаг, %" остаётся как есть, а "Отклонение, %" — это ЧТО РЕАЛЬНО
   ИЗМЕРЕНО, независимо от того, как именно считается допуск.
2. Заголовки шагов — "Шаг N (+X.XX%)" вместо "Задание + шаг 1"/"Задание 1
   + шаг 2" — раньше нумерация "Задание 1/2/3" выглядела так, будто шаги
   накопительные, хотя по формулам в Excel каждый независимо строится от
   базы 50%.
3. Итоговая строка-вердикт — то же зелёное/красное, что и у ТО
   ("соответствует"/"не соответствует"), а не только точечная подсветка
   ячеек: "N из M шагов вне допуска".
"""

import random
from report_common import (
    header_cell, value_cell, plain_text, two_col_row,
    band, APP_COLORS, diagram_field, footer_fields, spec_table,
)

PAGE_W = 760
LEFT_X, LEFT_W = 0, 372
RIGHT_X, RIGHT_W = 388, 372
LABEL_W = 140

COLOR_OK = "rgba(198,239,206,255)"
COLOR_FAIL = "rgba(255,199,206,255)"

def error_bg(text):
    if text is None:
        return "rgba(255,255,255,255)"
    return COLOR_FAIL if float(text) > 100 else COLOR_OK


def page_header_fields():
    fields = []
    fields.append(plain_text(0, 0, 520, 24,
        "Акт теста на чувствительность реагирования, поз. [valvePosition]",
        bold=True, size=11))
    fields.append(plain_text(520, 0, 240, 24, "установки [objectTag]",
        bold=True, align="hCenter", size=11))

    top0 = 32
    fields.append(header_cell(LEFT_X, top0, LEFT_W, 22, "Данные по объекту"))
    top_left = top0 + 22
    obj_rows = [
        ("Объект", "[objectName]"),
        ("Цех", "[objectShop]"),
        ("Установка", "[objectInstall]"),
    ]
    for label, value in obj_rows:
        fields += two_col_row(LEFT_X, top_left, LABEL_W, LEFT_W - LABEL_W, 20, label, value)
        top_left += 20

    spec_fields, top_right = spec_table(RIGHT_X, top0, RIGHT_W, LABEL_W,
                                         header="Краткая спецификация на клапан",
                                         input_signal_label="Привод, ПБ")
    fields += spec_fields

    bottom = max(top_left, top_right) + 12
    return fields, bottom


HEADER_FIELDS, HEADER_H = page_header_fields()

STEP_DURATION = 12.0
BASE_LEVEL = 50.0
UP_DELTAS = [3.0, 5.0, 6.0, 6.5]
DOWN_DELTAS = [3.0, 5.0, 6.0, 6.5]
STEP_LEVELS_UP = [BASE_LEVEL] + [BASE_LEVEL + d for d in UP_DELTAS]
STEP_LEVELS_DOWN = [BASE_LEVEL] + [BASE_LEVEL - d for d in DOWN_DELTAS]
STEP_LEVELS = [0.0] + STEP_LEVELS_UP + list(reversed(STEP_LEVELS_DOWN)) + [0.0]

def step_response(prev_level, target_level, dt, *, tau=0.6, freq=1.2):
    import math
    return target_level + (prev_level - target_level) * math.exp(-dt / tau) * math.cos(freq * dt)

setpoint_curve, sensor_curve = [], []
prev_level, t = 0.0, 0.0
POINTS_PER_STEP = 50
for level in STEP_LEVELS:
    t_start, t_end = t, t + STEP_DURATION
    setpoint_curve.append((round(t_start, 3), level))
    setpoint_curve.append((round(t_end, 3), level))
    for i in range(POINTS_PER_STEP):
        dt = STEP_DURATION * i / (POINTS_PER_STEP - 1)
        noise = (random.random() - 0.5) * 0.4
        val = step_response(prev_level, level, dt) + noise
        sensor_curve.append((round(t_start + dt, 3), round(max(0.0, val), 2)))
    prev_level = level
    t = t_end

content_fields = []
top = 0
content_fields.append(plain_text(0, top, PAGE_W, 18,
    "ГРАФИК ТЕСТА НА ЧУВСТВИТЕЛЬНОСТЬ РЕАГИРОВАНИЯ", bold=True))
top += 18 + 8
CHART_W, CHART_H = 760, 280
content_fields.append(diagram_field(0, top, CHART_W, CHART_H, "",
    [("Задание", setpoint_curve, APP_COLORS["setpoint"]),
     ("Датчик линейных перемещений", sensor_curve, APP_COLORS["linear"])],
    x_axis_unit="s", y_axis_unit="%", field_name="chart_response"))
top += CHART_H + 16

content_fields.append(plain_text(0, top, PAGE_W, 18,
    "РЕЗУЛЬТАТЫ ТЕСТА НА ЧУВСТВИТЕЛЬНОСТЬ РЕАГИРОВАНИЯ (ДЕМО-ВАРИАНТ)", bold=True))
top += 18 + 16
RES_TOP = top

COL_LABEL, COL_STEP = 190, 114
TABLE_W = COL_LABEL + COL_STEP * 5

def direction_table(top, title, deltas, sign, target_texts, fact_values, error_values):
    fields = []
    fields.append(plain_text(0, top, TABLE_W, 18, title, bold=True))
    top += 18 + 4

    headers = ["Исходное значение"] + [f"Шаг {i+1} ({sign}{d:.2f}%)" for i, d in enumerate(deltas)]
    cx = 0
    fields.append(header_cell(cx, top, COL_LABEL, 22, "Положение хода клапана, %", border_right=False))
    cx += COL_LABEL
    for i, htext in enumerate(headers):
        last = (i == len(headers) - 1)
        fields.append(header_cell(cx, top, COL_STEP, 22, htext, border_right=None if last else False))
        cx += COL_STEP
    top += 22

    # Отклонение = Факт - Задание, честно посчитано из целевого и
    # фактического значений (пропускаем "Исходное значение" — это точка
    # отсчёта, не отдельный шаг с допуском). Задание считаем напрямую из
    # BASE_LEVEL±delta, а не парсингом строки "50.00 + 3.00" — она только
    # для отображения, и парсить её обратно в число легко сломать
    # (сначала так и сделал: replace(sign,"") на "50.00 - 3.00" стирает
    # именно тот "-", который и есть данные, оставляя "50.00  3.00" —
    # .split()[0] тогда просто возвращает базу, а не реальную цель).
    targets = [BASE_LEVEL] + [BASE_LEVEL + (d if sign == "+" else -d) for d in deltas]
    facts = [float(f) for f in fact_values]
    deviations = [None] + [round(facts[i] - targets[i], 2) for i in range(1, len(targets))]

    rows = [
        ("Задание, %", target_texts, False),
        ("Факт, %", fact_values, False),
        ("Отклонение, %", [f"{d:+.2f}" if d is not None else None for d in deviations], False),
        ("Ошибка на шаг, %", error_values, True),
    ]
    for label, values, highlighted in rows:
        cx = 0
        fields.append(value_cell(cx, top, COL_LABEL, 22, label, border_right=False, border_top=False))
        cx += COL_LABEL
        for i, val in enumerate(values):
            last = (i == len(values) - 1)
            text = "" if val is None else str(val)
            kw = {"bg": error_bg(val)} if highlighted else {}
            fields.append(value_cell(cx, top, COL_STEP, 22, text, align="hCenter",
                                      border_right=None if last else False, border_top=False, **kw))
            cx += COL_STEP
        top += 22
    return fields, top, error_values

left_fields, left_bottom, up_errors = direction_table(
    RES_TOP, "Восходящее движение", UP_DELTAS, "+",
    ["50.00", "50.00 + 3.00", "50.00 + 5.00", "50.00 + 6.00", "50.00 + 6.50"],
    ["49.94", "53.33", "55.47", "56.59", "57.63"],
    [None, "13.00", "7.00", "12.00", "108.00"],
)
content_fields += left_fields

right_fields, right_bottom, down_errors = direction_table(
    left_bottom + 16, "Нисходящее движение", DOWN_DELTAS, "-",
    ["50.00", "50.00 - 3.00", "50.00 - 5.00", "50.00 - 6.00", "50.00 - 6.50"],
    ["50.73", "48.10", "46.57", "45.47", "45.09"],
    [None, "12.33", "23.50", "10.00", "24.00"],
)
content_fields += right_fields

# ==================== Итоговый вердикт ====================
# Тот же приём, что и verdict_highlight() в ТО — человекочитаемая фраза с
# фоном, не только точечная подсветка ячеек. Посчитано из реальных
# "Ошибка на шаг, %" выше (>100 = вне допуска), а не выдумано.
all_errors = [float(v) for v in (up_errors + down_errors) if v is not None]
total_steps = len(all_errors)
failed_steps = sum(1 for v in all_errors if v > 100)
verdict_top = right_bottom + 16
if failed_steps == 0:
    verdict_text = f"Тест пройден: все {total_steps} шагов в пределах допуска"
    verdict_bg = COLOR_OK
else:
    verdict_text = f"Тест не пройден: {failed_steps} из {total_steps} шагов превышает допуск"
    verdict_bg = COLOR_FAIL
content_fields.append(value_cell(0, verdict_top, TABLE_W, 26, verdict_text,
                                  align="hCenter", bold=True, bg=verdict_bg))

CONTENT_HEIGHT = verdict_top + 26

header_band = band("PageHeader", HEADER_FIELDS, name="TchrRedesignHeaderBand", height=HEADER_H)
title_band = band("ReportTitle", content_fields, name="TchrRedesignContentBand", height=CONTENT_HEIGHT)
footer_band = band("PageFooter", footer_fields(), name="TchrRedesignFooterBand", height=20)

print(f"# ТЧР (демо-редизайн) высота: {HEADER_H + CONTENT_HEIGHT} из {1108} на странице")

xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<Reports email="" lib="QtRPT" programmer="MpiSdk">
  <Report border="0" borderColor="rgba(0,0,0,255)" borderStyle="solid" borderWidth="1" marginsBottom="40" marginsLeft="40" marginsRight="40" marginsTop="40" orientation="0" pageHeight="1188" pageNo="1" pageWidth="840" watermark="0">
    {header_band}
    {title_band}
    {footer_band}
  </Report>
</Reports>
'''

with open("tchr_redesign_demo.xml", "w", encoding="utf-8") as f:
    f.write(xml)

print(f"Написано tchr_redesign_demo.xml")
