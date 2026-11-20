# Run from CTest or directly: cmake -DPROJECT_ROOT=... -P this-file.
foreach(layer Domain Report Storage Shared)
    file(GLOB_RECURSE sources "${PROJECT_ROOT}/Src/${layer}/*.h" "${PROJECT_ROOT}/Src/${layer}/*.cpp")
    if(layer STREQUAL "Domain")
        list(APPEND sources
            "${PROJECT_ROOT}/Src/Utils/NumberUtils.h"
            "${PROJECT_ROOT}/Src/Utils/NumberUtils.cpp"
            "${PROJECT_ROOT}/Src/Utils/SignalUtils.h"
            "${PROJECT_ROOT}/Src/Utils/TimeUtils.h"
            "${PROJECT_ROOT}/Src/Utils/TimeUtils.cpp")
    endif()
    foreach(source IN LISTS sources)
        file(STRINGS "${source}" includes REGEX "^[ \t]*#[ \t]*include")
        foreach(line IN LISTS includes)
            if(line MATCHES "[\"<](Gui/|Widgets/|Utils/StyledMessageBox\\.h|Utils/UiText\\.h|Utils/Shortcuts/|ValidatorFactory/)")
                message(FATAL_ERROR "${source}: lower layer includes application UI: ${line}")
            endif()
            if(NOT layer STREQUAL "Report" AND line MATCHES "[<](QtWidgets/|QtCharts/|QApplication[>]|QWidget[>]|QDialog[>]|QLineEdit[>]|QLabel[>]|QMessageBox[>])")
                message(FATAL_ERROR "${source}: lower layer includes a UI toolkit header: ${line}")
            endif()
        endforeach()
    endforeach()
endforeach()
message(STATUS "Domain, Report, Storage and Shared do not include application UI")
