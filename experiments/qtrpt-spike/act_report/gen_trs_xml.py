#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Генератор trs.xml — шаблон "Акт теста на разрешающую способность" (ТРС)
под движок QtRPT. Четвёртый тип акта (после ТО, ТШР, ТЧР — см. те же
файлы). Одна страница: шапка+график+результаты в одном ReportTitle, как
у ТШР/ТЧР.

ТРС соответствует Widgets::Chart::ChartType::Resolution (Manager.cpp::
createResolutionChart — та же пара рядов "Задание"/"Датчик линейных
перемещений", что и у Response/Step).

Структура и реальные значения — из report Илим.xlsx, лист "ТРС" (тот же
реальный отчёт, что и для gen_tchr_xml.py — см. его докстринг про
актуальную структуру "Ошибка на шаг, %" без отдельной строки "Допустимая
ошибка"). В отличие от ТЧР — тут НЕТ итоговой строки "Ошибка по
положению, %" (в реальном файле её после блоков результатов просто нет),
и шаги в обоих направлениях называются одинаково ("Ступенчатый шаг N", не
"Задание +/- шаг N") — воспроизведено 1:1, не унифицировано с ТЧР.

Смысл теста — прогрессивно УМЕНЬШАЮЩИЕСЯ ступени (3% -> 2% -> 1% -> 0.5%,
в отличие от ТЧР, где ступени растут: 3% -> 5% -> 6% -> 6.5%) — проверка
чувствительности клапана к всё более мелким шагам, отсюда и название
"разрешающая способность".
"""

import math
import random  # уже засеян (random.seed(42)) в report_common
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
        "Акт теста на разрешающую способность, поз. [valvePosition]",
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

# ==================== График теста на разрешающую способность ==========
# Форма — по референс-скриншоту пользователя (Resolution_1.bmp): в
# отличие от ТЧР (несколько КРУПНЫХ ступеней по нарастающей), тут после
# выхода на базу — серия МЕЛКИХ ступеней-реверсов вокруг одного уровня
# (проверка чувствительности к маленьким приращениям, а не к большому
# ходу). Величины — реальные дельты ТРС (report Илим.xlsx): 3/2/1/0.5%,
# УБЫВАЮЩИЕ (у ТЧР — растущие 3/5/6/6.5%), в этом и есть разница между
# "чувствительность реагирования" и "разрешающая способность".
STEP_DURATION = 12.0
BASE_LEVEL = 50.0
UP_DELTAS = [3.0, 2.0, 1.0, 0.5]
DOWN_DELTAS = [3.0, 2.0, 1.0, 0.5]

STEP_LEVELS_UP = [BASE_LEVEL] + [BASE_LEVEL + d for d in UP_DELTAS]
STEP_LEVELS_DOWN = [BASE_LEVEL] + [BASE_LEVEL - d for d in DOWN_DELTAS]
# Референс-график — не монотонный подъём+спуск, а серия небольших
# реверсов около базового уровня (проверка мелких шагов туда-обратно).
STEP_LEVELS = (
    [0.0]
    + STEP_LEVELS_UP
    + list(reversed(STEP_LEVELS_UP[:-1]))
    + list(reversed(STEP_LEVELS_DOWN))
    + STEP_LEVELS_DOWN[1:]
    + [0.0]
)

def step_response(prev_level, target_level, dt, *, tau=0.5, freq=1.4):
    return target_level + (prev_level - target_level) * math.exp(-dt / tau) * math.cos(freq * dt)

setpoint_curve = []
sensor_curve = []
prev_level = 0.0
POINTS_PER_STEP = 40
t = 0.0
for level in STEP_LEVELS:
    t_start = t
    t_end = t + STEP_DURATION
    setpoint_curve.append((round(t_start, 3), level))
    setpoint_curve.append((round(t_end, 3), level))

    for i in range(POINTS_PER_STEP):
        dt = STEP_DURATION * i / (POINTS_PER_STEP - 1)
        noise = (random.random() - 0.5) * 0.3
        val = step_response(prev_level, level, dt) + noise
        sensor_curve.append((round(t_start + dt, 3), round(max(0.0, val), 2)))

    prev_level = level
    t = t_end

content_fields = []
top = 0
content_fields.append(plain_text(0, top, PAGE_W, 18,
    "ГРАФИК ТЕСТА НА РАЗРЕШАЮЩУЮ СПОСОБНОСТЬ", bold=True))
top += 18 + 8

CHART_W, CHART_H = 760, 280
content_fields.append(diagram_field(0, top, CHART_W, CHART_H, "",
    [("Задание", setpoint_curve, APP_COLORS["setpoint"]),
     ("Датчик линейных перемещений", sensor_curve, APP_COLORS["linear"])],
    x_axis_unit="s", y_axis_unit="%", field_name="chart_resolution"))
top += CHART_H + 16

# ==================== Результаты теста ====================
content_fields.append(plain_text(0, top, PAGE_W, 18,
    "РЕЗУЛЬТАТЫ ТЕСТА НА РАЗРЕШАЮЩУЮ СПОСОБНОСТЬ", bold=True))
top += 18 + 16
RES_TOP = top

COL_LABEL, COL_STEP = 190, 114
TABLE_W = COL_LABEL + COL_STEP * 5

STEP_HEADERS = ["Исходное значение", "Ступенчатый шаг 1", "Ступенчатый шаг 2",
                "Ступенчатый шаг 3", "Ступенчатый шаг 4"]

def direction_table(top, title, target_texts, fact_values, error_values):
    fields = []
    fields.append(plain_text(0, top, TABLE_W, 18, title, bold=True))
    top += 18 + 4

    cx = 0
    fields.append(header_cell(cx, top, COL_LABEL, 22, "Положение хода клапана, %", border_right=False))
    cx += COL_LABEL
    for i, htext in enumerate(STEP_HEADERS):
        last = (i == len(STEP_HEADERS) - 1)
        fields.append(header_cell(cx, top, COL_STEP, 22, htext, border_right=None if last else False))
        cx += COL_STEP
    top += 22

    rows = [
        ("Задание, %", target_texts, False),
        ("Факт, %", fact_values, False),
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
    return fields, top

left_fields, left_bottom = direction_table(
    RES_TOP, "Восходящее движение",
    ["50.00", "50.00 + 3.00", "50.00 + 2.00", "50.00 + 1.00", "50.00 + 0.50"],
    ["50.59", "54.02", "53.30", "52.29", "51.78"],
    [None, "14.33", "35.50", "70.00", "138.00"],
)
content_fields += left_fields

right_fields, right_bottom = direction_table(
    left_bottom + 16, "Нисходящее движение",
    ["50.00", "50.00 - 3.00", "50.00 - 2.00", "50.00 - 1.00", "50.00 - 0.50"],
    ["50.59", "48.44", "49.52", "50.41", "51.22"],
    [None, "28.33", "46.50", "82.00", "226.00"],
)
content_fields += right_fields

CONTENT_HEIGHT = right_bottom

header_band = band("PageHeader", HEADER_FIELDS, name="TrsHeaderBand", height=HEADER_H)
title_band = band("ReportTitle", content_fields, name="TrsContentBand", height=CONTENT_HEIGHT)
footer_band = band("PageFooter", footer_fields(), name="TrsFooterBand", height=20)

print(f"# ТРС высота: {HEADER_H + CONTENT_HEIGHT} из {1108} на странице")

xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<Reports email="" lib="QtRPT" programmer="MpiSdk">
  <Report border="0" borderColor="rgba(0,0,0,255)" borderStyle="solid" borderWidth="1" marginsBottom="40" marginsLeft="40" marginsRight="40" marginsTop="40" orientation="0" pageHeight="1188" pageNo="1" pageWidth="840" watermark="0">
    {header_band}
    {title_band}
    {footer_band}
  </Report>
</Reports>
'''

with open("trs.xml", "w", encoding="utf-8") as f:
    f.write(xml)

print(f"Написано trs.xml, высота шапки: {HEADER_H}, график: {CHART_H}, таблицы результатов: {CONTENT_HEIGHT}")
