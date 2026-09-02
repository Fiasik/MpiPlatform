#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Генератор tshr.xml — шаблон "Акт шагового испытания клапана" (ТШР) под
движок QtRPT. Второй тип акта (первый — gen_act_xml.py, "Акт о проведении
диагностики клапана", ТО), сделан по референс-скриншотам Excel-мокапа
пользователя + реальному графику шаговой реакции из приложения
(experiments/chart-preview аналог — Src/Widgets/Chart/Manager.cpp::
createStepChart: useTimeaxis(true), addAxis("%.2f%%"), ряды "Задание"
(чёрный) + "Датчик линейных перемещений" (linearColor)).

Общие с ТО хелперы и общие блоки ("Краткая спецификация клапана"/"...на
клапан", футер) — из report_common.py, не задублированы (см. память
report_types_shared_blocks: эти блоки подтверждённо переиспользуемые).

РАСХОЖДЕНИЕ с ТО, оставлено сознательно по референс-скриншоту, а не
"исправлено" молча: "Данные по объекту" тут — 3 строки (Объект/Цех/
Установка), у ТО "Информация об объекте" — 4 строки (+ФИО, "Отделение/
установка" вместо "Установка"). Если это просто несовпадение двух
Excel-мокапов, а не сознательное решение — стоит унифицировать вручную.

Одна страница: шапка (PageHeader — тут не принципиально, что "повторяется
на каждой", раз страница одна, но оставлен тот же механизм, что и у ТО, на
случай если контент когда-то не влезет и появится вторая страница) +
график + результаты друг под другом в одном ReportTitle.
"""

import math
import random  # уже засеян (random.seed(42)) в report_common
from report_common import (
    field, header_cell, label_cell, value_cell, plain_text, two_col_row,
    band, APP_COLORS, diagram_field, linspace, footer_fields, spec_table,
)

PAGE_W = 760
LEFT_X, LEFT_W = 0, 372
RIGHT_X, RIGHT_W = 388, 372
LABEL_W = 140


def page_header_fields():
    """Заголовок + "Данные по объекту"/"Краткая спецификация на клапан" —
    повторяется на каждой странице (PageHeader), поэтому это ФУНКЦИЯ, а не
    общий список: каждому вызову (= каждой странице/<Report>) нужны свои
    объекты полей, как и у footer_fields() (см. report_common.py)."""
    fields = []
    fields.append(plain_text(0, 0, 520, 24,
        "Акт шагового испытания клапана, поз. [valvePosition]",
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
                                         header="Краткая спецификация на клапан")
    fields += spec_fields

    bottom = max(top_left, top_right) + 12  # небольшой отступ перед содержимым страницы
    return fields, bottom


HEADER_FIELDS, HEADER_H = page_header_fields()

# ==================== График теста шаговой реакции ====================
# Форма и оси портированы из реального Step-графика приложения
# (Src/Widgets/Chart/Manager.cpp::createStepChart): X — время, Y — "%.2f%%"
# (ход в процентах), ряды "Задание" (чёрный, идеальная ступенька) и
# "Датчик линейных перемещений" (linearColor, реальный отклик с небольшим
# перерегулированием — как на референс-графике теста).
#
# 20 ступеней (10 вверх, 10 вниз) — под 20 строк результатов (2 таблицы
# по 10, см. страницу 2). Время — просто секунды на оси (x_axis_unit="s"):
# у движка (QValueAxis) нет формата "мм:сс", как в самом приложении
# (ChartView.cpp::m_xaxisTime, QDateTimeAxis) — под это нужен отдельный
# патч движка, не делали, т.к. не просили именно это.
STEP_DURATION = 10.0  # секунд на ступень
STEP_LEVELS_UP = linspace(10, 100, 10)
STEP_LEVELS_DOWN = list(reversed(STEP_LEVELS_UP[:-1])) + [0.0]
STEP_LEVELS = STEP_LEVELS_UP + STEP_LEVELS_DOWN  # 20 уровней

def step_response(prev_level, target_level, dt, *, tau=1.3, freq=0.8):
    """Огибающая переходного процесса с небольшим перерегулированием —
    затухающая косинусоида вместо чистой экспоненты (апериодического
    звена без выброса), как на референс-графике (виден небольшой "нос"
    на крупных ступенях)."""
    return target_level + (prev_level - target_level) * math.exp(-dt / tau) * math.cos(freq * dt)

setpoint_curve = []
sensor_curve = []
prev_level = 0.0
POINTS_PER_STEP = 60
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
content_fields.append(plain_text(0, top, PAGE_W, 18, "ГРАФИК ТЕСТА ШАГОВОЙ РЕАКЦИИ", bold=True))
top += 18 + 8

# Тот же размер, что у графиков на странице "Диагностические графики
# клапана" (gen_act_xml.py: CHART_W, CHART_H = 760, 280) — одного размера
# со всеми остальными нативными графиками отчёта, а не на всю страницу.
CHART_W, CHART_H = 760, 280
content_fields.append(diagram_field(0, top, CHART_W, CHART_H, "",
    [("Задание", setpoint_curve, APP_COLORS["setpoint"]),
     ("Датчик линейных перемещений", sensor_curve, APP_COLORS["linear"])],
    x_axis_unit="s", y_axis_unit="%", field_name="chart_step"))
top += CHART_H + 16

# ==================== Результаты теста ====================
# Два стола по 10 строк рядом — под референс-скриншот пользователя.
# "Т86, время"/"Перерегулирование" — иллюстративные (не выведены
# аналитически из кривой выше, это плейсхолдер-таблица, как и сам график
# на плейсхолдер-данных); № 1-10 слева = подъём, 1-10 справа = спуск.
content_fields.append(plain_text(0, top, PAGE_W, 18, "РЕЗУЛЬТАТЫ ТЕСТА ШАГОВОЙ РЕАКЦИИ", bold=True))
top += 18 + 16
RES_TOP = top
COL_N, COL_STEP, COL_T86, COL_OVERSHOOT = 40, 90, 110, 130
TABLE_W = COL_N + COL_STEP + COL_T86 + COL_OVERSHOOT
GAP_BETWEEN = 40
TABLE_L_X = 0
TABLE_R_X = TABLE_W + GAP_BETWEEN

def steps_table(x, top, row_count, index_offset):
    """"№" — статичная разметка (просто номер строки в таблице, не данные).
    "Шаг"/"Т86, время"/"Перерегулирование" — плейсхолдеры [stepNShag]/
    [stepNT86]/[stepNOver], N — сквозной номер шага (1..20, тот же порядок,
    что и Domain::Tests::Option::Step::Result::steps, см. Report::Pdf::
    StepReactionSource). Раньше "Шаг" был статичным "10%.."100%".."0%" —
    ошибочное предположение, что тест всегда идёт равномерным диапазоном
    0..100 и обратно; реальные from/to каждого шага приходят из
    StepResult и могут быть любыми — подставляются в рантайме, как и
    остальные два столбца. Если реальных шагов меньше 20 — лишние строки
    остаются пустыми (плейсхолдер просто не находит значения), а не
    показывают выдуманный процент.
    """
    fields = []
    headers = [("№", COL_N), ("Шаг", COL_STEP), ("Т86, время", COL_T86), ("Перерегулирование", COL_OVERSHOOT)]
    cx = x
    for htext, w in headers:
        last = (w == headers[-1][1])
        fields.append(header_cell(cx, top, w, 22, htext, border_right=None if last else False))
        cx += w
    row_top = top + 22
    for i in range(1, row_count + 1):
        n = index_offset + i
        cx = x
        row = [
            (str(i), COL_N),
            (f"[step{n}Shag]", COL_STEP),
            (f"[step{n}T86]", COL_T86),
            (f"[step{n}Over]", COL_OVERSHOOT),
        ]
        for j, (text, w) in enumerate(row):
            last = (j == len(row) - 1)
            fields.append(value_cell(cx, row_top, w, 22, text, align="hCenter",
                                      border_right=None if last else False, border_top=False))
            cx += w
        row_top += 22
    return fields, row_top

left_fields, left_bottom = steps_table(TABLE_L_X, RES_TOP, len(STEP_LEVELS_UP), 0)
right_fields, right_bottom = steps_table(TABLE_R_X, RES_TOP, len(STEP_LEVELS_DOWN), 10)
content_fields += left_fields
content_fields += right_fields

CONTENT_HEIGHT = max(left_bottom, right_bottom)

header_band = band("PageHeader", HEADER_FIELDS, name="TshrHeaderBand", height=HEADER_H)
title_band = band("ReportTitle", content_fields, name="TshrContentBand", height=CONTENT_HEIGHT)
footer_band = band("PageFooter", footer_fields(), name="TshrFooterBand", height=20)

print(f"# ТШР высота: {HEADER_H + CONTENT_HEIGHT} из {1108} на странице")

xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<Reports email="" lib="QtRPT" programmer="MpiSdk">
  <Report border="0" borderColor="rgba(0,0,0,255)" borderStyle="solid" borderWidth="1" marginsBottom="40" marginsLeft="40" marginsRight="40" marginsTop="40" orientation="0" pageHeight="1188" pageNo="1" pageWidth="840" watermark="0">
    {header_band}
    {title_band}
    {footer_band}
  </Report>
</Reports>
'''

with open("tshr.xml", "w", encoding="utf-8") as f:
    f.write(xml)

print(f"Написано tshr.xml, высота шапки: {HEADER_H}, график: {CHART_H}, таблицы результатов: {CONTENT_HEIGHT}")
