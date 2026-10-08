// xlsxworksheet.h

#ifndef XLSXWORKSHEET_H
#define XLSXWORKSHEET_H

#include <QtGlobal>
#include <QObject>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include <QPointF>
#include <QIODevice>
#include <QDateTime>
#include <QUrl>
#include <QImage>

#include "xlsxabstractsheet.h"
#include "xlsxcell.h"
#include "xlsxcellrange.h"
#include "xlsxcellreference.h"
#include "xlsxcelllocation.h"

class WorksheetTest;

QT_BEGIN_NAMESPACE_XLSX

class DocumentPrivate;
class Workbook;
class Format;
class Drawing;
class DataValidation;
class ConditionalFormatting;
class CellRange;
class RichString;
class Relationships;
class Chart;

class WorksheetPrivate;
class QXLSX_EXPORT Worksheet : public AbstractSheet
{
    Q_DECLARE_PRIVATE(Worksheet)

private:
    friend class DocumentPrivate;
    friend class Workbook;
    friend class ::WorksheetTest;
    Worksheet(const QString &sheetName, int sheetId, Workbook *book, CreateFlag flag);
    Worksheet *copy(const QString &distName, int distId) const override;

public:
    ~Worksheet();

public:
    bool write(const CellReference &row_column, const QVariant &value, const Format &format=Format());
    bool write(int row, int column, const QVariant &value, const Format &format=Format());

    QVariant read(const CellReference &row_column) const;
    QVariant read(int row, int column) const;

    bool writeString(const CellReference &row_column, const QString &value, const Format &format=Format());
    bool writeString(int row, int column, const QString &value, const Format &format=Format());
    bool writeString(const CellReference &row_column, const RichString &value, const Format &format=Format());
    bool writeString(int row, int column, const RichString &value, const Format &format=Format());

    bool writeInlineString(const CellReference &row_column, const QString &value, const Format &format=Format());
    bool writeInlineString(int row, int column, const QString &value, const Format &format=Format());

    bool writeNumeric(const CellReference &row_column, double value, const Format &format=Format());
    bool writeNumeric(int row, int column, double value, const Format &format=Format());

    bool writeFormula(const CellReference &row_column, const CellFormula &formula, const Format &format=Format(), double result=0);
    bool writeFormula(int row, int column, const CellFormula &formula, const Format &format=Format(), double result=0);

    bool writeBlank(const CellReference &row_column, const Format &format=Format());
    bool writeBlank(int row, int column, const Format &format=Format());

    bool writeBool(const CellReference &row_column, bool value, const Format &format=Format());
    bool writeBool(int row, int column, bool value, const Format &format=Format());

    bool writeDateTime(const CellReference &row_column, const QDateTime& dt, const Format &format=Format());
    bool writeDateTime(int row, int column, const QDateTime& dt, const Format &format=Format());

    // dev67
    bool writeDate(const CellReference &row_column, const QDate& dt, const Format &format=Format());
    bool writeDate(int row, int column, const QDate& dt, const Format &format=Format());

    bool writeTime(const CellReference &row_column, const QTime& t, const Format &format=Format());
    bool writeTime(int row, int column, const QTime& t, const Format &format=Format());

    bool writeHyperlink(const CellReference &row_column, const QUrl &url, const Format &format=Format(), const QString &display=QString(), const QString &tip=QString());
    bool writeHyperlink(int row, int column, const QUrl &url, const Format &format=Format(), const QString &display=QString(), const QString &tip=QString());

    bool addDataValidation(const DataValidation &validation);
    bool addConditionalFormatting(const ConditionalFormatting &cf);

    Cell *cellAt(const CellReference &row_column) const;
    Cell *cellAt(int row, int column) const;

    int insertImage(int row, int column, const QImage &image);
    bool getImage(int imageIndex, QImage& img);
    bool getImage(int row, int column, QImage& img);
    uint getImageCount();

    Chart *insertChart(int row, int column, const QSize &size);

    bool mergeCells(const CellRange &range, const Format &format=Format());
    bool unmergeCells(const CellRange &range);
    QList<CellRange> mergedCells() const;

    bool setColumnWidth(const CellRange& range, double width);
    bool setColumnFormat(const CellRange& range, const Format &format);
    bool setColumnHidden(const CellRange& range, bool hidden);
    bool setColumnWidth(int colFirst, int colLast, double width);
    bool setColumnFormat(int colFirst, int colLast, const Format &format);
    bool setColumnHidden(int colFirst, int colLast, bool hidden);

    double columnWidth(int column);
    Format columnFormat(int column);
    bool isColumnHidden(int column);

    bool setRowHeight(int rowFirst,int rowLast, double height);
    bool setRowFormat(int rowFirst,int rowLast, const Format &format);
    bool setRowHidden(int rowFirst,int rowLast, bool hidden);

    double rowHeight(int row);
    Format rowFormat(int row);
    bool isRowHidden(int row);

    bool groupRows(int rowFirst, int rowLast, bool collapsed = true);
    bool groupColumns(int colFirst, int colLast, bool collapsed = true);
    bool groupColumns(const CellRange &range, bool collapsed = true);
    CellRange dimension() const;

    bool isWindowProtected() const;
    void setWindowProtected(bool protect);
    bool isFormulasVisible() const;
    void setFormulasVisible(bool visible);
    bool isGridLinesVisible() const;
    void setGridLinesVisible(bool visible);
    bool isRowColumnHeadersVisible() const;
    void setRowColumnHeadersVisible(bool visible);
    bool isZerosVisible() const;
    void setZerosVisible(bool visible);
    bool isRightToLeft() const;
    void setRightToLeft(bool enable);
    bool isSelected() const;
    void setSelected(bool select);
    bool isRulerVisible() const;
    void setRulerVisible(bool visible);
    bool isOutlineSymbolsVisible() const;
    void setOutlineSymbolsVisible(bool visible);
    bool isWhiteSpaceVisible() const;
    void setWhiteSpaceVisible(bool visible);
 	bool setStartPage(int spagen); //add by liufeijin20181028

    // Печать: PpaperSize/Pscale/Porientation/PMxxx (см. xlsxworksheet_p.h,
    // "pagesetup and print settings add by liufeijin") уже читаются и
    // пишутся в XML, но были доступны только через загрузку существующего
    // файла — не было ни одного публичного сеттера, чтобы задать их для
    // листа, построенного с нуля. rowBreaks (принудительный разрыв
    // страницы) не было вообще, ни на чтение, ни на запись. См.
    // QXlsx/LOCAL_PATCHES.md, раздел про печать/пагинацию.
    void setPageOrientation(const QString &orientation); // "portrait" | "landscape"
    void setPaperSize(int paperSize);                    // код ECMA-376 §18.17.3.31, 9 = A4
    void setPageScale(int percent);                      // 100 = 100%
    void setPageMargins(double left, double right, double top, double bottom,
                        double header, double footer);   // дюймы
    bool addRowPageBreak(int row);                       // принудительный разрыв ПОСЛЕ этой строки

    // "Вписать по ширине/высоте" — надёжнее фиксированного setPageScale(),
    // когда ширина содержимого заранее не известна точно (её никто не
    // считает руками — Excel сам подбирает масштаб под реальные ширины
    // столбцов). heightPages=0 означает "сколько страниц по высоте
    // понадобится", без ограничения — самый частый случай (ограничивают
    // обычно только ширину).
    void setFitToPage(int widthPages, int heightPages = 0);

    // oddHeader/oddFooter уже читаются/пишутся (см. saveToXmlFile) — опять
    // не было публичного сеттера для листа, построенного с нуля. Текст —
    // в родном формате Excel header/footer: &L/&C/&R переключают левую/
    // центральную/правую секцию, &P/&N — номер страницы/страниц всего,
    // &"Шрифт,Стиль" и &<размер> — шрифт/кегль. Без цветового кода (&K...)
    // текст остаётся чёрным — ровно то, что нужно для футера.
    void setHeaderText(const QString &oddHeader);
    void setFooterText(const QString &oddFooter);

    QVector<CellLocation> getFullCells(int* maxRow, int* maxCol);

private:
    void saveToXmlFile(QIODevice *device) const override;
    bool loadFromXmlFile(QIODevice *device) override;
};

QT_END_NAMESPACE_XLSX
#endif // XLSXWORKSHEET_H
