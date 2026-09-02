# Локальные правки QXlsx (Chart API)

Эта копия QXlsx доработана относительно апстрима — без этих правок
`QXlsx::Chart` не может построить нормальный scatter-график с реальной
числовой осью X. Ниже — что именно поменяно, почему, и как накатить то же
самое на другую копию библиотеки (другая версия приложения — свой vendored
QXlsx, эти правки нужно перенести туда отдельно, они не синхронизируются
автоматически).

Готовый патч лежит рядом: `xlsxchart.patch` — можно попробовать применить
напрямую (`git apply xlsxchart.patch` из корня той версии, где лежит своя
копия QXlsx). Если версия QXlsx там другая и патч не встанет чисто — правьте
по этому описанию вручную, здесь показано куда именно и зачем.

Все правки — в трёх файлах: `header/xlsxchart.h`, `header/xlsxchart_p.h`,
`source/xlsxchart.cpp`.

---

## 1. Включена мёртвая запись оси X для scatter/bubble-графиков

**Файл:** `source/xlsxchart.cpp`, метод `ChartPrivate::saveXmlSer`.

**Симптом:** scatter-график строится, серии на месте, но по оси X —
пусто/дефолт, реальные числа из данных не используются.

**Причина:** блок, который пишет `<c:xVal>`, был обёрнут в `#if 0` — то есть
код рабочий, просто выключен вручную (видимо, кем-то из авторов библиотеки
на середине разработки).

**Правка:** убрать `#if 0` / `#endif` вокруг блока:

```cpp
if (!ser->axDataSource_numRef.isEmpty())
{
    if (chartType == Chart::CT_ScatterChart || chartType == Chart::CT_BubbleChart)
    {
        writer.writeStartElement(QStringLiteral("c:xVal"));
    }
    else
    {
        writer.writeStartElement(QStringLiteral("c:cat"));
    }

    writer.writeStartElement(QStringLiteral("c:numRef"));
    writer.writeTextElement(QStringLiteral("c:f"), ser->axDataSource_numRef);
    writer.writeEndElement();//c:numRef
    writer.writeEndElement();//c:cat or c:xVal
}
```

---

## 2. Диапазон оси X больше не захватывает строку заголовка

**Файл:** `source/xlsxchart.cpp`, метод `Chart::addSeries`.

**Симптом:** X и Y расходятся ровно на одну позицию (первая точка X — это
текст заголовка столбца, а не число).

**Причина:** диапазон для `axDataSouruce_numRef` строился из
`range.firstRow()` (сама шапка таблицы), а поправка на `headerH` (пропуск
строки заголовка) применялась только ПОСЛЕ, к `firstDataRow` — то есть было
уже поздно.

**Правка:** переставить блок, который считает `axDataSouruce_numRef`, ПОСЛЕ
корректировки `firstDataRow`/`firstDataColumn` под `headerH`/`headerV`:

```cpp
int firstDataRow = range.firstRow();
int firstDataColumn = range.firstColumn();

if( headerH ) { firstDataRow += 1; }
if( headerV ) { firstDataColumn += 1; }

QString axDataSouruce_numRef;
if (d->chartType == CT_ScatterChart || d->chartType == CT_BubbleChart)
{
    firstDataColumn += 1;
    CellRange subRange(firstDataRow, range.firstColumn(), range.lastRow(), range.firstColumn());
    axDataSouruce_numRef = sheetName + QLatin1String("!") + subRange.toString(true, true);
}
```

(раньше `firstDataRow`/`firstDataColumn` корректировались уже ПОСЛЕ этого
блока — просто поменять местами два фрагмента).

---

## 3. Использование: `swapHeaders=true` для правильного имени серии

Не правка библиотеки, а важный нюанс её вызова — легко наступить снова.

**Симптом:** серии на графике безымянные / имя уходит не туда.

**Причина:** в `saveXmlSer` без `swapHeaders=true` `headerH_numRef`
(желаемое имя серии) попадает в `<c:cat>` вместо `<c:tx>`.

**Как звать правильно:**

```cpp
chart->addSeries(CellRange(1, 1, lastRow, 3), dataSheetPtr,
                  /*headerH=*/true, /*headerV=*/false, /*swapHeaders=*/true);
```

---

## 4. Новое: `Chart::setSeriesLineStyle(bool showMarker, qreal lineWidthPt)`

**Зачем:** Excel по умолчанию рисует маркер на КАЖДОЙ точке scatter-серии.
На реальных данных (сотни точек) маркеры сливаются в сплошное "жирное
пятно" вместо тонкой линии — в библиотеке вообще не было способа это
выключить.

**Заголовок `header/xlsxchart.h`** (публичный метод класса `Chart`):

```cpp
void setSeriesLineStyle(bool showMarker, qreal lineWidthPt = 1.0);
```

**Приватный заголовок `header/xlsxchart_p.h`** (новые поля в `XlsxSeries`):

```cpp
bool    showMarker = true;
qreal   lineWidthPt = -1; // <0 — не задано явно, оставить дефолт Excel
```

**Реализация `source/xlsxchart.cpp`:**

```cpp
void Chart::setSeriesLineStyle(bool showMarker, qreal lineWidthPt)
{
    Q_D(Chart);
    for (auto& ser : d->seriesList)
    {
        ser->showMarker = showMarker;
        ser->lineWidthPt = lineWidthPt;
    }
}
```

И запись в XML внутри `saveXmlSer` — **строго между `c:tx` и `c:cat`/`c:xVal`**,
порядок по схеме `CT_ScatterSer`: `idx, order, tx, spPr, marker, ..., xVal, yVal`:

```cpp
if (ser->lineWidthPt > 0)
{
    const int emu = static_cast<int>(ser->lineWidthPt * 12700); // 12700 EMU = 1pt
    writer.writeStartElement(QStringLiteral("c:spPr"));
    writer.writeStartElement(QStringLiteral("a:ln"));
    writer.writeAttribute(QStringLiteral("w"), QString::number(emu));
    writer.writeEndElement(); // a:ln
    writer.writeEndElement(); // c:spPr
}

if (!ser->showMarker)
{
    writer.writeStartElement(QStringLiteral("c:marker"));
    writer.writeEmptyElement(QStringLiteral("c:symbol"));
    writer.writeAttribute(QStringLiteral("val"), QStringLiteral("none"));
    writer.writeEndElement(); // c:marker
}
```

**Использование:** вызывать ПОСЛЕ `addSeries()` — применяется сразу ко всем
уже добавленным на данный момент сериям:

```cpp
chart->addSeries(...);
chart->setSeriesLineStyle(/*showMarker=*/false, /*lineWidthPt=*/1.0);
```

---

## 5. Новое: `Chart::setAxisRange(Chart::ChartAxisPos pos, double min, double max)`

**Зачем:** библиотека всегда отдаёт масштабирование оси на откуп Excel, а
он берёт "круглый" диапазон с запасом (для данных 3..21 покажет 0..25,
съедая четверть графика пустым местом). Явного способа задать точные
границы не было.

**Заголовок `header/xlsxchart.h`:**

```cpp
void setAxisRange(Chart::ChartAxisPos pos, double min, double max);
```

**Приватный заголовок `header/xlsxchart_p.h`:**

- в `#include` добавить `<QPair>`;
- в `class XlsxAxis` добавить поля:

```cpp
bool hasMin = false;
double axMin = 0;
bool hasMax = false;
double axMax = 0;
```

- в `class ChartPrivate` добавить:

```cpp
QMap< Chart::ChartAxisPos, QPair<double, double> > axisRangeOverrides;
```

**Реализация `source/xlsxchart.cpp`:**

```cpp
void Chart::setAxisRange(Chart::ChartAxisPos pos, double min, double max)
{
    Q_D(Chart);
    d->axisRangeOverrides[pos] = qMakePair(min, max);
}
```

Оси у `Chart` создаются лениво — только в момент сохранения, внутри
`ChartPrivate::saveXmlScatterChart` (в блоке `if (axisList.isEmpty())`).
Поэтому применять переопределения нужно сразу после создания осей, в том
же методе, добавив после блока их создания:

```cpp
// Chart::ChartAxisPos и XlsxAxis::AxisPos совпадают по значениям
// (None=-1, Left=0, Right, Top, Bottom), поэтому пробрасываем как есть.
for (const auto& ax : axisList)
{
    const auto pos = static_cast<Chart::ChartAxisPos>(ax->axisPos);
    if (axisRangeOverrides.contains(pos))
    {
        const auto range = axisRangeOverrides.value(pos);
        ax->hasMin = true;
        ax->axMin = range.first;
        ax->hasMax = true;
        ax->axMax = range.second;
    }
}
```

И сама запись — внутри `ChartPrivate::saveXmlAxisEG_AxShared`, в блок
`<c:scaling>` (порядок по схеме `CT_Scaling`: `logBase?, orientation, max?, min?`
— max и min пишутся именно в этом порядке, после `orientation`):

```cpp
writer.writeStartElement(QStringLiteral("c:scaling"));
    writer.writeEmptyElement(QStringLiteral("c:orientation"));
        writer.writeAttribute(QStringLiteral("val"), QStringLiteral("minMax"));
    if (axis->hasMax)
    {
        writer.writeEmptyElement(QStringLiteral("c:max"));
        writer.writeAttribute(QStringLiteral("val"), QString::number(axis->axMax));
    }
    if (axis->hasMin)
    {
        writer.writeEmptyElement(QStringLiteral("c:min"));
        writer.writeAttribute(QStringLiteral("val"), QString::number(axis->axMin));
    }
writer.writeEndElement(); // c:scaling
```

**Использование:**

```cpp
chart->setAxisRange(Chart::Bottom, 3.0, 21.0); // ось X жёстко 3..21
```

---

## Проверка после переноса

1. Собрать проект целиком (не только новый спайк) — эти файлы общие с
   `Report::Saver`, если он у вас есть.
2. Построить тестовый график, сохранить `.xlsx`, открыть заново через
   `QXlsx::Document` и прочитать любую известную ячейку — если открылось и
   прочиталось, файл не битый.
3. Открыть в настоящем Excel и глазами проверить: подписи серий в легенде,
   ось X с реальными числами (не 0..N по умолчанию), тонкие линии без точек
   на каждой точке данных.

Пример рабочего вызова целиком — `experiments/xlsx-chart/main.cpp` в этом
репозитории.
