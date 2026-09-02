#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Генератор tchr.xml — шаблон "Акт теста на чувствительность реагирования"
(ТЧР) под движок QtRPT. Третий тип акта (после ТО — gen_act_xml.py, и ТШР —
gen_tshr_xml.py). Как и ТШР — одна страница, шапка+график+результаты в
одном ReportTitle.

ТЧР соответствует Widgets::Chart::ChartType::Response (Manager.cpp::
createResponseChart: useTimeaxis(true), addAxis("%.2f%%"), ряды "Задание"
(чёрный) + "Датчик линейных перемещений" — та же пара рядов, что и у Step).

Структура и реальные значения — не выдуманы, а взяты из уже заполненного
боевого отчёта пользователя (report Илим.xlsx, лист "ТЧР", тест от
08.05.2026) — самый правдоподобный источник, который был под рукой. В
пустом шаблоне Report.xlsx (MpiSkdIlim) когда-то была ещё отдельная строка
"Допустимая ошибка на шаг, %" между "Факт" и "Ошибка на шаг" — в актуальном
заполненном отчёте её уже нет: "Ошибка на шаг, %" там сама по себе уже
готовое отношение (факт.ошибка / допустимая ошибка × 100) — 100 и меньше
значит "в допуске". Правки той более старой структуры не делаю, ориентир —
актуальный реальный файл, а не пустой шаблон.

Общие с ТО/ТШР блоки ("Данные по объекту"/"Краткая спецификация на
клапан", футер) — из report_common.py (см. память report_types_shared_blocks).
Единственное расхождение внутри самой спецификации — подпись строки
"Привод, ПБ" вместо "Приход, ПВ" у ТО/ТШР: сохранено как есть в реальном
файле, не унифицировано молча (см. spec_table(input_signal_label=...)).
"""

import random  # уже засеян (random.seed(42)) в report_common
from report_common import (
    header_cell, value_cell, plain_text, two_col_row,
    band, APP_COLORS, diagram_field, linspace, footer_fields, spec_table,
)

# Зелёный/красный — те же цвета, что и у verdict_highlight() в остальных
# актах (мягкий зелёный/красный по просьбе пользователя). Тут не через
# JS-тернарник движка (highlighting="[var]>100?...", см. report_common.py::
# error_ratio_highlight() — тот вариант для боевого Report::Pdf:: с
# реальными данными через onSetValue), а статикой: на этапе "просто
# макет" значения и так уже известны при генерации XML, задавать их через
# плейсхолдер+источник данных пока незачем.
COLOR_OK = "rgba(198,239,206,255)"
COLOR_FAIL = "rgba(255,199,206,255)"

def error_bg(text):
    if text is None:
        return "rgba(255,255,255,255)"
    return COLOR_FAIL if float(text) > 100 else COLOR_OK

PAGE_W = 760
LEFT_X, LEFT_W = 0, 372
RIGHT_X, RIGHT_W = 388, 372
LABEL_W = 140


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

# ==================== График теста на чувствительность реагирования ====
# Форма — по референс-скриншоту пользователя (Response_1.bmp): база у ~0%,
# резкий подъём к ~50%, несколько НЕРАВНОМЕРНЫХ ступеней вверх до пика
# (~58%), резкий сброс, несколько ступеней вниз, финальный сброс к 0%.
# Величины самих ступеней — из реальных данных ТЧР (report Илим.xlsx):
# база 50%, дальше +3/+5/+6/+6.5 (вверх), затем от пика вниз -3/-5/-6/-6.5
# (эти дельты в реальном отчёте считаются от той же базы 50%, а не
# накопительно от предыдущей ступени — воспроизведено как накопительная
# лестница, т.к. именно так выглядит референс-график: каждая следующая
# ступень заметно выше предыдущей).
STEP_DURATION = 12.0  # секунд на ступень (у ТЧР ступени короче ТШР на референсе)
BASE_LEVEL = 50.0
UP_DELTAS = [3.0, 5.0, 6.0, 6.5]
DOWN_DELTAS = [3.0, 5.0, 6.0, 6.5]

STEP_LEVELS_UP = [BASE_LEVEL] + [BASE_LEVEL + d for d in UP_DELTAS]
STEP_LEVELS_DOWN = [BASE_LEVEL] + [BASE_LEVEL - d for d in DOWN_DELTAS]
STEP_LEVELS = [0.0] + STEP_LEVELS_UP + list(reversed(STEP_LEVELS_DOWN)) + [0.0]

def step_response(prev_level, target_level, dt, *, tau=0.6, freq=1.2):
    """Быстрый отклик с лёгким перерегулированием — на референс-графике
    видна короткая "притирка" датчика к заданию на каждой ступени, без
    выраженного апериодического звена, как у ТШР (там ступени крупнее и
    длиннее)."""
    import math
    return target_level + (prev_level - target_level) * math.exp(-dt / tau) * math.cos(freq * dt)

setpoint_curve = []
sensor_curve = []
prev_level = 0.0
POINTS_PER_STEP = 50
t = 0.0
for level in STEP_LEVELS:
    t_start = t
    t_end = t + STEP_DURATION
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

# Тот же размер, что у остальных нативных графиков отчёта (760x280) — по
# просьбе пользователя "такого же размера, как и на всех других страницах".
CHART_W, CHART_H = 760, 280
content_fields.append(diagram_field(0, top, CHART_W, CHART_H, "",
    [("Задание", setpoint_curve, APP_COLORS["setpoint"]),
     ("Датчик линейных перемещений", sensor_curve, APP_COLORS["linear"])],
    x_axis_unit="s", y_axis_unit="%", field_name="chart_response"))
top += CHART_H + 16

# ==================== Результаты теста ====================
# Структура 1:1 с report Илим.xlsx: два блока (восходящее/нисходящее
# движение), в каждом — база + 4 ступени, 3 строки данных (Задание/Факт/
# Ошибка на шаг). У ТЧР, в отличие от ТРС, после обоих блоков есть ещё
# одна итоговая строка "Ошибка по положению, %" (E64 в реальном файле).
content_fields.append(plain_text(0, top, PAGE_W, 18,
    "РЕЗУЛЬТАТЫ ТЕСТА НА ЧУВСТВИТЕЛЬНОСТЬ РЕАГИРОВАНИЯ", bold=True))
top += 18 + 16
RES_TOP = top

COL_LABEL, COL_STEP = 190, 114
TABLE_W = COL_LABEL + COL_STEP * 5

UP_HEADERS = ["Исходное значение", "Задание + шаг 1", "Задание 1 + шаг 2",
              "Задание 2 + шаг 3", "Задание 3 + шаг 4"]
DOWN_HEADERS = ["Исходное значение", "Задание - шаг 1", "Задание 1 - шаг 2",
                "Задание 2 - шаг 3", "Задание 3 - шаг 4"]

def direction_table(top, title, headers, target_texts, fact_values, error_values):
    fields = []
    fields.append(plain_text(0, top, TABLE_W, 18, title, bold=True))
    top += 18 + 4

    cx = 0
    fields.append(header_cell(cx, top, COL_LABEL, 22, "Положение хода клапана, %", border_right=False))
    cx += COL_LABEL
    for i, htext in enumerate(headers):
        last = (i == len(headers) - 1)
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
    RES_TOP, "Восходящее движение", UP_HEADERS,
    ["50.00", "50.00 + 3.00", "50.00 + 5.00", "50.00 + 6.00", "50.00 + 6.50"],
    ["49.94", "53.33", "55.47", "56.59", "57.63"],
    [None, "13.00", "7.00", "12.00", "108.00"],
)
content_fields += left_fields

right_fields, right_bottom = direction_table(
    left_bottom + 16, "Нисходящее движение", DOWN_HEADERS,
    ["50.00", "50.00 - 3.00", "50.00 - 5.00", "50.00 - 6.00", "50.00 - 6.50"],
    ["50.73", "48.10", "46.57", "45.47", "45.09"],
    [None, "12.33", "23.50", "10.00", "24.00"],
)
content_fields += right_fields

summary_top = right_bottom + 16
content_fields.append(value_cell(0, summary_top, COL_LABEL, 22, "Ошибка по положению, %",
                                  border_right=False))
content_fields.append(value_cell(COL_LABEL, summary_top, COL_STEP, 22, "0.79", align="hCenter"))

CONTENT_HEIGHT = summary_top + 22

header_band = band("PageHeader", HEADER_FIELDS, name="TchrHeaderBand", height=HEADER_H)
title_band = band("ReportTitle", content_fields, name="TchrContentBand", height=CONTENT_HEIGHT)
footer_band = band("PageFooter", footer_fields(), name="TchrFooterBand", height=20)

print(f"# ТЧР высота: {HEADER_H + CONTENT_HEIGHT} из {1108} на странице")

xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<Reports email="" lib="QtRPT" programmer="MpiSdk">
  <Report border="0" borderColor="rgba(0,0,0,255)" borderStyle="solid" borderWidth="1" marginsBottom="40" marginsLeft="40" marginsRight="40" marginsTop="40" orientation="0" pageHeight="1188" pageNo="1" pageWidth="840" watermark="0">
    {header_band}
    {title_band}
    {footer_band}
  </Report>
</Reports>
'''

with open("tchr.xml", "w", encoding="utf-8") as f:
    f.write(xml)

print(f"Написано tchr.xml, высота шапки: {HEADER_H}, график: {CHART_H}, таблицы результатов: {CONTENT_HEIGHT}")
