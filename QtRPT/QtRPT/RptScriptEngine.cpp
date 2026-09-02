/*
Name: QtRpt
Version: 3.2.0
Web-site: https://qtrpt.sourceforge.io
Programmer: Oleksii Osypov
E-mail: aliks-os@ukr.net
Web-site: http://www.aliks-os.tk

Copyright 2012-2025 Oleksii Osypov

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "RptScriptEngine.h"
#include <QDebug>
#include <QtMath>
#include "CommonClasses.h"
#include "qtrpt.h"

Q_DECLARE_METATYPE(QColor);

RptScriptEngine::RptScriptEngine(QObject *parent) :
    QJSEngine(parent)
{
    qRegisterMetaType<PageList>("<QList<RptPageObject*> >");
    qRegisterMetaType<PageList>("PageList");
    qRegisterMetaType<QtRPT*>("QtRPT*");
    qRegisterMetaType<RptPageObject*>("RptPageObject*");
    qRegisterMetaType<RptBandObject*>();
    qRegisterMetaType<RptFieldObject*>();

    scriptAPI.setParent(this);
    QJSValue wrapobj = this->newQObject(&scriptAPI);
    this->globalObject().setProperty("QColor", wrapobj.property("qcolorValue"));
    this->globalObject().setProperty("Replace", wrapobj.property("funcReplace"));
    this->globalObject().setProperty("Substring", wrapobj.property("funcSubstring"));
    this->globalObject().setProperty("ToUpper", wrapobj.property("funcToUpper"));
    this->globalObject().setProperty("ToLower", wrapobj.property("funcToLower"));
    this->globalObject().setProperty("Frac", wrapobj.property("funcFrac"));
    this->globalObject().setProperty("Floor", wrapobj.property("funcFloor"));
    this->globalObject().setProperty("Ceil", wrapobj.property("funcCeil"));
    this->globalObject().setProperty("Round", wrapobj.property("funcRound"));
    this->globalObject().setProperty("debug", wrapobj.property("funcDebug"));
    this->globalObject().setProperty("NumberToWords", wrapobj.property("funcNumberToWords"));
    this->globalObject().setProperty("SUM", wrapobj.property("funcAggregate"));

    addObject(parent);
}

void RptScriptEngine::addObject(QObject *object)
{
    QJSValue scriptObject = this->newQObject(object);

    auto qtrpt = qobject_cast<QtRPT*>(object);
    if (qtrpt)
        this->globalObject().setProperty("QtRPT", scriptObject);
    else
        this->globalObject().setProperty(object->objectName(), scriptObject);
}

QJSValue RptScriptEngine::evaluate(const QString &program, const QString &fileName, int lineNumber)
{
    Q_UNUSED(fileName);
    Q_UNUSED(lineNumber);

    QJSValue result = QJSEngine::evaluate(program);

    if (result.isError())
    {
        qDebug()
                << "Uncaught exception at line"
                << result.property("lineNumber").toInt()
                << ":" << result.toString();
    }

    return result;
}

void RptScriptEngine::setListOfPair(QList<AggregateValues> *listOfPair)
{
    this->listOfPair = listOfPair;
}

void RptScriptEngine::setRowList(QList<RowData> *rowList)
{
    this->rowList = rowList;
}

void RptScriptEngine::setGroupIdxList(QList<int> *GroupIdxList, int group)
{
    if (group == 0) GroupIdxList_0 = GroupIdxList;
    else if (group == 1) GroupIdxList_1 = GroupIdxList;
    else if (group == 2) GroupIdxList_2 = GroupIdxList;
}
//--------------------------------------------------------

ScriptAPI::ScriptAPI(QObject *parent) :
    QObject(parent)
{

}

QColor ScriptAPI::qcolorValue(int r, int g, int b)
{
    return QColor(r,g,b,255);
}

QString ScriptAPI::funcReplace(QString param, QString strBefore, QString strAfter)
{
    return param.replace(strBefore, strAfter);
}

QString ScriptAPI::funcSubstring(QString param, int startPos, int length)
{
    return param.mid(startPos, length);
}

QString ScriptAPI::funcToUpper(QString param)
{
    return param.toUpper();
}

QString ScriptAPI::funcToLower(QString param)
{
    return param.toLower();
}

int ScriptAPI::funcFrac(double value)
{
    int b = qFloor(value);
    b = (value-b) * 100 + 0.5;
    return b;
}

int ScriptAPI::funcFloor(double value)
{
    return qFloor(value);
}

int ScriptAPI::funcCeil(double value)
{
    return qCeil(value);
}

int ScriptAPI::funcRound(double value)
{
    return qRound(value);
}

void ScriptAPI::funcDebug(QString value)
{
    qDebug() << value;
}

QString ScriptAPI::funcNumberToWords(QString paramLanguage, double value)
{
    return double2Money(value, paramLanguage);
}

static double parseNumberAutoLocale(const QString &text, bool *ok = nullptr)
{
    QString trimmed = text.trimmed();

    if (trimmed.contains(',') && !trimmed.contains('.')) {
        return QLocale(QLocale::German).toDouble(trimmed, ok);
    } else if (trimmed.contains('.') && !trimmed.contains(',')) {
        return QLocale::c().toDouble(trimmed, ok);
    } else if (trimmed.contains(',') && trimmed.contains('.')) {
        double val = QLocale(QLocale::German).toDouble(trimmed, ok);
        if (ok && *ok) return val;
        val = QLocale::c().toDouble(trimmed, ok);
        return val;
    } else {
        return QLocale::system().toDouble(trimmed, ok);
    }
}

double ScriptAPI::funcAggregate(int funcMode, QString paramName)
{
    paramName = paramName.replace("'", "");
    auto engine = qobject_cast<RptScriptEngine*>(this->parent());

    double total = 0;
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::lowest();
    int count = 0;

    QList<int> *groupIdxList = nullptr;
    int groupLevel = engine->globalObject().property("groupLevel").toInt();
    if (groupLevel == 0)
        groupIdxList = engine->GroupIdxList_0;
    else if (groupLevel == 1)
        groupIdxList = engine->GroupIdxList_1;
    else if (groupLevel == 2)
        groupIdxList = engine->GroupIdxList_2;

    bool showInGroup = engine->globalObject().property("showInGroup").toBool();

    for (const auto &aggValues : *engine->listOfPair) {
        if (aggValues.paramName != paramName)
            continue;

        QString rawText = aggValues.paramValue.toString();
        if (rawText.isEmpty())
            continue;

        bool ok = false;
        double value = parseNumberAutoLocale(rawText, &ok);
        if (!ok)
            continue;

        bool include = true;
        if (groupIdxList && !groupIdxList->isEmpty() && showInGroup)
            include = groupIdxList->contains(aggValues.lnNo);

        if (include) {
            total += value;
            count += 1;
            if (value > max) max = value;
            if (value < min) min = value;
        }
    }

    switch (funcMode) {
    case 0: return total; // SUM
    case 1: return (count > 0 ? total / count : 0); // AVG
    case 2: return count; // COUNT
    case 3: return (count > 0 ? max : 0); // MAX
    case 4: return (count > 0 ? min : 0); // MIN
    default: return 0;
    }
}


/*double ScriptAPI::funcAggregate(int funcMode, QString paramName)
{
    paramName = paramName.replace("'", "");
    auto engine = qobject_cast<RptScriptEngine*>(this->parent());

    double total = 0;
    double min = 0;
    double max = 0;
    int count = 0;

    QList<int> *groupIdxList = nullptr;
    if (engine->globalObject().property("groupLevel").toInt() == 0)
        groupIdxList = engine->GroupIdxList_0;
    else if (engine->globalObject().property("groupLevel").toInt() == 1)
        groupIdxList = engine->GroupIdxList_1;
    else if (engine->globalObject().property("groupLevel").toInt() == 2)
        groupIdxList = engine->GroupIdxList_2;

    if (!engine->listOfPair->isEmpty())
        min = engine->listOfPair->first().paramValue.toDouble();  //set initial value for Min

    for (auto &aggValues : *engine->listOfPair) {
        if (aggValues.paramName == paramName) {
            if (groupIdxList && !groupIdxList->isEmpty() && engine->globalObject().property("showInGroup").toBool() == true) {
                for (auto &grpIdx : *groupIdxList)
                {
                    if (grpIdx == aggValues.lnNo)
                    {
                        total += aggValues.paramValue.toDouble();
                        count += 1;
                        if (max < aggValues.paramValue.toDouble())
                            max = aggValues.paramValue.toDouble();
                        if (min > aggValues.paramValue.toDouble())
                            min = aggValues.paramValue.toDouble();
                    }
                }
            } else {
                if (!aggValues.paramValue.toString().isEmpty()) {
                    total += aggValues.paramValue.toDouble();
                    count += 1;
                    if (max < aggValues.paramValue.toDouble())
                        max = aggValues.paramValue.toDouble();
                    if (min > aggValues.paramValue.toDouble())
                        min = aggValues.paramValue.toDouble();
                }
            }
        }
    }

    switch (funcMode) {
    case 0:  //SUM
        return total;
    case 1:  //AVG
        if (count > 0)
            return total/count;
        else
            return 0;
    case 2:  //COUNT
        return count;
    case 3:  //MAX
        return max;
    case 4:  //MIN
        return min;
    default: return 0;
    }
    return 0;
}*/
