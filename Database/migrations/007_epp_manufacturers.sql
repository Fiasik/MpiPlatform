-- ЭПП-300 и ЭПП-1 в 005 были моделями без производителя (видны при любом
-- производителе ЗРА). Уточнение заказчика (2026-11-11): ЭПП-300 — это
-- РУСТ-95, в списке "РУСТ ЭПП-300"; ЭПП-1 — производитель СПЗ, в списке
-- "СПЗ ЭПП-1". После этого моделей без производителя в справочнике нет.
--
-- Перенос = вставить новую запись с прежним sort_order + удалить старую
-- (как с Logix в 006): если такое имя уже ввели вручную, INSERT OR IGNORE
-- его не тронет и миграция не упадёт.

INSERT OR IGNORE INTO catalog_manufacturers(name, sort_order, source) VALUES
    ('СПЗ', 1003, 'builtin');

INSERT OR IGNORE INTO catalog_models(kind, manufacturer, dynamic_error, name, sort_order, source)
    SELECT kind, 'РУСТ-95', dynamic_error, 'РУСТ ЭПП-300', sort_order, source
    FROM catalog_models
    WHERE kind = 'positioner' AND manufacturer = '' AND name = 'ЭПП-300' AND source = 'builtin';

INSERT OR IGNORE INTO catalog_models(kind, manufacturer, dynamic_error, name, sort_order, source)
    SELECT kind, 'СПЗ', dynamic_error, 'СПЗ ЭПП-1', sort_order, source
    FROM catalog_models
    WHERE kind = 'positioner' AND manufacturer = '' AND name = 'ЭПП-1' AND source = 'builtin';

DELETE FROM catalog_models
    WHERE kind = 'positioner' AND manufacturer = '' AND source = 'builtin'
      AND name IN ('ЭПП-300', 'ЭПП-1');
