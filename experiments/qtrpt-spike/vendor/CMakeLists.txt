# Вендоренный движок QtRPT (без QtRptDesigner/QtRptDemo/QtRptCGI — они
# нам не нужны для проверки самого движка отчётов). Только исходники под
# Apache License 2.0 (см. QtRPT/LICENSE.txt) — NO_BARCODE отключает
# единственную GPL-зависимость (Zint), она нам и не нужна.
#
# Источник: https://sourceforge.net/p/qtrpt/code_git/ (ветка master,
# коммит eee196e, версия 3.2.0).
#
# QtRptDesigner (визуальный редактор шаблонов) пробовали завендорить и
# собрать тоже — сознательно отказались: слой работы с полем на холсте
# (TContainerField.cpp и смежные) не синхронизирован с текущим движком
# (вызывает методы RptCrossTabObject, которых давно нет, ожидает
# QGraphicsItem-базу, которой у поля больше нет, плюс отдельные
# Qt5→Qt6 пропуски вроде QPalette::Background) — похоже, эту конфигурацию
# (Linux, Qt6) апстрим просто не собирает и не тестирует, готовых
# бинарников под Linux для актуальной версии тоже нет. Сам движок
# (то, что печатает PDF) — единственное, что реально нужно для печати
# готового шаблона — работает полностью корректно.

qt_add_library(QtRptCore STATIC
    QtRPT/qtrpt.cpp
    QtRPT/RptBandObject.cpp
    QtRPT/RptCrossTabObject.cpp
    QtRPT/RptDsAbstract.cpp
    QtRPT/RptDsInline.cpp
    QtRPT/RptDsPlugin.cpp
    QtRPT/RptFieldObject.cpp
    QtRPT/RptPageObject.cpp
    QtRPT/RptScriptEngine.cpp
    QtRPT/RptSql.cpp
    QtRPT/qtrpt.h
    QtRPT/qtrpt_global.h
    QtRPT/qtrptnamespace.h
    QtRPT/RptBandObject.h
    QtRPT/RptCrossTabObject.h
    QtRPT/RptDsAbstract.h
    QtRPT/RptDsInline.h
    QtRPT/RptDsPlugin.h
    QtRPT/RptFieldObject.h
    QtRPT/RptPageObject.h
    QtRPT/RptScriptEngine.h
    QtRPT/RptSql.h
    CommonFiles/CommonClasses.cpp
    CommonFiles/CommonClasses.h
    CommonFiles/numbertoarabicword.cpp
    CommonFiles/numbertoarabicword.h
    CommonFiles/Barcode.cpp
    CommonFiles/Barcode.h
    QtRPT/imagesRpt.qrc
)

target_compile_definitions(QtRptCore
    PUBLIC
        QTRPT_LIBRARY
        NO_BARCODE
)

target_include_directories(QtRptCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/QtRPT
        ${CMAKE_CURRENT_SOURCE_DIR}/CommonFiles
)

target_link_libraries(QtRptCore
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
        Qt6::Xml
        Qt6::Qml
        Qt6::Sql
        Qt6::Charts
)
