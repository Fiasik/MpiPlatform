-- Источник сессии 'legacy' — карточки, накопленные до перехода на БД
-- (хранились в QSettings/реестре Windows), разово переносятся как сессии без
-- тестов. CHECK (source IN ('live','harvest')) такое значение не пропускает,
-- а SQLite не умеет менять CHECK без пересборки таблицы — пересобираем
-- sessions и заодно CHECK на source не возвращаем (перечисления здесь без
-- CHECK, см. 002). id копируются как есть, поэтому внешние ключи остальных
-- таблиц (valve_snapshots, crossing_results, ...) остаются верными: ссылки
-- в SQLite идут по имени таблицы, а не по её внутреннему устройству.
CREATE TABLE sessions_new (
    id              INTEGER PRIMARY KEY,
    position_id     INTEGER NOT NULL REFERENCES positions(id),
    folder_path     TEXT,
    ts              TEXT NOT NULL,
    variant         TEXT,
    pattern         TEXT,
    operator        TEXT,
    app_version     TEXT,
    schema_version  INTEGER NOT NULL,
    source          TEXT NOT NULL,
    needs_repair    INTEGER NOT NULL DEFAULT 0
);

INSERT INTO sessions_new(id, position_id, folder_path, ts, variant, pattern, operator,
                         app_version, schema_version, source, needs_repair)
    SELECT id, position_id, folder_path, ts, variant, pattern, operator,
           app_version, schema_version, source, needs_repair
    FROM sessions;

DROP TABLE sessions;
ALTER TABLE sessions_new RENAME TO sessions;
CREATE INDEX idx_sessions_position ON sessions(position_id);

-- Служебные флаги приложения (например, "перенос старых карточек уже
-- выполнен"), чтобы не хранить их ни в QSettings, ни отдельным файлом.
CREATE TABLE app_state (
    key    TEXT PRIMARY KEY,
    value  TEXT NOT NULL
);
