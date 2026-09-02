#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Общие хелперы для генераторов XML-шаблонов QtRPT — вынесены из
gen_act_xml.py, когда появился второй тип акта (gen_tshr_xml.py, "Акт
шагового испытания клапана"), чтобы не дублировать один и тот же код
построения полей/бэндов/графиков в обоих генераторах.

Единицы XML: 1 unit = 0.25mm (подтверждено эмпирически: pageWidth/4 -> мм
в qtrpt.cpp::setPageSettings). A4 портрет: pageWidth=840, pageHeight=1188.
"""

import random
from xml.sax.saxutils import quoteattr

random.seed(42)  # детерминированный плейсхолдерный "шум" на графиках

FIELD_COUNTER = [0]

def next_name():
    FIELD_COUNTER[0] += 1
    return f"f{FIELD_COUNTER[0]}"

def field(left, top, width, height, value, *, bold=False, align="hLeft",
          valign="vCenter", size=8, bordered=True, bg="rgba(255,255,255,255)",
          field_type="label", padding_x=6, padding_y=3, auto_height=False,
          border_top=None, border_bottom=None, border_left=None, border_right=None,
          highlighting=""):
    # value идёт через quoteattr — экранирует " (как в "РДК Системс") и
    # прочие XML-спецсимволы; без этого атрибут обрывается на первой же
    # незаэкранированной кавычке внутри текста.
    value_attr = quoteattr(value)

    # Каждая ячейка рисует свои 4 стороны независимо (RptFieldObject::
    # paintBorder — просто 4 drawLine). У двух соседних ячеек общая
    # граница рисуется ДВАЖДЫ, ровно по одним координатам — от этого она
    # визуально жирнее прочих линий. Чтобы общая граница рисовалась один
    # раз, соседние ячейки договариваются, кто её рисует (см. two_col_row
    # ниже): у левой ячейки border_right=False, у правой — свой правый;
    # у не-первой строки в стопке — border_top=False, полагается на
    # нижнюю границу строки/шапки выше.
    def side(explicit, default):
        return default if explicit is None else explicit

    on = "rgba(0,0,0,255)" if bordered else "rgba(255,255,255,0)"
    off = "rgba(255,255,255,0)"
    bt = on if side(border_top, bordered) else off
    bb = on if side(border_bottom, bordered) else off
    bl = on if side(border_left, bordered) else off
    br = on if side(border_right, bordered) else off

    if not bordered:
        bg = "rgba(255,255,255,0)"

    # paddingX/paddingY — родные атрибуты движка (RptFieldObject.cpp,
    # geometry.paddingX/Y), реально сдвигают текст от левого/верхнего
    # края при отрисовке. Правый/нижний край при этом не двигается, но
    # у нас почти весь текст hLeft — этого достаточно.
    return (
        f'<TContainerField ZValue="11" aligmentH="{align}" aligmentV="{valign}" '
        f'autoHeight="{1 if auto_height else 0}" backgroundColor="{bg}" '
        f'borderBottom="{bb}" borderColor="rgba(0,0,0,255)" '
        f'borderLeft="{bl}" borderRight="{br}" '
        f'borderStyle="solid" borderTop="{bt}" borderWidth="1px" '
        f'fontBold="{1 if bold else 0}" fontColor="rgba(0,0,0,255)" '
        f'fontFamily="Arial" fontItalic="0" fontSize="{size}" fontStrikeout="0" '
        f'fontUnderline="0" format="" groupName="" height="{height}" '
        f'highlighting={quoteattr(highlighting)} left="{left}" name="{next_name()}" '
        f'paddingX="{padding_x}" paddingY="{padding_y}" printing="1" '
        f'rotate="0" textWrap="1" top="{top}" type="{field_type}" '
        f'value={value_attr} width="{width}"/>'
    )

def header_cell(left, top, width, height, text, **kw):
    return field(left, top, width, height, text, bold=True, align="hCenter",
                 bg="rgba(224,224,224,255)", **kw)

def label_cell(left, top, width, height, text, **kw):
    return field(left, top, width, height, text, align="hLeft", **kw)

def value_cell(left, top, width, height, text, align="hLeft", valign="vCenter", **kw):
    return field(left, top, width, height, text, align=align, valign=valign, **kw)

def plain_text(left, top, width, height, text, *, bold=False, align="hLeft", size=8, **kw):
    return field(left, top, width, height, text, bold=bold, align=align,
                 size=size, bordered=False, **kw)

def two_col_row(x, top, label_w, value_w, height, label, value, *,
                 first_row=False, value_align="hLeft"):
    """Строка label|value с общей вертикальной границей, нарисованной
    один раз (правую границу лейбла отключаем), и общей верхней
    границей, нарисованной один раз (кроме первой строки стопки — её
    верх рисует шапка/сама строка)."""
    out = []
    out.append(label_cell(x, top, label_w, height, label,
                           border_right=False, border_top=first_row))
    out.append(value_cell(x + label_w, top, value_w, height, value, align=value_align,
                           border_top=first_row))
    return out

def band(band_type, fields_list, *, band_no=1, name=None, height=None, extra_attrs=""):
    name = name or f"{band_type}Band{band_no}"
    return (
        f'<ReportBand bandNo="{band_no}" height="{height}" name="{name}" '
        f'type="{band_type}" width="760" {extra_attrs}>\n'
        + "\n".join(fields_list) + "\n</ReportBand>"
    )

def verdict_highlight(placeholder):
    """highlighting= для условной подсветки ячейки-вердикта фоном —
    QColor(198,239,206)/QColor(255,199,206) (мягко-зелёный/мягко-красный),
    цвета по просьбе пользователя. Движок: RptFieldObject::highlighting,
    обработка в QtRPT::processHighligthing (qtrpt.cpp).

    ЛОКАЛЬНЫЙ ПАТЧ движка обязателен: оригинальный processHighligthing()
    был сломан для любого реалистичного цвета — exp.insert(1,"'") вставлял
    кавычку на фиксированной 1-й позиции строки цвета, "rgba(198,239,206,255)"
    превращался в "r'gba(198,239,206,255)", JS SyntaxError при evaluate()
    (проверено qDebug()'ом formulaStr). Заодно добавлен "прямой" режим —
    highlighting="<expr>;backgroundColor" (без "=значение") — где <expr>
    сам возвращает нужную строку цвета и может быть вложенным тернарником
    на любое число исходов; тут нужно 3 (соответствует/не соответствует/
    иначе), а не 2, как в исходном "cond;свойство=значение" виде.

    placeholder — плейсхолдер того же поля в квадратных скобках, напр.
    "[resDynErrorNote]".
    """
    var = placeholder.strip("[]")
    return (
        f"[{var}]=='соответствует'?'rgba(198,239,206,255)':"
        f"([{var}]=='не соответствует'?'rgba(255,199,206,255)':'rgba(255,255,255,255)');"
        "backgroundColor"
    )

def error_ratio_highlight(placeholder):
    """Та же механика, что и verdict_highlight(), но сравнение числовое, а
    не строковое — для "Ошибка на шаг, %" в ТЧР/ТРС (report Илим.xlsx):
    там это уже готовое отношение факт.ошибки к допустимой, ×100 — 100
    и меньше значит "в допуске" (зелёный), больше 100 — "превышает"
    (красный). Отдельной строки "Допустимая ошибка на шаг" в реальном
    отчёте больше нет (была в старом пустом шаблоне Report.xlsx, но не в
    актуальном заполненном) — расчёт этого отношения уже сделан до
    попадания в ячейку, тут только цвет по порогу.
    """
    var = placeholder.strip("[]")
    return (
        f"[{var}]>100?'rgba(255,199,206,255)':'rgba(198,239,206,255)';"
        "backgroundColor"
    )

# Цвета сведены из продового Widgets::Chart, а не выдуманы — чтобы графики
# в акте выглядели как те же ряды в приложении, а не рандомной палитрой.
# Источники:
# - Src/Widgets/Chart/Manager.cpp::createTaskChart/createFrictionChart/
#   createPressureChart/createStepChart — "Задание" всегда чёрный
#   (QColor::fromRgb(0,0,0), захардкожен внутри), "Регрессия (прямой/
#   обратный ход)" в Pressure-графике тоже захардкожены внутри
#   (rgb(30,100,200)/rgb(200,50,50)).
# - experiments/chart-preview/main.cpp — linearColor/pressureNColor это
#   ПАРАМЕТРЫ, которые Manager сам не выбирает (их передаёт вызывающий код,
#   т.е. в проде это где-то ещё, не здесь) — взяты значения из этого
#   харнесса как ближайший к проду референс: linearColor=(214,40,40).
# - ChartView.cpp::duplicateChartSeries — "дубль"-слой второго прохода
#   красится как color.lighter(170), не отдельным цветом.
#
# TODO(потом): когда график будет питаться реальными данными из
# приложения, эти цвета должны приходить оттуда же (сериализовать вместе
# с точками), а не дублироваться здесь константой.
APP_COLORS = {
    # Не rgba(0,0,0,255) буквально: движок молча заменяет ряд на цвет из
    # своей темы, если QPen ряда совпадает с QPen() по умолчанию, а он
    # по умолчанию ЧЁРНЫЙ, поэтому "чистый" чёрный неотличим от "цвет не
    # задавали" (проверено: с (0,0,0,255) "Задание" рисовалось оранжевым
    # вместо чёрного). (1,1,1) визуально = чёрный, но не совпадает с QPen().
    "setpoint": "rgba(1,1,1,255)",                  # "Задание"
    "linear": "rgba(214,40,40,255)",                # датчик/трение/перемещение
    "linear_ghost": "rgba(255,156,156,255)",        # дубль второго прохода (lighter(170))
    "regression_forward": "rgba(30,100,200,255)",   # "Регрессия (прямой ход)"
    "regression_reverse": "rgba(200,50,50,255)",    # "Регрессия (обратный ход)"
}

def diagram_field(left, top, width, height, caption, series, *, show_legend=True,
                   x_axis_unit="", y_axis_unit="", field_name=None):
    """type="diagram" поле — нативный QChart движка (RptFieldObject::
    paintDiagram, QtRPT.pri тянет QtCharts). Данные — не через onSetValue,
    а буквально вписаны в XML как дочерние <graph>/<value> (формат сверен
    с оригинальным примером апстрима, QtRptDemo/.../example7a.xml,
    chartType="SeriesTypeLine"). staticChart="1" — движок не пытается
    ничего досчитывать через setValueDiagram, просто рисует то, что уже
    в XML — ровно то, что нужно для графика на плейсхолдер-данных.

    series — список (caption, [(x, y), ...], color) на каждую линию; color —
    строка rgba(...), обычно один из APP_COLORS.

    Два разных шрифтовых режима в paintDiagram() (RptFieldObject.cpp), и
    их легко перепутать при добавлении нового текста на диаграмму:
    - title/axis-подписи рисуются через QChartView::render() — сцена внутри
      компонуется в номинальном (низком) DPI и растягивается в geometry,
      поэтому paintDiagram() x2-удваивает их шрифт перед отрисовкой; здесь в
      XML (titleFont, fontSize у осей) указывай СИЛЬНО МЕНЬШЕ желаемого.
    - легенда (legendFont) — своя, рисуется напрямую через painter->
      drawText() поверх графика (см. ниже), БЕЗ удвоения и БЕЗ прохода через
      QChartView — тут в XML указывай обычный, целевой размер (как для
      caption= обычных текстовых полей).

    caption — заголовок ВНУТРИ QChart (chart->setTitle()) — обычно не
    используем (caption=""), заголовок выносим отдельным plain_text() над
    полем, как остальные заголовки разделов.

    Легенда — не штатная QLegend движка, а наша собственная (маркер
    "линия-точка", как сама линия ряда на графике, сверху по центру): у
    QLegend в Qt6.8 нет API под отступ между цветным маркером и текстом
    (qlegend.h/qlegendmarker.h — только цвет/шрифт/форма), и с пробелами
    в имени ряда та же история — QLegend их обрезает при рендере.

    xAxisUnit/yAxisUnit — единица измерения, приклеивается к каждой подписи
    деления оси ("24.00 mm"), как в продовом Widgets::Chart (см.
    experiments/chart-preview) — там тоже нет отдельного заголовка оси,
    только единица в самих числах. Движок сам это не умел, локальный патч
    RptFieldObject.cpp (QValueAxis::setLabelFormat).

    field_name — стабильное имя поля (атрибут name=), а не автоинкремент
    next_name() — нужно, чтобы Report::Pdf::DiagramInjector (боевой C++)
    мог находить нужное diagram-поле в шаблоне по имени и подставлять туда
    реальные точки при печати, а не полагаться на "fNNN", которое сдвинется
    при любой правке XML до этого поля.
    """
    graphs = []
    for i, (series_caption, points, color) in enumerate(series):
        values = "\n".join(f'<value x="{x}" y="{y}"/>' for x, y in points)
        graphs.append(
            f'<graph caption={quoteattr(series_caption)} color="{color}" graphDS="ds{i+1}">\n'
            f'{values}\n</graph>'
        )
    return (
        f'<TContainerField ZValue="11" aligmentH="hLeft" aligmentV="vCenter" '
        f'autoHeight="0" backgroundColor="rgba(255,255,255,255)" '
        f'borderBottom="rgba(0,0,0,255)" borderColor="rgba(0,0,0,255)" '
        f'borderLeft="rgba(0,0,0,255)" borderRight="rgba(0,0,0,255)" '
        f'borderStyle="solid" borderTop="rgba(0,0,0,255)" borderWidth="1px" '
        f'caption={quoteattr(caption)} chartType="SeriesTypeLine" '
        f'colorBackground="rgba(255,255,255,255)" colorLegend="rgba(64,64,68,255)" '
        f'colorTitle="rgba(0,0,0,255)" fontBold="0" fontColor="rgba(0,0,0,255)" '
        f'fontFamily="Arial" fontItalic="0" fontSize="28" fontStrikeout="0" '
        f'fontUnderline="0" groupName="" height="{height}" left="{left}" '
        f'legendAligment="1" legendFont="Arial,7,-1,5,50,0,0,0,0,0" '
        f'name="{field_name or next_name()}" printing="1" rotate="0" '
        f'showLegend="{1 if show_legend else 0}" staticChart="1" '
        f'titleFont="Arial,20,-1,5,75,0,0,0,0,0" top="{top}" type="diagram" '
        f'value="diagram" width="{width}" '
        f'xAxisUnit={quoteattr(x_axis_unit)} yAxisUnit={quoteattr(y_axis_unit)}>\n'
        + "\n".join(graphs) + "\n</TContainerField>"
    )

def linspace(a, b, n):
    step = (b - a) / (n - 1)
    return [a + step * i for i in range(n)]

def footer_fields():
    # Функция, а не общий список — каждому <Report> нужны СВОИ объекты
    # полей (у каждого свой сгенерированный name), общий PageFooter между
    # несколькими <Report> не работает (getBand по индексу текущей
    # страницы, см. qtrpt.cpp::processPFooter).
    return [
        plain_text(0, 0, 300, 18, 'Разработано ООО "РДК Системс"'),
        plain_text(260, 0, 240, 18, "Поз. [valvePosition], дата [testDate]", align="hCenter"),
        plain_text(600, 0, 160, 18, "Стр. <Page> из <TotalPages>", align="hRight"),
    ]

def spec_table(x, top, width, label_w, *, header="Краткая спецификация клапана",
               input_signal_label="Приход, ПВ"):
    """"Краткая спецификация клапана"/"...на клапан" — блок, общий для ТО
    и ТШР (см. память report_types_shared_blocks: подтверждено, что этот
    блок и "Информация об объекте" переиспользуются между типами актов).
    ТЧР/ТРС (реальный отчёт "report Илим.xlsx") используют тот же блок,
    но с другой подписью этой одной строки — "Привод, ПБ" вместо
    "Приход, ПВ" (реальное расхождение в исходнике, не опечатка, которую
    я себе позволил "исправить" — сохраняю как есть на каждый тип акта).
    Возвращает (fields, bottom_y).
    """
    fields = [header_cell(x, top, width, 22, header)]
    top += 22
    rows = [
        ("Позиция", "[specPosition]"),
        ("Серийный номер", "[specSerial]"),
        ("Модель клапана", "[specModel]"),
        ("Производитель", "[specVendor]"),
        ("DN/PN (ANSI)", "[specDnPn]"),
        ("Позиционер", "[specPositioner]"),
        ("Давление питания, бар", "[specSupplyPressure]"),
        (input_signal_label, "[specInputSignal]"),
        ("Модель привода", "[specActuator]"),
        ("Движение хода клапана", "[specMotion]"),
        ("Материал сальника", "[specPacking]"),
    ]
    for label, value in rows:
        fields += two_col_row(x, top, label_w, width - label_w, 18, label, value)
        top += 18
    return fields, top
