-- Карточка клапана: шкала позиционера и рекомендованное значение
-- динамической ошибки раньше в БД не попадали вообще (при повторном открытии
-- окна клапана они сбрасывались). Без CHECK — перечисления в этой БД
-- намеренно не ограничиваем: SQLite не умеет менять CHECK без пересборки
-- таблицы, а новые значения enum'ов появляются чаще, чем хочется её
-- пересобирать.
ALTER TABLE valve_snapshots ADD COLUMN positioner_scale TEXT NOT NULL DEFAULT 'normal';
ALTER TABLE valve_snapshots ADD COLUMN dynamic_error_recommended TEXT;

-- Какие проверки допусков были ВКЛЮЧЕНЫ на момент расчёта результата.
-- Снимок карточки (valve_snapshots) оператор может поправить после теста
-- ("Назад"), а результат при этом остаётся посчитанным по прежнему выбору,
-- поэтому флаги дублируются в самих результатах. NULL — строка старая,
-- до этой миграции, выбор на момент расчёта не известен.
ALTER TABLE crossing_results ADD COLUMN friction_enabled INTEGER;
ALTER TABLE crossing_results ADD COLUMN valve_stroke_enabled INTEGER;
ALTER TABLE crossing_results ADD COLUMN dynamic_error_enabled INTEGER;
ALTER TABLE crossing_results ADD COLUMN linear_characteristic_enabled INTEGER;
ALTER TABLE crossing_results ADD COLUMN spring_enabled INTEGER;
