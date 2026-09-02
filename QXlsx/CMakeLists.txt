cmake_minimum_required(VERSION 3.21)

project(QXlsx LANGUAGES CXX)

set(QXLSX_CPP
    source/xlsxabstractooxmlfile.cpp
    source/xlsxabstractsheet.cpp
    source/xlsxcell.cpp
    source/xlsxcellformula.cpp
    source/xlsxcelllocation.cpp
    source/xlsxcellrange.cpp
    source/xlsxcellreference.cpp
    source/xlsxchart.cpp
    source/xlsxchartsheet.cpp
    source/xlsxcolor.cpp
    source/xlsxconditionalformatting.cpp
    source/xlsxcontenttypes.cpp
    source/xlsxdatavalidation.cpp
    source/xlsxdatetype.cpp
    source/xlsxdocpropsapp.cpp
    source/xlsxdocpropscore.cpp
    source/xlsxdocument.cpp
    source/xlsxdrawing.cpp
    source/xlsxdrawinganchor.cpp
    source/xlsxformat.cpp
    source/xlsxmediafile.cpp
    source/xlsxnumformatparser.cpp
    source/xlsxrelationships.cpp
    source/xlsxrichstring.cpp
    source/xlsxsharedstrings.cpp
    source/xlsxsimpleooxmlfile.cpp
    source/xlsxstyles.cpp
    source/xlsxtheme.cpp
    source/xlsxutility.cpp
    source/xlsxworkbook.cpp
    source/xlsxworksheet.cpp
    source/xlsxzipreader.cpp
    source/xlsxzipwriter.cpp
)

set(QXLSX_PUBLIC_HEADERS
    header/xlsxabstractooxmlfile.h
    header/xlsxabstractsheet.h
    header/xlsxcell.h
    header/xlsxcellformula.h
    header/xlsxcelllocation.h
    header/xlsxcellrange.h
    header/xlsxcellreference.h
    header/xlsxchart.h
    header/xlsxchartsheet.h
    header/xlsxconditionalformatting.h
    header/xlsxdatavalidation.h
    header/xlsxdatetype.h
    header/xlsxdocument.h
    header/xlsxformat.h
    header/xlsxglobal.h
    header/xlsxrichstring.h
    header/xlsxworkbook.h
    header/xlsxworksheet.h
)

set(QXLSX_PRIVATE_HEADERS
    header/xlsxabstractooxmlfile_p.h
    header/xlsxabstractsheet_p.h
    header/xlsxcell_p.h
    header/xlsxcellformula_p.h
    header/xlsxchartsheet_p.h
    header/xlsxchart_p.h
    header/xlsxcolor_p.h
    header/xlsxconditionalformatting_p.h
    header/xlsxcontenttypes_p.h
    header/xlsxdatavalidation_p.h
    header/xlsxdocpropsapp_p.h
    header/xlsxdocpropscore_p.h
    header/xlsxdocument_p.h
    header/xlsxdrawinganchor_p.h
    header/xlsxdrawing_p.h
    header/xlsxformat_p.h
    header/xlsxmediafile_p.h
    header/xlsxnumformatparser_p.h
    header/xlsxrelationships_p.h
    header/xlsxrichstring_p.h
    header/xlsxsharedstrings_p.h
    header/xlsxsimpleooxmlfile_p.h
    header/xlsxstyles_p.h
    header/xlsxtheme_p.h
    header/xlsxutility_p.h
    header/xlsxworkbook_p.h
    header/xlsxworksheet_p.h
    header/xlsxzipreader_p.h
    header/xlsxzipwriter_p.h
)

add_library(QXlsx STATIC
    ${QXLSX_CPP}
    ${QXLSX_PUBLIC_HEADERS}
    ${QXLSX_PRIVATE_HEADERS}
)

add_library(QXlsx::QXlsx ALIAS QXlsx)

target_include_directories(QXlsx
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/header
)

target_link_libraries(QXlsx
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

get_target_property(QT_CORE_INCDIR Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(QT_GUI_INCDIR Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)

list(GET QT_CORE_INCDIR 0 QT_CORE_INCLUDE_BASE)
list(GET QT_GUI_INCDIR  0 QT_GUI_INCLUDE_BASE)

set(QT_VER "${Qt6Core_VERSION_MAJOR}.${Qt6Core_VERSION_MINOR}.${Qt6Core_VERSION_PATCH}")

target_include_directories(QXlsx
    PRIVATE
        "${QT_CORE_INCLUDE_BASE}/${QT_VER}"
        "${QT_CORE_INCLUDE_BASE}/${QT_VER}/QtCore"
        "${QT_GUI_INCLUDE_BASE}/${QT_VER}"
        "${QT_GUI_INCLUDE_BASE}/${QT_VER}/QtGui"
)

target_compile_definitions(QXlsx
    PRIVATE
        QT_NO_KEYWORDS
        QT_NO_CAST_TO_ASCII
        QT_NO_CAST_FROM_ASCII
        QT_NO_URL_CAST_FROM_STRING
        QT_NO_CAST_FROM_BYTEARRAY
        QT_USE_QSTRINGBUILDER
        QT_NO_SIGNALS_SLOTS_KEYWORDS
        QT_USE_FAST_OPERATOR_PLUS
)