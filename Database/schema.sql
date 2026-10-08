-- Черновая схема истории тестов арматуры (SQLite).
--
-- Заменяет две вещи разом:
--  1) Registry (Src/Storage/Registry.h) — сейчас хранит только ТЕКУЩУЮ
--     карточку клапана поверх вложенных QSettings-групп
--     object -> manufactory -> department -> position, истории нет.
--     Здесь valve_snapshots — снимок карточки на момент КАЖДОЙ сессии,
--     не перезаписываемое состояние.
--  2) Result-структуры (Src/Domain/Tests/**/Result.h) — сейчас существуют
--     только в памяти во время живого теста и никогда не сохраняются
--     отдельно (только то, что попало в напечатанный Excel/PDF).
--
-- .data (сырые точки графика) сюда НЕ переносятся — остаются файлами на
-- диске, здесь только путь (chart_snapshots.file_path). Причина: файлы
-- маленькие (1.6-95 КБ по факту), совместимость с MpiSki, лаборанту проще
-- скопировать файл, чем лезть в БД; риск не оправдан.
--
-- source='harvest' — строки, восстановленные из архива старых report.xlsx
-- (см. отдельный ML-проект, dataset.jsonl); source='live' — то, что
-- приложение пишет само сразу после теста, с более полным набором полей.

PRAGMA foreign_keys = ON;

-- ============================== Иерархия объекта/клапана ==============

CREATE TABLE sites (
    id           INTEGER PRIMARY KEY,
    object       TEXT NOT NULL,
    manufactory  TEXT NOT NULL DEFAULT '',
    department   TEXT NOT NULL DEFAULT '',
    UNIQUE (object, manufactory, department)
);

CREATE TABLE positions (
    id               INTEGER PRIMARY KEY,
    site_id          INTEGER NOT NULL REFERENCES sites(id),
    position_number  TEXT NOT NULL,
    UNIQUE (site_id, position_number)
);

-- ============================== Сессия теста ===========================

CREATE TABLE sessions (
    id              INTEGER PRIMARY KEY,
    position_id     INTEGER NOT NULL REFERENCES positions(id),
    folder_path     TEXT,             -- путь к папке с датой (для .data/отчёта)
    ts              TEXT NOT NULL,    -- ISO8601
    variant         TEXT,             -- Domain::kVariant (Ski/Ilim/...)
    pattern         TEXT,             -- SelectTests::PatternType
    operator        TEXT,
    app_version     TEXT,
    schema_version  INTEGER NOT NULL,
    source          TEXT NOT NULL CHECK (source IN ('live', 'harvest')),
    -- Ручная отметка оператора "нуждается в ремонте" — привязана к
    -- конкретной сессии (дате), не к позиции в целом: клапан мог быть
    -- отремонтирован между двумя визитами, см. HistoryPresenter::
    -- onNeedsRepairToggled()/checkBox_history_needsRepair в MainWindow.ui.
    needs_repair    INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX idx_sessions_position ON sessions(position_id);

-- Снимок карточки клапана НА МОМЕНТ этой сессии — 1:1 с sessions,
-- умышленно не переиспользуется между сессиями, даже если ничего не
-- поменялось: сравнение "настройки отличаются" всегда просто diff двух
-- строк по session_id, без дополнительной логики "а не поменялось ли".
CREATE TABLE valve_snapshots (
    session_id                  INTEGER PRIMARY KEY REFERENCES sessions(id),

    manufacturer                TEXT,
    valve_model                 TEXT,
    serial_number                TEXT,
    dn_mm                       REAL,
    pn_bar                      REAL,
    dn_pn_raw                   TEXT,   -- как было в источнике, если не распозналось

    positioner_model            TEXT,
    solenoid_valve_model        TEXT,
    limit_switch_model          TEXT,
    position_sensor_model       TEXT,

    stroke_movement             TEXT CHECK (stroke_movement IN ('linear', 'rotary')),
    valve_stroke                TEXT,
    drive_model                 TEXT,
    safe_position                TEXT CHECK (safe_position IN ('normally_closed', 'normally_open')),
    drive_type                  TEXT CHECK (drive_type IN ('spring_diaphragm', 'single_sided', 'double_acting')),
    positioner_type              TEXT,

    drive_range_low              REAL,
    drive_range_high             REAL,
    drive_diameter               REAL,
    tool_number                  TEXT,
    diameter_pulley              REAL,
    material_stuffing_box_seal   TEXT,

    -- CrossingLimits — применённые пороги на момент теста
    crossing_friction_enabled              INTEGER,
    crossing_friction_lower                REAL,
    crossing_friction_upper                REAL,
    crossing_linear_characteristic_enabled INTEGER,
    crossing_linear_characteristic         REAL,
    crossing_valve_stroke_enabled          INTEGER,
    crossing_valve_stroke                  REAL,
    crossing_spring_enabled                INTEGER,
    crossing_spring_lower                  REAL,
    crossing_spring_upper                  REAL,
    crossing_dynamic_error_enabled         INTEGER
);

-- Материалы компонентов клапана НА МОМЕНТ этой сессии — тот же принцип
-- "снимок, не текущее состояние", что и у valve_snapshots, отдельной
-- таблицей (а не колонками в valve_snapshots): это MaterialsOfComponentParts,
-- самостоятельная структура в Registry.h со своим отдельным чтением/записью,
-- не часть ValveInfo.
CREATE TABLE valve_snapshot_materials (
    session_id          INTEGER PRIMARY KEY REFERENCES sessions(id),
    plunger              TEXT,
    saddle                TEXT,
    cap                   TEXT,
    corpus                TEXT,
    ball                  TEXT,
    disk                  TEXT,
    shaft                 TEXT,
    stock                 TEXT,
    guide_sleeve          TEXT,
    stuffing_box_seal     TEXT,
    cv                    TEXT
);

-- Ссылка на сырые точки графика — сам .data не трогаем.
CREATE TABLE chart_snapshots (
    id          INTEGER PRIMARY KEY,
    session_id  INTEGER NOT NULL REFERENCES sessions(id),
    chart_name  TEXT NOT NULL,   -- "Stroke" / "Step" / "Response" / "Resolution" / ...
    file_path   TEXT NOT NULL
);

CREATE INDEX idx_chart_snapshots_session ON chart_snapshots(session_id);

-- ============================== Результаты тестов ======================

CREATE TABLE stroke_results (
    session_id               INTEGER PRIMARY KEY REFERENCES sessions(id),
    forward_time_ms          INTEGER,
    backward_time_ms         INTEGER,
    -- Считаются, показываются в UI живого теста, но НИКОГДА не попадали в
    -- Excel/PDF (см. TelemetryUiMapper.cpp vs Excel/Blocks/CyclicSummary.cpp) —
    -- сейчас единственное место, где это вообще сохранится.
    forward_signal_delay_ms  INTEGER,
    backward_signal_delay_ms INTEGER
);

CREATE TABLE step_results (
    session_id  INTEGER PRIMARY KEY REFERENCES sessions(id),
    test_value  INTEGER
);

CREATE TABLE step_result_rows (
    id          INTEGER PRIMARY KEY,
    session_id  INTEGER NOT NULL REFERENCES step_results(session_id),
    row_index   INTEGER NOT NULL,
    from_pct    REAL,
    to_pct      REAL,
    t_value_ms  INTEGER,
    overshoot_pct REAL
);

CREATE INDEX idx_step_rows_session ON step_result_rows(session_id);

CREATE TABLE response_results (
    session_id       INTEGER PRIMARY KEY REFERENCES sessions(id),
    base_task        REAL,
    base_actual      REAL,
    base_actual_two  REAL,
    hysteresis       REAL   -- Result::gis
);

CREATE TABLE response_result_rows (
    id           INTEGER PRIMARY KEY,
    session_id   INTEGER NOT NULL REFERENCES response_results(session_id),
    direction    TEXT NOT NULL CHECK (direction IN ('up', 'down')),
    row_index    INTEGER NOT NULL,
    selected_pct REAL,
    actual_pct   REAL,
    error_pct    REAL,
    allowed_pct  REAL,
    -- Детерминированная производная (порог поверх error_pct/allowed_pct) —
    -- держим для UI, но это кандидат на исключение из ML-фич (data leakage).
    is_ok        INTEGER
);

CREATE INDEX idx_response_rows_session ON response_result_rows(session_id);

-- Resolution — та же форма, что и Response (Analyzer.cpp у них почти
-- идентичен, см. эту же сессию ранее).
CREATE TABLE resolution_results (
    session_id       INTEGER PRIMARY KEY REFERENCES sessions(id),
    base_task        REAL,
    base_actual      REAL
);

CREATE TABLE resolution_result_rows (
    id           INTEGER PRIMARY KEY,
    session_id   INTEGER NOT NULL REFERENCES resolution_results(session_id),
    direction    TEXT NOT NULL CHECK (direction IN ('up', 'down')),
    row_index    INTEGER NOT NULL,
    selected_pct REAL,
    actual_pct   REAL,
    error_pct    REAL,
    allowed_pct  REAL,
    is_ok        INTEGER
);

CREATE INDEX idx_resolution_rows_session ON resolution_result_rows(session_id);

-- Итог проверки допусков (CrossingStatus, Src/Storage/Telemetry.h) — по
-- одной строке на сессию, пишется в конце теста (см. HistoryWriter::
-- writeCrossingResults(), MainWindow::endTest()), а не при её начале, как
-- valve_snapshots: до конца теста этих значений просто ещё нет. *_state —
-- как в CrossingStatus::State ('unknown' — проверка выключена/нет данных).
-- *_miss — NULL, если состояние не 'fail' (см. CrossingStatus::...Miss).
-- У пружины (spring_*) нет общего value/lower/upper/miss — два независимых
-- допуска (low/high), поэтому пара колонок на каждый, а не одна как у
-- остальных четырёх проверок.
CREATE TABLE crossing_results (
    session_id                      INTEGER PRIMARY KEY REFERENCES sessions(id),

    friction_state                  TEXT NOT NULL DEFAULT 'unknown'
                                       CHECK (friction_state IN ('unknown', 'ok', 'fail')),
    friction_value                  REAL,
    friction_lower                  REAL,
    friction_upper                  REAL,
    friction_miss                   REAL,

    valve_stroke_state              TEXT NOT NULL DEFAULT 'unknown'
                                       CHECK (valve_stroke_state IN ('unknown', 'ok', 'fail')),
    valve_stroke_value              REAL,
    valve_stroke_lower              REAL,
    valve_stroke_upper              REAL,
    valve_stroke_miss               REAL,

    dynamic_error_state             TEXT NOT NULL DEFAULT 'unknown'
                                       CHECK (dynamic_error_state IN ('unknown', 'ok', 'fail')),
    dynamic_error_value             REAL,
    dynamic_error_lower             REAL,
    dynamic_error_upper             REAL,
    dynamic_error_miss              REAL,

    linear_characteristic_state     TEXT NOT NULL DEFAULT 'unknown'
                                       CHECK (linear_characteristic_state IN ('unknown', 'ok', 'fail')),
    linear_characteristic_value     REAL,
    linear_characteristic_lower     REAL,
    linear_characteristic_upper     REAL,
    linear_characteristic_miss      REAL,

    spring_state                    TEXT NOT NULL DEFAULT 'unknown'
                                       CHECK (spring_state IN ('unknown', 'ok', 'fail')),
    spring_low_value                REAL,
    spring_low_lower                REAL,
    spring_low_upper                REAL,
    spring_low_miss                 REAL,
    spring_high_value               REAL,
    spring_high_lower               REAL,
    spring_high_upper               REAL,
    spring_high_miss                REAL
);

-- ============================== Заключение и ЗИП ========================
--
-- ВНИМАНИЕ: ключи ниже стабильны только после того, как ConclusionField
-- перестанет отдавать comboBox->currentText() и получит настоящий
-- dbKeyFor()-код ответа (сейчас это только у самого ПОЛЯ, не у выбранного
-- варианта). До этой правки колонки просто хранят текущий текст —
-- рабочее, но не гарантированно устойчивое к правкам .ui состояние.
CREATE TABLE conclusions (
    session_id                  INTEGER PRIMARY KEY REFERENCES sessions(id),
    seating_zone_key             TEXT,
    positioner_adaptation_key    TEXT,
    auxiliary_equipment_key      TEXT,
    friction_graph_key           TEXT,
    gland_leak_key                TEXT,
    summary_template_key         TEXT,
    summary_addendum             TEXT,   -- дописка оператора поверх шаблона
    additional_notes             TEXT
);

-- Внешний ключ на sessions, а не на conclusions: список ЗИП относится к
-- сессии как таковой, а не "принадлежит" именно записи заключения — так
-- порядок сохранения этих двух кусков tab_reportGeneration друг от друга
-- не зависит.
CREATE TABLE session_spare_parts (
    id            INTEGER PRIMARY KEY,
    session_id    INTEGER NOT NULL REFERENCES sessions(id),
    part_code     TEXT,
    is_free_text  INTEGER NOT NULL DEFAULT 0,
    free_text     TEXT
);

CREATE INDEX idx_spare_parts_session ON session_spare_parts(session_id);

-- ============================== Правки оператора ========================
--
-- Оператор иногда правит посчитанное значение руками перед печатью отчёта
-- (трение, динамическая ошибка, время хода — см. collectReportOverrides()
-- в ReportGenerationPresenter). Расхождение само по себе сигнал, поэтому
-- держим оба значения. Общая таблица вместо пары колонок в каждой
-- result-таблице — гибче и не раздувает схему ради редкого случая.
CREATE TABLE field_overrides (
    id               INTEGER PRIMARY KEY,
    session_id       INTEGER NOT NULL REFERENCES sessions(id),
    field_name       TEXT NOT NULL,
    computed_value   REAL,
    corrected_value  REAL,
    UNIQUE (session_id, field_name)
);
