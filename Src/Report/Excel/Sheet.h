#pragma once

#include <QHash>
#include <QString>
#include <QVariant>

#include "xlsxformat.h"

class QImage;

namespace QXlsx {
class Document;
class Worksheet;
}

namespace Report::Excel {

// Лист отчёта, который строится кодом, без .xlsx-шаблона.
//
// Сетка — kColumns одинаковых столбцов (B..P, столбец A — узкое поле), все
// блоки адресуют столбцы номерами сетки 1..kColumns, а строки — бегущим
// курсором row(): блок пропущен (нет материалов, нет соленоида) — всё ниже
// само сдвигается. Ни одного абсолютного номера строки, в отличие от
// шаблонных Pages/Blocks.
//
// Вертикальных объединений нет, кроме одного осознанного (текст заключения
// рядом со списком ЗИП в Acts::technicalInspection) — там весь диапазон
// строк принадлежит одному блоку и больше в него никто не пишет.
class Sheet final {
public:
    static constexpr int kColumns = 15;

    // Две половины листа для таблиц "подпись — значение" рядом друг с
    // другом (объект | спецификация); столбец 8 — промежуток.
    static constexpr int kLeftFirst = 1;
    static constexpr int kLeftLast = 7;
    static constexpr int kRightFirst = 9;
    static constexpr int kRightLast = kColumns;

    struct Styles {
        QXlsx::Format title;
        QXlsx::Format titleRight;
        QXlsx::Format header;
        QXlsx::Format heading;   // заголовок без рамки (над графиком)
        QXlsx::Format label;
        QXlsx::Format value;
        QXlsx::Format cell;
        QXlsx::Format cellOk;
        QXlsx::Format cellFail;
        QXlsx::Format text;
        QXlsx::Format caption;
        QXlsx::Format underline; // значение на линии ("Дата: ____")
    };

    // Создаёт лист name в doc (первый лист книги переименовывается) и
    // настраивает печать: A4, книжная, вписать по ширине в 1 страницу.
    Sheet(QXlsx::Document& doc, const QString& name);

    int row() const { return m_row; }
    void skip(int rows = 1) { m_row += rows; }

    // Принудительный разрыв страницы после последней строки блока
    // (курсор уже стоит на следующей), разрыв после строки row()-1.
    void pageBreak();

    // Значение в строках [rowFirst..rowLast] и столбцах сетки [first..last];
    // если это больше одной ячейки — объединяет. Формат ложится на весь
    // диапазон (рамка вокруг всего объединения, а не только угла).
    void put(int rowFirst, int rowLast, int first, int last,
             const QVariant& value, const QXlsx::Format& format);
    void put(int row, int first, int last,
             const QVariant& value, const QXlsx::Format& format)
    {
        put(row, row, first, last, value, format);
    }

    // Excel не подбирает высоту строки под перенесённый текст в
    // объединённых ячейках — считаем сами по ширине отрезка [first..last].
    // Высота строки только растёт: несколько ячеек одной строки берут
    // максимум.
    void fitHeight(int row, int first, int last, const QString& text, double fontPt = 11);
    void setMinHeight(int row, double pt);
    double height(int row) const;

    // Сколько строк по 11pt займёт text на ширине [first..last].
    int lineCount(int first, int last, const QString& text, double fontPt = 11) const;

    // Выпадающий список на ячейках столбца сетки gridColumn в строках
    // [rowFirst..rowLast] (у объединённых ячеек — левый верхний угол).
    // formula — диапазон ('Лист'!$A$1:$A$9) или список в кавычках
    // ("да,нет"); пустая — ничего не делает. Это подсказка, а не запрет:
    // свой текст ввести тоже можно, Excel не ругается.
    void dropdown(int rowFirst, int rowLast, int gridColumn, const QString& formula);

    // Картинка в текущей строке на ширину столбцов [first..last], высота =
    // ширина * aspect; строка получает высоту картинки, курсор уходит на
    // следующую.
    void image(const QImage& image, int first, int last, double aspect);

    // Колонтитул внизу каждой печатной страницы: слева left, по центру
    // "Стр. N из M", справа right.
    void setFooter(const QString& left, const QString& right);

    const Styles& styles() const { return m_styles; }

private:
    int column(int gridColumn) const { return gridColumn + 1; }
    double widthPx(int first, int last) const;

    QXlsx::Worksheet* m_ws = nullptr;
    Styles m_styles;
    QHash<int, double> m_heights;
    int m_row = 1;
};

}
