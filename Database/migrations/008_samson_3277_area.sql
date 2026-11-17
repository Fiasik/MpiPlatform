-- У Samson 3277 в скобках площадь мембраны, а не размер: в файле заказчика
-- было "(120 мм)", правильно "(120 см²)" (уточнение заказчика, 2026-11-11).
-- Переименование = вставить новое имя с прежним sort_order + удалить старое
-- (как в 006/007), чтобы ручная запись с тем же именем не уронила миграцию.
-- Снимки карточек в истории (valve_snapshots) не трогаем — это данные на
-- момент теста.

INSERT OR IGNORE INTO catalog_models(kind, manufacturer, dynamic_error, name, sort_order, source)
    SELECT kind, manufacturer, dynamic_error, replace(name, ' мм)', ' см²)'), sort_order, source
    FROM catalog_models
    WHERE kind = 'drive' AND manufacturer = 'Samson' AND source = 'builtin'
      AND name IN ('Samson 3277 (120 мм)', 'Samson 3277 (240 мм)', 'Samson 3277 (350 мм)',
                   'Samson 3277 (700 мм)', 'Samson 3277 (1400 мм)', 'Samson 3277 (2800 мм)');

DELETE FROM catalog_models
    WHERE kind = 'drive' AND manufacturer = 'Samson' AND source = 'builtin'
      AND name IN ('Samson 3277 (120 мм)', 'Samson 3277 (240 мм)', 'Samson 3277 (350 мм)',
                   'Samson 3277 (700 мм)', 'Samson 3277 (1400 мм)', 'Samson 3277 (2800 мм)');
