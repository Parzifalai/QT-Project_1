# Указываем минимальную требуемую версию CMake
cmake_minimum_required(VERSION 3.5)

# Название проекта, версия и используемые языки
project(minisynth-qt VERSION 1.0 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON) # Автоматически обрабатывать .ui файлы
set(CMAKE_AUTOMOC ON)  # Автоматически генерировать moc файлы
set(CMAKE_AUTORCC ON) # Автоматически обрабатывать .qrc файлы
# Устанавливаем стандарт  и поддержку C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt5 Qt6) # Ищем пакет Qt, поддерживающий версии Qt5 или Qt6

if ((CMAKE_SYSTEM_NAME MATCHES "Linux") AND (QT_VERSION_MAJOR EQUAL 6) AND (QT_VERSION VERSION_LESS 6.4)) # Проверка версии Qt для Linux.  Если версия Qt6 и меньше 6.4, выводим предупреждение.
    message(WARNING "Unsupported Qt version ${QT_VERSION} for system ${CMAKE_SYSTEM_NAME}")
endif()
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Multimedia REQUIRED)

# Список исходных файлов проекта
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    tonesynth.h
    tonesynth.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(minisynth-qt
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(minisynth-qt
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(minisynth-qt PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
)

# Специальные опции для Emscripten
if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    target_link_options(minisynth-qt PUBLIC -sASYNCIFY -Os)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_target(minisynth-qt)
endif()
