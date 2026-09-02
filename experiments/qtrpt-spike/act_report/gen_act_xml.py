#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Генератор experiments/qtrpt-spike/examples_report/act.xml — шаблона
"Акт о проведении диагностики клапана" под движок QtRPT.

Единицы XML: 1 unit = 0.25mm (подтверждено эмпирически: pageWidth/4 -> мм
в qtrpt.cpp::setPageSettings). A4 портрет: pageWidth=840, pageHeight=1188.

Начиная с этой версии — не один сплошной ReportTitle-бэнд с руками
посчитанными Y на всё, а несколько бэндов:
  ReportTitle   — шапка/инфоблок/таблица результатов/дата/заключение
                  (стабильная по размеру часть, печатается 1 раз).
  MasterHeader/MasterData (bandNo=1) — таблица ЗИП. Число строк реально
                  переменное (см. QtRPT::processMasterData — движок сам
                  проверяет, влезает ли следующая строка, и уходит на
                  новую страницу с повтором шапки, если нет).
  MasterHeader/MasterData (bandNo=2) — комментарий к тесту. autoHeight=1
                  у самого поля — если текст не влезает в 1 "запись",
                  та же процедура ухода на новую страницу срабатывает
                  и для него.
  ReportSummary — таблица подписей, печатается 1 раз, ПОСЛЕ того, как
                  движок отработает оба MasterData (сама следит, чтобы
                  не вылезти за нижний край страницы).
  PageFooter    — как раньше, футер+номер страницы на каждой странице.
Это и даёт настоящую адаптивность (переполнение уходит на след.
страницу, а не наезжает на соседний блок или тихо пропадает) —
раньше всё было в одном ReportTitle, а он вообще не проверяет
переполнение (см. QtRPT::processRTitle — просто печатает и всё).
"""


import math
import random  # уже засеян (random.seed(42)) в report_common — та же
                # глобальная state, повторно сеять не нужно
from report_common import (
    FIELD_COUNTER, next_name, field, header_cell, label_cell, value_cell,
    plain_text, two_col_row, band, verdict_highlight, APP_COLORS,
    diagram_field, linspace, footer_fields, spec_table,
)

# ==================== ReportTitle: статическая часть ====================
title_fields = []

# ---- Блок 1: заголовок ----
# "установки" убрана из первого (динамического по длине) блока и
# объединена со значением objectTag во втором — по просьбе должна
# читаться как единое целое и стоять примерно по центру оставшегося
# места, а не быть прижатой к самому краю листа.
title_fields.append(plain_text(0, 0, 520, 24,
    "Акт о проведении диагностики клапана [valvePosition]",
    bold=True, size=11))
title_fields.append(plain_text(520, 0, 240, 24, "установки [objectTag]",
    bold=True, align="hCenter", size=11))

# ---- Блок 2a: информация об объекте (левая колонка, x=0..372) ----
LEFT_X, LEFT_W = 0, 372
RIGHT_X, RIGHT_W = 388, 372
LABEL_W = 140
VALUE_W = LEFT_W - LABEL_W  # 232

top = 40
title_fields.append(header_cell(LEFT_X, top, LEFT_W, 22, "Информация об объекте"))
top += 22
obj_rows = [
    ("Объект", "[objectName]"),
    ("Цех", "[objectShop]"),
    ("Отделение/установка", "[objectUnit]"),
    ("ФИО", "[objectFIO]"),
]
for label, value in obj_rows:
    title_fields += two_col_row(LEFT_X, top, LABEL_W, VALUE_W, 20, label, value)
    top += 20

# ---- Блок 2b: материал деталей (та же левая колонка, ниже) ----
top += 8
title_fields.append(header_cell(LEFT_X, top, LEFT_W, 22, "Материал деталей"))
top += 22
material_rows = [
    ("Корпус", "[matBody]"),
    ("Крышка", "[matCover]"),
    ("Седло", "[matSeat]"),
    ("Шар", "[matBall]"),
    ("Диск", "[matDisc]"),
    ("Плунжер", "[matPlug]"),
    ("Вал", "[matShaft]"),
    ("Шток", "[matStem]"),
    ("Направляющие втулки", "[matBushing]"),
]
for label, value in material_rows:
    title_fields += two_col_row(LEFT_X, top, LABEL_W, VALUE_W, 18, label, value)
    top += 18
left_col_bottom = top

# ---- Блок 2c: краткая спецификация клапана (правая колонка) ----
# spec_table() — общий с ТШР (gen_tshr_xml.py) хелпер из report_common.py,
# см. память report_types_shared_blocks: этот блок подтверждённо общий
# для обоих типов актов, не переопределяем его тут по-своему.
spec_fields, right_col_bottom = spec_table(RIGHT_X, 40, RIGHT_W, LABEL_W)
title_fields += spec_fields

info_block_bottom = max(left_col_bottom, right_col_bottom)

# ---- Блок 3: результат диагностики ----
top = info_block_bottom + 16
title_fields.append(plain_text(0, top, 760, 18, "Результат диагностики", bold=True))
top += 18

COL_LABEL, COL_V1, COL_V2, COL_V3 = 260, 167, 167, 166
X_LABEL = 0
X_V1 = X_LABEL + COL_LABEL
X_V2 = X_V1 + COL_V1
X_V3 = X_V2 + COL_V2

title_fields.append(header_cell(X_LABEL, top, COL_LABEL, 22, "Параметр измерения", border_right=False))
title_fields.append(header_cell(X_V1, top, COL_V1, 22, "Действительное значение", border_right=False))
title_fields.append(header_cell(X_V2, top, COL_V2, 22, "Рекомендованное значение", border_right=False))
title_fields.append(header_cell(X_V3, top, COL_V3, 22, "Примечание"))
top += 22

# Высота строки таблицы результатов. Была 24 везде, ровно под одну строку
# текста — с реальными данными смотрелось "впритык" к границе даже у
# однострочных лейблов, чуть просторнее для дыхания. У строки "Ход штока/
# Ход вала" лейбл буквально в 2 строки (перенос "\n" в самом тексте, не
# автовысота движка) — под неё отдельная, кратно большая высота, а не
# общая: остальные лейблы однострочные, раздувать их так же не нужно.
ROW_H = 26
TWO_LINE_ROW_H = 42

result_rows_3col = [
    ("Динамическая ошибка (среднее значение, %)", "[resDynErrorValue]", "2.0", "[resDynErrorNote]", ROW_H),
    ("Ход штока, мм\nХод вала, град", "[resStrokeValue]", "[resStrokeRef]", "[resStrokeNote]", TWO_LINE_ROW_H),
    ("Заданный диапазон пружин привода, бар", "[resSpringRange]", "[resSpringRangeRef]", "[resSpringNote]", ROW_H),
    ("Нижнее/верхнее значение давления в приводе, бар", "[resPressureRange]", "—", "—", ROW_H),
    ("Коэффициент трения, %", "[resFrictionPct]", "—", "—", ROW_H),
    ("Трение, среднее значение, Н", "[resFrictionAvg]", "—", "—", ROW_H),
]
# Строки идут сразу под шапкой (border_bottom у шапки уже есть) —
# верх ни у одной из строк рисовать не нужно, иначе задвоится с
# нижней границей шапки.
#
# Подсветка вердикта — только у "настоящих" плейсхолдеров ([resXxxNote]),
# не у статичных "—" (тем нечего подсвечивать: соответствие тут не
# оценивается вообще, см. result_rows_3col выше).
for label, v1, v2, v3, row_h in result_rows_3col:
    title_fields.append(label_cell(X_LABEL, top, COL_LABEL, row_h, label, border_right=False, border_top=False))
    title_fields.append(value_cell(X_V1, top, COL_V1, row_h, v1, align="hCenter", border_right=False, border_top=False))
    title_fields.append(value_cell(X_V2, top, COL_V2, row_h, v2, align="hCenter", border_right=False, border_top=False))
    is_verdict = v3.startswith("[") and v3.endswith("]")
    title_fields.append(value_cell(X_V3, top, COL_V3, row_h, v3, align="hCenter", border_top=False,
                                    highlighting=verdict_highlight(v3) if is_verdict else ""))
    top += row_h

result_rows_merged = [
    ("Зона посадки плунжер/седло*", "[resSeatingZone]"),
    ("Адаптация/детали адаптации позиционера*", "[resPositionerAdaptation]"),
    ("Состояние навесного оборудования*", "[resAuxEquipment]"),
    ("График трения хода клапана*", "[resFrictionGraph]"),
    ("Наличие течи по сальнику*", "[resGlandLeak]"),
]
for label, value in result_rows_merged:
    title_fields.append(label_cell(X_LABEL, top, COL_LABEL, ROW_H, label, border_right=False, border_top=False))
    title_fields.append(value_cell(X_V1, top, COL_V1 + COL_V2, ROW_H, value, align="hCenter", border_right=False, border_top=False))
    title_fields.append(value_cell(X_V3, top, COL_V3, ROW_H, "", align="hCenter", border_top=False))
    top += ROW_H

# последняя строка снова 3-колоночная (время срабатывания)
title_fields.append(label_cell(X_LABEL, top, COL_LABEL, ROW_H, "Время срабатывания прямого/обратного хода, сек", border_right=False, border_top=False))
title_fields.append(value_cell(X_V1, top, COL_V1, ROW_H, "[resTimeForward]", align="hCenter", border_right=False, border_top=False))
title_fields.append(value_cell(X_V2, top, COL_V2, ROW_H, "[resTimeBackward]", align="hCenter", border_right=False, border_top=False))
title_fields.append(value_cell(X_V3, top, COL_V3, ROW_H, "", align="hCenter", border_top=False))
top += ROW_H

results_bottom = top

conclusion_bottom = results_bottom  # дата переехала в PageFooter, см. ниже

TITLE_BAND_HEIGHT = conclusion_bottom
CONTENT_HEIGHT = 1188 - 40 - 40  # 1108
print(f"# ReportTitle высота: {TITLE_BAND_HEIGHT} из {CONTENT_HEIGHT} на странице")

# Отступ между блоками ниже (Заключение/ЗИП/Комментарий/подписи) — не
# через margin (не работает вообще, см. историю в pdf-report-spike) и
# не через spacer-таблицу (а её тут и негде вставить — бэнды просто
# идут вплотную), а через top-смещение первого поля в каждом бэнде:
# сам бэнд "выше" на GAP, а всё содержимое просто начинается ниже.
GAP = 8

# ============ MasterHeader/MasterData bandNo=1: заключение (тоже адаптивный) ============
# Раньше "Заключение*" стояло статикой в ReportTitle рядом с ЗИП (в 2
# колонки). У полос данных в этом движке нет колонок — только полоса на
# всю ширину, поэтому Заключение и ЗИП теперь друг под другом. Раз уж
# переезжает — дал ему ту же защиту от переполнения, что и комментарию:
# autoHeight на текстовом поле (см. правку в RptFieldObject.cpp).
#
# Заголовок и текст — СОЗНАТЕЛЬНО в разных бэндах (MasterHeader +
# MasterData), не одной записью. Пробовал объединить (как выглядело бы
# логичнее) — поймал баг движка: если ПЕРЕД автовысотным полем в той же
# записи стоит другое поле с иным начертанием шрифта, два прохода
# измерения дают разную высоту переноса строк, автовысота недосчитывает,
# и текст наезжает на футер (см. подробный комментарий в
# RptFieldObject.cpp). Раздельные бэнды это обходят — минус в том, что
# заголовок теоретически может остаться сиротой в конце страницы, если
# разрыв придётся точно между ним и текстом. Редкий случай и совсем не
# такой опасный, как наезд текста на футер.
conclusion_header_fields = [header_cell(0, GAP, 760, 20, "Заключение*")]
conclusion_data_fields = [value_cell(0, 0, 760, 16, "[conclusionText]", valign="vTop", auto_height=True)]

# ============ MasterHeader/MasterData bandNo=2: список ЗИП (реально переменный) ============
zip_header_fields = [header_cell(0, GAP, 760, 20, "Рекомендуемый ЗИП*")]
zip_data_fields = [value_cell(0, 0, 760, 18, "[sparePartText]")]

# ============ MasterHeader/MasterData bandNo=3: комментарий (реально переменной высоты) ============
# См. пояснение у Заключения выше — та же причина держать заголовок
# отдельным бэндом.
comment_header_fields = [plain_text(0, GAP, 760, 14, "Комментарий к тесту", bold=True)]
comment_data_fields = [plain_text(0, 0, 760, 16, "[testCommentText]", auto_height=True)]

# ==================== ReportSummary: подписи ====================
summary_fields = []
top = GAP
SIG_LABEL_W = 140
SIG_VALUE_W = 760 - SIG_LABEL_W
sig_rows = [
    ("ИСПОЛНИТЕЛЬ:", ""),
    ("ЗАКАЗЧИК:", ""),
    ("", ""),
]
first = True
for label, value in sig_rows:
    summary_fields += two_col_row(0, top, SIG_LABEL_W, SIG_VALUE_W, 20, label, value, first_row=first)
    top += 20
    first = False
SUMMARY_HEIGHT = top


report_title_band = band("ReportTitle", title_fields, name="RepTitleBand", height=TITLE_BAND_HEIGHT)
conclusion_header_band = band("MasterHeader", conclusion_header_fields, band_no=1, name="ConclusionHeaderBand", height=GAP + 20)
conclusion_data_band = band("MasterData", conclusion_data_fields, band_no=1, name="ConclusionDataBand", height=16)
zip_header_band = band("MasterHeader", zip_header_fields, band_no=2, name="ZipHeaderBand", height=GAP + 20)
zip_data_band = band("MasterData", zip_data_fields, band_no=2, name="ZipDataBand", height=18)
comment_header_band = band("MasterHeader", comment_header_fields, band_no=3, name="CommentHeaderBand", height=GAP + 14)
comment_data_band = band("MasterData", comment_data_fields, band_no=3, name="CommentDataBand", height=16)
summary_band = band("ReportSummary", summary_fields, name="SigBand", height=SUMMARY_HEIGHT)

# <Page>/<TotalPages> — служебные токены движка, подставляются на
# печати (см. qtrpt.cpp: str.replace("<Page>", ...)) — передаём как
# обычный текст с настоящими < >, quoteattr() сам заэкранирует их в
# &lt;/&gt; для валидного XML-атрибута; движок потом видит "<Page>".
# Дата раньше стояла отдельной строкой справа под таблицей результатов
# — перенесена в футер (печатается на каждой странице, а не только там,
# где раньше физически кончалась таблица). footer_fields() — общий с
# ТШР хелпер из report_common.py.
footer_band = band("PageFooter", footer_fields(), name="PageFooterBand", height=20)

# ==================== Страница 2: диагностические графики ====================
# Отдельный <Report> — не переполнение первого (там уже показали, чем
# это чревато), а штатный механизм QtRPT для "следующая страница с
# другим содержимым": каждый <Report> в <Reports> — это отдельная
# физическая страница со своими бэндами (см. qtrpt.cpp::
# makeReportObjectStructure — читает все <Report> подряд, processReport
# на каждый сам стартует printer->newPage()).
#
# Графики — нативные QChart движка (см. diagram_field выше), не
# картинки: данные — синтетика для проверки вёрстки, реальные придут
# из телеметрии, когда дойдём до реальных данных.
graphs_fields = []
graphs_fields.append(plain_text(0, 0, 760, 24,
    "Диагностические графики клапана, поз. [valvePosition]", bold=True, size=11))

CHART_W, CHART_H = 760, 280
top = 32

# Количество точек, оси и форма кривых портированы 1:1 из продового виджета
# (Widgets::Chart, experiments/chart-preview/main.cpp — makeTaskPreviewPoints/
# makeFrictionPreviewPoints/makeStrokeVsPressureCurve), не только плотность
# точек, но и какая величина на какой оси и сама форма кривой — плейсхолдер
# должен выглядеть так же, как реальные превью-графики в приложении.

# 1) Ход штока (мм) от управляющего сигнала (мА) — сигнал прогоняется туда
# и обратно (3 -> 21 -> 3 мА, как в остальных проходах теста), без давлений
# и без второй оси — только положение. "Задание" — идеальная безынерционная
# кривая (commandedStroke зависит только от текущего мА, поэтому прямой и
# обратный проход у неё совпадают день в день, видна одной линией). Реальный
# датчик — гистерезисная пара "Прямой ход"/"Обратный ход" (запаздывание в
# разные стороны на подъёме/спуске), та же идея, что и на графике давления
# ниже, только тут ещё есть с чем сравнить — идеальным "Заданием" рядом.
mA_min, mA_max = 3.0, 21.0
ramp_start, ramp_end = 5.0, 19.0
stroke_max_task = 24.7
sensor_lag_mA = 0.25

def commanded_stroke(mA):
    if mA <= ramp_start:
        return 0.0
    if mA >= ramp_end:
        return stroke_max_task
    return stroke_max_task * (mA - ramp_start) / (ramp_end - ramp_start)

mA_forward = linspace(mA_min, mA_max, 450)
mA_reverse = linspace(mA_max, mA_min, 450)

setpoint_curve = (
    [(round(mA, 3), round(commanded_stroke(mA), 2)) for mA in mA_forward]
    + [(round(mA, 3), round(commanded_stroke(mA), 2)) for mA in mA_reverse]
)

sensor_fwd = []
for mA in mA_forward:
    lag = commanded_stroke(max(mA_min, mA - sensor_lag_mA))  # не успевает за растущим сигналом
    noise = (random.random() - 0.5) * 0.3
    sensor_fwd.append((round(mA, 3), round(max(0.0, lag + noise), 2)))

sensor_rev = []
for mA in mA_reverse:
    lag = commanded_stroke(min(mA_max, mA + sensor_lag_mA))  # не успевает за падающим сигналом
    noise = (random.random() - 0.5) * 0.3
    sensor_rev.append((round(mA, 3), round(max(0.0, lag + noise), 2)))

graphs_fields.append(plain_text(0, top, 760, 18,
    "График зависимости ход штока/управляющий сигнал", bold=True))
top += 18 + 6
graphs_fields.append(diagram_field(0, top, CHART_W, CHART_H, "",
    [("Прямой ход", sensor_fwd, APP_COLORS["linear"]),
     ("Обратный ход", sensor_rev, APP_COLORS["linear_ghost"]),
     ("Задание", setpoint_curve, APP_COLORS["setpoint"])],
    x_axis_unit="mA", y_axis_unit="mm", field_name="chart_signal"))
top += CHART_H + 16

# 2) Давление в приводе (бар) -> ход штока (мм), прямой/обратный ход —
# гистерезис (аналог Pressure-графика, makeStrokeVsPressureCurve, 350 точек
# на каждое направление хода).
def stroke_vs_pressure(start_bar, end_bar, *, n=350, stroke_max=25.6, bar_min=0.0, bar_max=2.0):
    curve = []
    for i in range(n):
        t = i / (n - 1)
        bar = bar_min + t * (bar_max - bar_min)
        if bar <= start_bar:
            stroke = 0.0
        elif bar >= end_bar:
            stroke = stroke_max
        else:
            stroke = stroke_max * (bar - start_bar) / (end_bar - start_bar)
        if start_bar < bar < end_bar:
            stroke += (random.random() - 0.5) * 0.4
        curve.append((round(bar, 3), round(max(0.0, stroke), 2)))
    return curve

pressure_fwd = stroke_vs_pressure(0.50, 1.10)
pressure_rev = stroke_vs_pressure(0.42, 1.02)
graphs_fields.append(plain_text(0, top, 760, 18,
    "График зависимости ход штока/давление в приводе", bold=True))
top += 18 + 6
graphs_fields.append(diagram_field(0, top, CHART_W, CHART_H, "",
    [("Прямой ход", pressure_fwd, APP_COLORS["regression_forward"]),
     ("Обратный ход", pressure_rev, APP_COLORS["regression_reverse"])],
    x_axis_unit="bar", y_axis_unit="mm", field_name="chart_pressure"))
top += CHART_H + 16

# 3) Трение (Н) по ходу штока (мм) — аналог Friction-графика, форма
# подогнана под референс (Friction_1.bmp) в самом превью: резкий спад с
# пикового трогания, шумное плато, плавный рост к концу хода. 400 точек.
def base_friction_force(x):
    if x < 2.0:
        t = (x - 1.0) / 1.0
        return 63.0 - t * 16.0
    if x < 18.0:
        return 46.0 + 2.0 * math.sin((x - 2.0) / 3.0)
    t = (x - 18.0) / (24.0 - 18.0)
    return 46.0 + t * 9.0

friction_curve = []
for x in linspace(1.0, 24.0, 400):
    noise = (random.random() - 0.5) * 3.0
    friction_curve.append((round(x, 3), round(max(0.0, base_friction_force(x) + noise), 2)))

graphs_fields.append(plain_text(0, top, 760, 18, "График трения", bold=True))
top += 18 + 6
graphs_fields.append(diagram_field(0, top, CHART_W, CHART_H, "",
    [("Трение", friction_curve, APP_COLORS["linear"])],
    x_axis_unit="mm", y_axis_unit="N", field_name="chart_friction"))
top += CHART_H

graphs_band = band("ReportTitle", graphs_fields, band_no=1, name="GraphsTitleBand", height=top)
graphs_footer_band = band("PageFooter", footer_fields(), name="GraphsPageFooterBand", height=20)

xml = f'''<?xml version="1.0" encoding="UTF-8"?>
<Reports email="" lib="QtRPT" programmer="MpiSdk">
  <Report border="0" borderColor="rgba(0,0,0,255)" borderStyle="solid" borderWidth="1" marginsBottom="40" marginsLeft="40" marginsRight="40" marginsTop="40" orientation="0" pageHeight="1188" pageNo="1" pageWidth="840" watermark="0">
    {report_title_band}
    {conclusion_header_band}
    {conclusion_data_band}
    {zip_header_band}
    {zip_data_band}
    {comment_header_band}
    {comment_data_band}
    {summary_band}
    {footer_band}
  </Report>
  <Report border="0" borderColor="rgba(0,0,0,255)" borderStyle="solid" borderWidth="1" marginsBottom="40" marginsLeft="40" marginsRight="40" marginsTop="40" orientation="0" pageHeight="1188" pageNo="1" pageWidth="840" watermark="0">
    {graphs_band}
    {graphs_footer_band}
  </Report>
</Reports>
'''

with open("act.xml", "w", encoding="utf-8") as f:
    f.write(xml)

print(f"Написано act.xml, полей: {FIELD_COUNTER[0]}")
