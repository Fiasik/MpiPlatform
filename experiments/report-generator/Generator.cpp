#include "Generator.h"

#include "xlsxdocument.h"
#include "xlsxformat.h"
#include "xlsxcellrange.h"

#include <QDebug>
#include <QTime>

// ПРАВИЛО: никаких merge-ов, охватывающих больше одной строки (ни чисто
// вертикальных, ни смешанных вроде B41:D42). Раскладка держится на бегущем
// курсоре строки (переменная row ниже) — блок пропущен (Ski без
// материалов) — всё, что дальше, просто сдвигается. Если бы существующая
// ячейка была "телом" многострочного merge-а не с начала (курсор
// промахнулся на 1), запись туда либо тихо потеряется, либо испортит файл.
// Горизонтальный merge (в одной строке, несколько столбцов) — это просто
// один и тот же ряд, ему нечего терять при сдвиге, поэтому такие
// используются свободно. Там, где реальному Report.xlsx нужно несколько
// строк высоты (картинка, длинная подпись параметра) — тут вместо этого
// одна строка с явно выставленной высотой (setRowHeight), тот же приём,
// что и под графики.
//
// Единственное намеренное исключение — ячейка "Заключение*" в блоке
// Заключение/ЗИП: она сознательно объединена по вертикали на всю высоту
// списка ЗИП (10 строк). Это безопасно ровно потому, что весь этот
// диапазон строк целиком принадлежит блоку и больше никто в него не
// пишет — не тот случай, когда курсор "не заметил" середину чужого
// merge-а и что-то потерял.
using namespace QXlsx;

namespace ReportGen {

namespace {

QString f2(qreal v) { return QString::number(v, 'f', 2); }
QString f3(qreal v) { return QString::number(v, 'f', 3); }

QString resultOk(CrossingStatus::State state)
{
    switch (state) {
    case CrossingStatus::State::Ok:   return QStringLiteral("соответствует");
    case CrossingStatus::State::Fail: return QStringLiteral("не соответствует");
    case CrossingStatus::State::Unknown: return QStringLiteral("не определено");
    }
    return {};
}

QString resultLimit(CrossingStatus::State state)
{
    switch (state) {
    case CrossingStatus::State::Ok:   return QStringLiteral("не превышает");
    case CrossingStatus::State::Fail: return QStringLiteral("превышает");
    case CrossingStatus::State::Unknown: return QStringLiteral("не определено");
    }
    return {};
}

// ---- Форматы ------------------------------------------------------------
// Не заводим общий "стиль на всё приложение" (это спайк одного листа) — но
// и не создаём Format() на каждую ячейку заново: один набор именованных
// форматов, переиспользуемых по коду ниже, как в любой нормальной вёрстке.
struct Formats {
    Format sectionHeader;
    Format tableSectionHeader;
    Format label;
    Format value;
    Format tableHeader;
    Format tableLabel;
    Format tableCell;
    Format summary;
    Format zipItem;
    Format caption;
    Format footer;

    explicit Formats()
    {
        sectionHeader.setFontSize(11);
        sectionHeader.setFontBold(true);
        sectionHeader.setFontColor(QColor("#6B7280"));
        sectionHeader.setVerticalAlignment(Format::AlignVCenter);

        // "Данные по объекту"/"Краткая спецификация..."/"Заключение*"/
        // "Рекомендуемый ЗИП*" — растянуты на всю ширину своей таблицы
        // (см. mergeCells на местах вызова), а не сидят в одной ячейке
        // подписи. Раньше подпись сидела просто в labelColLeft/labelColRight
        // и заставляла сам этот столбец быть широким под самый длинный
        // заголовок — теперь ширина столбцов не зависит от длины заголовков.
        // По центру, без двоеточия на конце — как в справочном PDF.
        tableSectionHeader.setFontSize(11);
        tableSectionHeader.setFontBold(true);
        tableSectionHeader.setFontColor(QColor("#374151"));
        tableSectionHeader.setHorizontalAlignment(Format::AlignHCenter);
        tableSectionHeader.setVerticalAlignment(Format::AlignVCenter);
        tableSectionHeader.setBorderStyle(Format::BorderThin);
        tableSectionHeader.setFillPattern(Format::PatternSolid);
        tableSectionHeader.setPatternBackgroundColor(QColor("#EDF1F4"));

        // AlignVCenter тут не декоративная мелочь: без неё текст в строке
        // стандартной высоты садится к самому низу ячейки и визуально
        // сливается с линией сетки под ней — на скриншоте это читалось как
        // подчёркивание у каждой подписи. BorderThin — по просьбе "каждой
        // таблице нужны бордеры": все label/value пары (объект, спецификация,
        // материалы, исполнитель/заказчик) теперь визуально таблица, а не
        // голый текст на сетке листа.
        label.setFontSize(12);
        label.setFontColor(QColor("#3F3F3F"));
        label.setVerticalAlignment(Format::AlignVCenter);
        label.setBorderStyle(Format::BorderThin);

        value.setFontSize(12);
        value.setVerticalAlignment(Format::AlignVCenter);
        value.setBorderStyle(Format::BorderThin);

        tableHeader.setFontSize(11);
        tableHeader.setFontBold(true);
        tableHeader.setTextWrap(true);
        tableHeader.setHorizontalAlignment(Format::AlignHCenter);
        tableHeader.setVerticalAlignment(Format::AlignVCenter);
        tableHeader.setBorderStyle(Format::BorderThin);
        tableHeader.setFillPattern(Format::PatternSolid);
        tableHeader.setPatternBackgroundColor(QColor("#EDF1F4"));

        // Отдельно от tableCell: это подпись параметра (длинный текст,
        // "Динамическая ошибка (среднее значение, %)"), а не короткое
        // число/статус. С центрированием и без переноса такой текст в
        // объединённой ячейке обрезается СИММЕТРИЧНО с обоих концов —
        // именно это было видно на скриншоте ("амическая ошибка..." вместо
        // "Динамическая ошибка..."). Левое выравнивание + перенос строки —
        // единственный вариант, где длинная подпись остаётся читаемой.
        tableLabel.setFontSize(12);
        tableLabel.setBorderStyle(Format::BorderThin);
        tableLabel.setHorizontalAlignment(Format::AlignLeft);
        tableLabel.setVerticalAlignment(Format::AlignVCenter);
        tableLabel.setTextWrap(true);
        tableLabel.setIndent(1);

        tableCell.setFontSize(12);
        tableCell.setBorderStyle(Format::BorderThin);
        tableCell.setHorizontalAlignment(Format::AlignHCenter);
        tableCell.setVerticalAlignment(Format::AlignVCenter);

        // Заключение — свободный текст в одну явно-высокую строку (не
        // вертикальный merge, см. правило вверху файла), поэтому
        // AlignTop, а не VCenter — длинный абзац должен начинаться от
        // верхнего края ячейки, а не "плавать" по центру блока.
        summary.setFontSize(12);
        summary.setBorderStyle(Format::BorderThin);
        summary.setTextWrap(true);
        summary.setHorizontalAlignment(Format::AlignLeft);
        summary.setVerticalAlignment(Format::AlignTop);
        summary.setIndent(1);

        zipItem.setFontSize(12);
        zipItem.setBorderStyle(Format::BorderThin);
        zipItem.setTextWrap(true);
        zipItem.setHorizontalAlignment(Format::AlignLeft);
        zipItem.setVerticalAlignment(Format::AlignVCenter);
        zipItem.setIndent(1);

        caption.setFontSize(11);
        caption.setFontBold(true);
        caption.setFontColor(QColor("#6B7280"));
        caption.setVerticalAlignment(Format::AlignVCenter);

        // Больше не используется в самом листе (её место занял настоящий
        // footer через setFooterText() — см. ниже), но формат оставлен на
        // случай, если что-то ещё в отчёте всё же захочет приглушённый
        // мелкий текст на самом листе.
        footer.setFontSize(8);
        footer.setFontColor(QColor("#94A0B1"));
        footer.setVerticalAlignment(Format::AlignVCenter);
    }
};

// ---- Место под графики ----------------------------------------------
// insertImage() у QXlsx кладёт картинку как "один-cell anchor" — размер в
// самом Excel считается ТОЛЬКО из пикселей изображения и его DPI-метаданных
// (dotsPerMeterX/Y), а не из ширины/высоты клеток под ней (см. обсуждение
// в чате и Worksheet::insertImage() в QXlsx/source/xlsxworksheet.cpp). Из
// этого — два вывода, оба применены ниже:
//  1. Одинаковый размер на трёх разных графиках получаем принудительным
//     scaled() до одного целевого пикселного размера ПЕРЕД insertImage —
//     ровно тот же приём, что уже в Report::Saver::saveReport(), просто
//     перенесён сюда.
//  2. DPI на входе может незаметно отличаться в зависимости от того, с
//     какого экрана/масштабирования ОС был сделан grab() виджета — если
     // его не выставить явно, "тот же" пиксельный размер на разных машинах
//     может дать разный физический размер картинки в самом Excel. Поэтому
//     выставляем dotsPerMeterX/Y сами (3780 ≈ 96 DPI), а не полагаемся на
//     то, что уже случайно оказалось в захваченном QImage.
constexpr int kChartTargetWidthPx = 720;
constexpr int kChartTargetHeightPx = 340;
constexpr int kChartDotsPerMeter = 3780; // ≈ 96 DPI

QImage normalizeChartImage(const QImage& source)
{
    if (source.isNull())
        return source;

    QImage scaled = source.scaled(
        kChartTargetWidthPx, kChartTargetHeightPx,
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation
        );
    scaled.setDotsPerMeterX(kChartDotsPerMeter);
    scaled.setDotsPerMeterY(kChartDotsPerMeter);
    return scaled;
}

// Высота под картинку — не "сколько строк по умолчанию наберётся примерно
// столько же", а точный расчёт из целевой высоты в пикселях: 1pt = 1/72",
// вставленная картинка считается по 96 DPI (см. kChartDotsPerMeter выше),
// значит 1px = 72/96 = 0.75pt. Кладём всю высоту в одну явно заданную
// строку — дальше отступ до следующего блока считается от неё, а не
// подбирается на глаз.
double chartRowHeightPt()
{
    return kChartTargetHeightPx * 72.0 / 96.0;
}

}

bool generateTechnicalInspection(
    const TechnicalInspectionData& data,
    Domain::Variant variant,
    const QString& sheetName,
    const QString& outPath
    )
{
    Document xlsx;
    if (!xlsx.sheetNames().isEmpty())
        xlsx.renameSheet(xlsx.sheetNames().first(), sheetName);
    else
        xlsx.addSheet(sheetName);

    // Печать: A4, книжная — как в реальном шаблоне. Масштаб — НЕ фиксированный
    // процент (пробовал 90% — таблица результатов (B:P, 15 столбцов) всё
    // равно не влезала по ширине на этом масштабе, и Excel сам вставлял
    // непрошеный вертикальный разрыв прямо посреди таблицы). "Вписать по
    // ширине в 1 страницу" — Excel/LibreOffice сам подбирает масштаб под
    // реальные ширины столбцов, а не по разу подобранному вручную числу,
    // которое отвяжется от сетки при первой же правке. Высоту не
    // ограничиваем (0) — сколько страниц понадобится по вертикали, столько
    // и будет (это уже управляется через addRowPageBreak() ниже).
    xlsx.setPageOrientation(QStringLiteral("portrait"));
    xlsx.setPaperSize(9); // ECMA-376 §18.17.3.31: 9 = A4
    xlsx.setFitToPage(1, 0);
    xlsx.setPageMargins(0.7, 0.7, 0.75, 0.75, 0.3, 0.3);

    // Заголовок и футер — настоящий Excel header/footer (печатается сам на
    // КАЖДОЙ странице — и с результатами, и с графиками — не строка листа).
    // Раньше "Акт о проведении..."/"Позиция" сидели в самой первой строке
    // как обычная ячейка — оттого им и было нужно обрамление как у ячейки,
    // хотя по смыслу это заголовок страницы, а не строка таблицы. &L/&C/&R
    // переключают левую/центральную/правую секцию, &"..." — шрифт, число —
    // кегль, &P/&N — номер страницы/страниц всего (простановка не руками).
    xlsx.setHeaderText(
        QStringLiteral("&L&\"Calibri,Bold\"&11Акт о проведении диагностики клапана"
                       "&R&\"Calibri,Bold\"&11Позиция %1").arg(data.valve.positionNumber)
        );
    // «Ёлочки», не прямые кавычки: прямая "..." внутри текста секции
    // коллизирует с синтаксисом &"Шрифт,Стиль" самого мини-языка футера —
    // openpyxl (и, вероятно, не только он) в таком случае не может
    // корректно разобрать секцию до конца. «...» тут ещё и просто
    // орфографически правильный вариант для русского текста.
    // Дата — только в правой секции (раньше дублировалась ещё и в
    // центральной, вместе с позицией — позиция и так уже есть в шапке
    // страницы выше, поэтому в центре футера от неё остался только номер
    // страницы).
    xlsx.setFooterText(
        QStringLiteral("&L&\"Calibri,Regular\"&9Разработано ООО «РДК Системс»"
                       "&C&\"Calibri,Regular\"&9Стр. &P из &N"
                       "&R&\"Calibri,Regular\"&9Дата %1")
            .arg(data.params.date)
        );

    const Formats fmt;

    // Колонки — не придуманы, а сверены (только на чтение) с реальным
    // Reports/Report.xlsx, лист "ТО": B:C — подпись слева, D:G — значение
    // слева; J:L — подпись справа, M:P — значение справа. Реальный шаблон
    // держит визуальную ширину почти целиком на объединении нескольких
    // дефолтных столбцов, а не на явной ширине одного — там же нашлись
    // фактические подписи ("Материал деталей", "Вал"/"Шток" и т.д.),
    // ниже воспроизведены дословно, а не придуманы заново.
    const int labelColLeft = 2;    // B
    const int valueColLeft = 4;    // D..G (4 колонки)
    const int labelColRight = 10;  // J..L (3 колонки)
    const int valueColRight = 13;  // M..P (4 колонки)

    // Таблица результатов — свои, более широкие объединения (реальные
    // B27:D28 / E27:G28 / H27:J28 / K27:P28): подпись параметра длиннее
    // обычной, а "Примечание" должно уместить целую фразу вроде "не
    // превышает".
    const int tableLabelCol = 2;   // B..D (3 колонки)
    const int tableFactCol = 5;    // E..G (3 колонки)
    const int tableNormCol = 8;    // H..J (3 колонки)
    const int tableResultCol = 11; // K..P (6 колонок)

    // Раньше 22/20 — под то, что "Данные по объекту"/"Краткая спецификация
    // на клапан:" сидели прямо в этом столбце и требовали от него влезать
    // целиком. Теперь эти подписи — растянутые на всю ширину таблицы шапки
    // (см. ниже), так что сам столбец можно сделать уже — держит только
    // подписи строк ("Отделение/установка", "Направляющая втулка" и т.п.),
    // а не самый длинный заголовок раздела. Левый и правый столбец теперь
    // одной ширины — это и была просьба "+- одинаковые".
    xlsx.setColumnWidth(labelColLeft, labelColLeft, 14);
    xlsx.setColumnWidth(labelColRight, labelColRight, 14);

    int row = 1;

    // --- Данные по объекту / спецификация клапана -----------------------
    // Растянуты как "шапка таблицы" на всю ширину своего блока (а не сидят
    // в одной ячейке labelColLeft/labelColRight) — иначе именно эта строка
    // заставляла бы labelColLeft/labelColRight быть шире, чем нужно для
    // самих подписей полей.
    const int sectionHeaderRow = row;
    xlsx.mergeCells(CellRange(sectionHeaderRow, labelColLeft, sectionHeaderRow, valueColLeft + 3), fmt.tableSectionHeader);
    xlsx.write(sectionHeaderRow, labelColLeft, QStringLiteral("Данные по объекту"), fmt.tableSectionHeader);
    xlsx.mergeCells(CellRange(sectionHeaderRow, labelColRight, sectionHeaderRow, valueColRight + 3), fmt.tableSectionHeader);
    xlsx.write(sectionHeaderRow, labelColRight, QStringLiteral("Краткая спецификация на клапан"), fmt.tableSectionHeader);
    row += 1;

    const int leftStart = row;
    {
        struct Row { QString label; QString value; };
        const QVector<Row> rows = {
            {QStringLiteral("Объект"), data.object.object},
            {QStringLiteral("Цех"), data.object.manufactory},
            {QStringLiteral("Отделение/установка"), data.object.department},
        };
        for (int i = 0; i < rows.size(); ++i) {
            xlsx.mergeCells(CellRange(leftStart + i, labelColLeft, leftStart + i, labelColLeft + 1), fmt.label);
            xlsx.write(leftStart + i, labelColLeft, rows[i].label, fmt.label);
            xlsx.mergeCells(CellRange(leftStart + i, valueColLeft, leftStart + i, valueColLeft + 3), fmt.value);
            xlsx.write(leftStart + i, valueColLeft, rows[i].value, fmt.value);
        }
    }
    const int leftEnd = leftStart + 3;

    const int rightStart = row;
    int rightRow = rightStart;
    auto writeRight = [&](const QString& label, const QVariant& value) {
        xlsx.mergeCells(CellRange(rightRow, labelColRight, rightRow, labelColRight + 2), fmt.label);
        xlsx.write(rightRow, labelColRight, label, fmt.label);
        xlsx.mergeCells(CellRange(rightRow, valueColRight, rightRow, valueColRight + 3), fmt.value);
        xlsx.write(rightRow, valueColRight, value, fmt.value);
        ++rightRow;
    };
    writeRight(QStringLiteral("Позиция"), data.valve.positionNumber);
    writeRight(QStringLiteral("Серийный номер"), data.valve.serialNumber);
    writeRight(QStringLiteral("Модель клапана"), data.valve.valveModel);
    writeRight(QStringLiteral("Производитель"), data.valve.manufacturer);
    writeRight(QStringLiteral("DN/PN (ANSI)"), QStringLiteral("%1 / %2").arg(data.valve.DN, data.valve.PN));
    if (data.positionerModel)
        writeRight(QStringLiteral("Позиционер"), data.valve.positionerModel);
    if (data.includeSolenoid) {
        writeRight(QStringLiteral("Соленоидный клапан"), data.valve.solenoidValveModel);
        writeRight(QStringLiteral("Концевой выключатель/датчик положения"),
                   QStringLiteral("%1 / %2").arg(data.valve.limitSwitchModel, data.valve.positionSensorModel));
    }
    writeRight(QStringLiteral("Давление питания, бар"), f2(data.telemetry.supplyRecord.pressure_bar));
    writeRight(QStringLiteral("Привод, ПБ"), data.params.safePosition);
    writeRight(QStringLiteral("Модель привода"), data.valve.driveModel);
    writeRight(QStringLiteral("Движение хода клапана"), data.params.strokeMovement);
    writeRight(QStringLiteral("Материал сальника"),
               ValveEnums::StuffingBoxSealToString(data.valve.materialStuffingBoxSeal));

    // --- Материалы деталей — только не-Ski ------------------------------
    // У Ski физически нет вкладки "Материалы" в ValveWindow (см. её
    // отсутствие в интерфейсе для этого варианта) — значит и заполнять
    // здесь нечего: блок просто не пишется, а не пишется "пусто" —
    // соответствует принципу "не выдумываем данные, которых нет".
    //
    // variant — параметр функции, не Domain::kSkiVariant: этот генератор
    // рассчитан на то, чтобы из ОДНОЙ сборки эксперимента демонстрировать
    // все три варианта подряд (см. main.cpp) — компилировать три разных
    // MPIPLATFORM_VARIANT ради спайка бессмысленно, поэтому ветвление тут
    // рантаймовое, в отличие от prod-кода в Report::Excel.
    //
    // Позиция — сразу под "Данные по объекту" (независимо от того, где
    // сейчас правый столбец: у C_SACVT с соленоидом он заметно длиннее
    // левого) с пропуском ровно в одну строку, а не "начинать ниже обоих
    // столбцов", как было раньше — так лист не растягивается по высоте
    // без необходимости только потому что справа больше полей.
    int afterParallelColumnsRow;
    if (variant != Domain::Variant::Ski) {
            const int materialsHeaderRow = leftEnd + 1; // leftEnd сам — пропущенная строка
            // "Материал деталей" (не "Материалы") — дословно как в реальном
            // шаблоне (B10:G10). Порядок и подписи полей (в т.ч. "Вал"/
            // "Шток" — их легко перепутать местами, что и произошло в первой
            // версии этого спайка) сверены по B11..B19.
            xlsx.mergeCells(CellRange(materialsHeaderRow, labelColLeft, materialsHeaderRow, valueColLeft + 3), fmt.tableSectionHeader);
            xlsx.write(materialsHeaderRow, labelColLeft, QStringLiteral("Материал деталей"), fmt.tableSectionHeader);

            const int materialsStart = materialsHeaderRow + 1;
            const auto& m = data.materials;

            auto writeLeftPair = [&](int r, const QString& label, const QString& value) {
                xlsx.mergeCells(CellRange(r, labelColLeft, r, labelColLeft + 1), fmt.label);
                xlsx.write(r, labelColLeft, label, fmt.label);
                xlsx.mergeCells(CellRange(r, valueColLeft, r, valueColLeft + 3), fmt.value);
                xlsx.write(r, valueColLeft, value, fmt.value);
            };

            writeLeftPair(materialsStart + 0, QStringLiteral("Корпус"), m.corpus);
            writeLeftPair(materialsStart + 1, QStringLiteral("Крышка"), m.cap);

            // Седло делит строку с CV/KV — тут B и C НЕ объединяются (два
            // самостоятельных подписи в одной строке), в отличие от всех
            // остальных строк этого блока.
            xlsx.write(materialsStart + 2, labelColLeft, QStringLiteral("Седло"), fmt.label);
            xlsx.mergeCells(CellRange(materialsStart + 2, valueColLeft, materialsStart + 2, valueColLeft + 1), fmt.value);
            xlsx.write(materialsStart + 2, valueColLeft, m.saddle, fmt.value);
            xlsx.write(materialsStart + 2, labelColLeft + 1, QStringLiteral("CV/KV"), fmt.label);
            xlsx.mergeCells(CellRange(materialsStart + 2, valueColLeft + 2, materialsStart + 2, valueColLeft + 3), fmt.value);
            xlsx.write(materialsStart + 2, valueColLeft + 2, m.CV, fmt.value);

            writeLeftPair(materialsStart + 3, QStringLiteral("Шар"), m.ball);
            writeLeftPair(materialsStart + 4, QStringLiteral("Диск"), m.disk);
            writeLeftPair(materialsStart + 5, QStringLiteral("Плунжер"), m.plunger);
            writeLeftPair(materialsStart + 6, QStringLiteral("Вал"), m.shaft);
            writeLeftPair(materialsStart + 7, QStringLiteral("Шток"), m.stock);
            writeLeftPair(materialsStart + 8, QStringLiteral("Направляющая втулка"), m.guideSleeve);

            const int materialsEnd = materialsStart + 9;
            afterParallelColumnsRow = std::max(materialsEnd, rightRow) + 1;
    } else {
        afterParallelColumnsRow = std::max(leftEnd, rightRow) + 1;
    }
    row = afterParallelColumnsRow;

    // --- Результат диагностики ------------------------------------------
    // Реальный шаблон растягивает этот заголовок на A23:Q25 — целых 3
    // строки. По правилу "без вертикальных объединений" (см. заголовок
    // файла) берём ОДНУ строку с явно увеличенной высотой вместо этого.
    xlsx.mergeCells(CellRange(row, labelColLeft, row, tableResultCol + 5), fmt.sectionHeader);
    xlsx.write(row, labelColLeft, QStringLiteral("РЕЗУЛЬТАТ ДИАГНОСТИКИ"), fmt.sectionHeader);
    xlsx.setRowHeight(row, row, 20);
    row += 1;

    const int tableHeaderRow = row;
    xlsx.mergeCells(CellRange(tableHeaderRow, tableLabelCol, tableHeaderRow, tableLabelCol + 2), fmt.tableHeader);
    xlsx.write(tableHeaderRow, tableLabelCol, QStringLiteral("Параметр измерения"), fmt.tableHeader);
    xlsx.mergeCells(CellRange(tableHeaderRow, tableFactCol, tableHeaderRow, tableFactCol + 2), fmt.tableHeader);
    xlsx.write(tableHeaderRow, tableFactCol, QStringLiteral("Действительное значение"), fmt.tableHeader);
    xlsx.mergeCells(CellRange(tableHeaderRow, tableNormCol, tableHeaderRow, tableNormCol + 2), fmt.tableHeader);
    xlsx.write(tableHeaderRow, tableNormCol, QStringLiteral("Рекомендованное значение"), fmt.tableHeader);
    xlsx.mergeCells(CellRange(tableHeaderRow, tableResultCol, tableHeaderRow, tableResultCol + 5), fmt.tableHeader);
    xlsx.write(tableHeaderRow, tableResultCol, QStringLiteral("Примечание"), fmt.tableHeader);
    // Реальные H27:J28/K27:P28 и т.д. — тоже 2 физические строки на
    // "визуальную" одну; тут вместо этого одна строка нужной высоты.
    xlsx.setRowHeight(tableHeaderRow, tableHeaderRow, 28);
    row += 1;

    const int tableStart = row;
    const auto& t = data.telemetry;
    const auto& v = data.valve;
    const auto& main = t.testMain;

    if (!main)
        qWarning() << "[ReportGen] ctx.telemetry.testMain отсутствует — часть таблицы результатов останется пустой";

    // Реальные строки этой таблицы — тоже пары физических строк (29-30,
    // 31-32, ...); тут каждая — одна строка высотой 30pt (вместо родных
    // ~2×19.9pt), этого достаточно для двухстрочной подписи параметра.
    // mergeFactNorm — для параметров, у которых физически нет отдельного
    // "рекомендованного значения" (столбец Норма). Сверено с реальным
    // Report.xlsx: там для таких строк (35/37/39 — давление в приводе,
    // коэффициент трения, среднее трение) столбец Факт (E:G) и столбец
    // Норма (H:J) не остаются раздельными — они слиты в одну широкую
    // ячейку E:J. Пустая отдельная ячейка "Норма" тут не нужна и просто
    // теряется — это сознательный вид, а не пропущенное значение.
    auto tableRow = [&](int rowOffset, const QString& label,
                         const QVariant& fact, const QVariant& norm, const QVariant& result,
                         bool mergeFactNorm = false) {
        const int r = tableStart + rowOffset;
        xlsx.mergeCells(CellRange(r, tableLabelCol, r, tableLabelCol + 2), fmt.tableLabel);
        xlsx.write(r, tableLabelCol, label, fmt.tableLabel);
        if (mergeFactNorm) {
            xlsx.mergeCells(CellRange(r, tableFactCol, r, tableNormCol + 2), fmt.tableCell);
            xlsx.write(r, tableFactCol, fact, fmt.tableCell);
        } else {
            xlsx.mergeCells(CellRange(r, tableFactCol, r, tableFactCol + 2), fmt.tableCell);
            xlsx.write(r, tableFactCol, fact, fmt.tableCell);
            xlsx.mergeCells(CellRange(r, tableNormCol, r, tableNormCol + 2), fmt.tableCell);
            xlsx.write(r, tableNormCol, norm, fmt.tableCell);
        }
        xlsx.mergeCells(CellRange(r, tableResultCol, r, tableResultCol + 5), fmt.tableCell);
        xlsx.write(r, tableResultCol, result, fmt.tableCell);
        xlsx.setRowHeight(r, r, 30);
    };

    if (v.dinamicErrorRecomend == QStringLiteral("Без позиционера")) {
        tableRow(0, QStringLiteral("Динамическая ошибка\n(среднее значение, %)"),
                  QVariant(), v.dinamicErrorRecomend, QVariant());
    } else if (main) {
        tableRow(0, QStringLiteral("Динамическая ошибка\n(среднее значение, %)"),
                  f2(main->dynamicErrorReal), v.dinamicErrorRecomend, resultOk(t.crossingStatus.dynamicError));
    } else {
        tableRow(0, QStringLiteral("Динамическая ошибка\n(среднее значение, %)"), {}, {}, {});
    }

    tableRow(1, QStringLiteral("Ход штока, мм\nХод вала, град"),
              f2(t.valveStrokeRecord.real), v.valveStroke, resultOk(t.crossingStatus.valveStroke));

    const bool doubleActing = (v.driveType == DriveType::DoubleActing);
    if (doubleActing) {
        tableRow(2, QStringLiteral("Заданный диапазон пружин\nпривода, бар"), {}, {}, QStringLiteral("Привод ДД"));
    } else if (main) {
        tableRow(2, QStringLiteral("Заданный диапазон пружин\nпривода, бар"),
                  QStringLiteral("%1–%2").arg(f2(main->springLow), f2(main->springHigh)),
                  QStringLiteral("%1–%2").arg(f2(v.driveRangeLow), f2(v.driveRangeHigh)),
                  resultOk(t.crossingStatus.spring));
    } else {
        tableRow(2, QStringLiteral("Заданный диапазон пружин\nпривода, бар"), {}, {}, {});
    }

    if (main) {
        tableRow(3, QStringLiteral("Нижнее/верхнее значение\nдавления в приводе, бар"),
                  QStringLiteral("%1–%2").arg(f2(main->lowLimitPressure), f2(main->highLimitPressure)), {}, {}, true);
        tableRow(4, QStringLiteral("Коэффициент трения, %"),
                  f2(main->frictionPercent), {}, resultLimit(t.crossingStatus.frictionPercent), true);
        tableRow(5, QStringLiteral("Трение, среднее значение, Н"), f3(main->frictionForce), {}, {}, true);
    } else {
        tableRow(3, QStringLiteral("Нижнее/верхнее значение\nдавления в приводе, бар"), {}, {}, {}, true);
        tableRow(4, QStringLiteral("Коэффициент трения, %"), {}, {}, {}, true);
        tableRow(5, QStringLiteral("Трение, среднее значение, Н"), {}, {}, {}, true);
    }

    if (t.testStroke) {
        // Реальный B51:D52/E51:G52/H51:J52 — одна подпись на всю строку
        // (не парная, как я сначала сделал), прямой ход в fact-колонке,
        // обратный — в norm-колонке; note-колонка тут не используется.
        const int r = tableStart + 6;
        xlsx.mergeCells(CellRange(r, tableLabelCol, r, tableLabelCol + 2), fmt.tableLabel);
        xlsx.write(r, tableLabelCol, QStringLiteral("Время срабатывания\nпрямого/обратного хода, сек"), fmt.tableLabel);
        xlsx.mergeCells(CellRange(r, tableFactCol, r, tableFactCol + 2), fmt.tableCell);
        xlsx.write(r, tableFactCol, QTime(0, 0).addMSecs(int(t.testStroke->forwardTimeMs)).toString("mm:ss.zzz"), fmt.tableCell);
        xlsx.mergeCells(CellRange(r, tableNormCol, r, tableNormCol + 2), fmt.tableCell);
        xlsx.write(r, tableNormCol, QTime(0, 0).addMSecs(int(t.testStroke->backwardTimeMs)).toString("mm:ss.zzz"), fmt.tableCell);
        xlsx.setRowHeight(r, r, 30);
    } else {
        qWarning() << "[ReportGen] ctx.telemetry.testStroke отсутствует";
    }

    // "*"-строки — качественные пункты заключения (Зона посадки/Адаптация
    // позиционера/...), продолжение той же таблицы результатов, а не
    // отдельный блок: в реальном шаблоне это буквально следующие строки
    // (41,43,45,47,49) той же самой таблицы, только в fact-колонке текст
    // ответа, а не число. Порядок — как объявлен в Report::Html::
    // ConclusionField (см. комментарий в самом enum).
    using Report::Html::ConclusionField;
    static const QVector<ConclusionField> kConclusionFields = {
        ConclusionField::SeatingZone,
        ConclusionField::PositionerAdaptation,
        ConclusionField::AuxiliaryEquipment,
        ConclusionField::FrictionGraph,
        ConclusionField::GlandLeak,
    };
    for (int i = 0; i < kConclusionFields.size(); ++i) {
        const ConclusionField field = kConclusionFields.at(i);
        tableRow(7 + i, Report::Html::titleFor(field) + QStringLiteral("*"),
                  data.conclusionAnswers.value(field), {}, {});
    }

    row = tableStart + 7 + kConclusionFields.size() + 1;

    // --- Заключение / Рекомендуемый ЗИП ----------------------------------
    // Те же границы столбцов, что и у таблицы результатов выше (левая часть
    // B:J под заключение, правая K:P под ЗИП) — визуально это продолжение
    // той же таблицы, а не новый независимый блок со своей сеткой.
    const int concZipHeaderRow = row;
    xlsx.mergeCells(CellRange(concZipHeaderRow, tableLabelCol, concZipHeaderRow, tableNormCol + 2), fmt.tableSectionHeader);
    xlsx.write(concZipHeaderRow, tableLabelCol, QStringLiteral("Заключение*"), fmt.tableSectionHeader);
    xlsx.mergeCells(CellRange(concZipHeaderRow, tableResultCol, concZipHeaderRow, tableResultCol + 5), fmt.tableSectionHeader);
    xlsx.write(concZipHeaderRow, tableResultCol, QStringLiteral("Рекомендуемый ЗИП*"), fmt.tableSectionHeader);
    row += 1;

    const int concZipStart = row;
    // ЗИП — ровно 10 строк под заполнение (не по числу реально пришедших
    // позиций, как было раньше) — по одной позиции на строку, лишние
    // строки остаются пустыми, но с той же рамкой/форматом, чтобы можно
    // было дописать вручную после экспорта.
    constexpr int kZipRowCount = 10;
    const int concZipEnd = concZipStart + kZipRowCount - 1;
    if (data.spareParts.size() > kZipRowCount) {
        qWarning() << "[ReportGen] позиций ЗИП больше" << kZipRowCount
                   << "— лишние" << (data.spareParts.size() - kZipRowCount) << "не попадут в отчёт";
    }

    // Заключение — растянуто по вертикали на всю высоту списка ЗИП (см.
    // исключение из правила "без вертикальных merge-ов" в начале файла),
    // а не сидит в одной строке произвольной высоты — так текст занимает
    // ровно ту же высоту блока, что и колонка ЗИП рядом с ним.
    xlsx.mergeCells(CellRange(concZipStart, tableLabelCol, concZipEnd, tableNormCol + 2), fmt.summary);
    xlsx.write(concZipStart, tableLabelCol, data.conclusionSummary, fmt.summary);

    for (int i = 0; i < kZipRowCount; ++i) {
        const int r = concZipStart + i;
        xlsx.mergeCells(CellRange(r, tableResultCol, r, tableResultCol + 5), fmt.zipItem);
        xlsx.write(r, tableResultCol, i < data.spareParts.size() ? data.spareParts.at(i) : QString(), fmt.zipItem);
    }

    row = concZipEnd + 2;

    // --- Исполнитель / Заказчик / Дата -----------------------------------
    // "ЗАКАЗЧИК:" — значение оставлено пустым: в Report::Context нет поля
    // с именем заказчика, а выдумывать данные, которых нет, не будем (см.
    // правило в начале файла и feedback_no_fabricated_data).
    xlsx.mergeCells(CellRange(row, labelColLeft, row, labelColLeft + 1), fmt.label);
    xlsx.write(row, labelColLeft, QStringLiteral("ИСПОЛНИТЕЛЬ:"), fmt.label);
    xlsx.mergeCells(CellRange(row, valueColLeft, row, valueColLeft + 3), fmt.value);
    xlsx.write(row, valueColLeft, data.object.FIO, fmt.value);
    row += 1;

    xlsx.mergeCells(CellRange(row, labelColLeft, row, labelColLeft + 1), fmt.label);
    xlsx.write(row, labelColLeft, QStringLiteral("ЗАКАЗЧИК:"), fmt.label);
    xlsx.mergeCells(CellRange(row, valueColLeft, row, valueColLeft + 3), fmt.value);
    row += 1;

    xlsx.mergeCells(CellRange(row, labelColLeft, row, labelColLeft + 1), fmt.label);
    xlsx.write(row, labelColLeft, QStringLiteral("Дата"), fmt.label);
    xlsx.mergeCells(CellRange(row, valueColLeft, row, valueColLeft + 3), fmt.value);
    xlsx.write(row, valueColLeft, data.params.date, fmt.value);
    row += 2;

    // "Разработано ООО ..." теперь в настоящем футере (setFooterText() у
    // самого верха функции) — он и так печатается на КАЖДОЙ странице, тут
    // как отдельная строка листа больше не нужен.

    // Разрыв страницы: результаты (+заключение/ЗИП/подписи) — на одной
    // печатной странице, три графика с подписями — на следующей. Реальный
    // шаблон делает то же самое (rowBreaks после строки 78, прямо перед
    // "Диагностические графики клапана") — только там это жёстко зашито под
    // конкретный номер строки конкретного файла, а тут — от вычисленного
    // row, так что разрыв не откочует, даже если что-то выше поменяет число
    // строк.
    xlsx.addRowPageBreak(row);
    row += 1;

    // --- Графики ----------------------------------------------------
    struct Chart { QString caption; QImage image; };
    const QVector<Chart> charts = {
        {QStringLiteral("График задания"), data.taskChart},
        {QStringLiteral("График давления от перемещения"), data.pressureChart},
        {QStringLiteral("График силы трения от перемещения"), data.frictionChart},
    };

    const int imageCol = labelColLeft;
    for (const auto& chart : charts) {
        if (chart.image.isNull())
            continue;

        xlsx.write(row, imageCol, chart.caption, fmt.caption);
        row += 1;

        // Worksheet::insertImage(row, col, ...) кладёт row/col НАПРЯМУЮ в
        // XlsxMarker без перевода в 0-based, а OOXML требует у <xdr:row>/
        // <xdr:col> именно 0-based — то есть картинка реально попадает на
        // ОДНУ строку и ОДИН столбец правее/ниже переданных чисел. Это не
        // домысел: в проде Pages::TechnicalInspection зовёт insertImage с
        // taskImageRow=83/taskImageColumn=1, и 83+1=84, 1+1=B точно
        // совпадает с реальным merge-блоком картинки в Report.xlsx
        // (B84:O106) — то есть боевой код уже неявно на этот сдвиг
        // рассчитан своими числами. Патчить сам QXlsx поэтому нельзя (тихо
        // сдвинет все картинки в уже работающих отчётах) — компенсируем
        // только тут, вычитанием 1 перед вызовом.
        const QImage normalized = normalizeChartImage(chart.image);
        xlsx.insertImage(row - 1, imageCol - 1, normalized);

        // Worksheet::setRowHeight() молча не сработает на строке, у
        // которой ещё нет ни одной записанной ячейки — internally она
        // проверяется через checkDimensions(..., checkOnly=true), а
        // insertImage() (это оверлей-рисунок, не ячейка) в измерения листа
        // саму строку не добавляет. Пустая запись ниже — не "мусорная
        // ячейка", а обязательный шаг, чтобы высота вообще применилась.
        xlsx.write(row, imageCol, QString(), fmt.value);
        xlsx.setRowHeight(row, row, chartRowHeightPt());

        row += 2; // одна строка под картинку + отступ перед следующим блоком
    }

    // Отдельной "Дата после графиков" строки на листе больше нет — футер
    // (setFooterText() выше) сам показывает "Поз. ..., дата ..." на этой
    // же, графиковой странице точно так же, как и на первой.

    return xlsx.saveAs(outPath);
}

}
