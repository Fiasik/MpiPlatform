-- Справочник оборудования: вторая версия файла заказчика "Позиционеры,
-- приводы, модели клапанов" (2026-11-11). 005 уже применена на рабочих
-- базах, поэтому разница — отдельной миграцией.
--
-- INSERT OR IGNORE: на другом стенде то же самое могли уже ввести вручную
-- (source = 'user') — тогда запись остаётся как есть, без ошибки миграции.
-- sort_order с 1001: порядок важен только внутри списка одного
-- производителя, новые модели встают после уже имеющихся.

-- Новые производители. "VEТEC" из файла (с кириллической Т) — это уже
-- существующий Vetec, "НТ Вэлв" тоже уже был.
INSERT OR IGNORE INTO catalog_manufacturers(name, sort_order, source) VALUES
    ('Power Genix', 1001, 'builtin'),
    ('Диалкон', 1002, 'builtin');

-- Logix в новом файле записан с маркой: "Flowserve Logix 3200" и т.д.
-- Переименование = вставить новое имя с прежним sort_order + удалить старое
-- (UPDATE упал бы на ключе, если новое имя уже введено вручную).
INSERT OR IGNORE INTO catalog_models(kind, manufacturer, dynamic_error, name, sort_order, source)
    SELECT kind, manufacturer, dynamic_error, 'Flowserve ' || name, sort_order, source
    FROM catalog_models
    WHERE kind = 'positioner' AND manufacturer = 'Flowserve' AND source = 'builtin'
      AND name IN ('Logix 3200', 'Logix 3400', 'Logix 520MD', 'Logix 3800', 'Logix 4200');

DELETE FROM catalog_models
    WHERE kind = 'positioner' AND manufacturer = 'Flowserve' AND source = 'builtin'
      AND name IN ('Logix 3200', 'Logix 3400', 'Logix 520MD', 'Logix 3800', 'Logix 4200');

INSERT OR IGNORE INTO catalog_models(kind, manufacturer, dynamic_error, name, sort_order, source) VALUES
    ('positioner', 'MetsoNelles', '1.5', 'Metso ND500 (A9Systems)', 1001, 'builtin'),
    ('positioner', 'MetsoNelles', '1.5', 'Metso ND6000 (A9Systems)', 1002, 'builtin'),
    ('positioner', 'MetsoNelles', '1.5', 'Metso ND7000 (A9Systems)', 1003, 'builtin'),
    ('positioner', 'Power Genix', '1.5', 'Power Genix ASD-5000', 1004, 'builtin'),
    ('positioner', 'Power Genix', '1.5', 'Power Genix ASD-7000', 1005, 'builtin'),
    ('positioner', 'Power Genix', '1.5', 'Power Genix SS5', 1006, 'builtin'),
    ('positioner', 'Power Genix', '2.5', 'Power Genix EPR', 1007, 'builtin'),
    ('positioner', 'Power Genix', '2.5', 'Power Genix EPL', 1008, 'builtin'),
    ('positioner', 'Power Genix', '2.5', 'Power Genix NEPR', 1009, 'builtin'),
    ('positioner', 'Power Genix', '2.5', 'Power Genix SER', 1010, 'builtin'),
    ('drive', 'НТ Вэлв', '', 'НТ Вэлв КМП-Л2-НЗ (70 мм)', 1011, 'builtin'),
    ('drive', 'НТ Вэлв', '', 'НТ Вэлв КМП-Л3-НЗ (100 мм)', 1012, 'builtin'),
    ('drive', 'НТ Вэлв', '', 'НТ Вэлв КМП-Л4-НЗ (125 мм)', 1013, 'builtin'),
    ('drive', 'НТ Вэлв', '', 'НТ Вэлв КМП-Л5-НЗ (165 мм)', 1014, 'builtin'),
    ('drive', 'НТ Вэлв', '', 'НТ Вэлв КМП-Л6-НЗ (205 мм)', 1015, 'builtin'),
    ('drive', 'НТ Вэлв', '', 'НТ Вэлв КМП-Л2-НО (70 мм)', 1016, 'builtin'),
    ('drive', 'НТ Вэлв', '', 'НТ Вэлв КМП-Л3-НО (100 мм)', 1017, 'builtin'),
    ('drive', 'НТ Вэлв', '', 'НТ Вэлв КМП-Л4-НО (125 мм)', 1018, 'builtin'),
    ('drive', 'НТ Вэлв', '', 'НТ Вэлв КМП-Л5-НО (165 мм)', 1019, 'builtin'),
    ('drive', 'НТ Вэлв', '', 'НТ Вэлв КМП-Л6-НО (205 мм)', 1020, 'builtin');
