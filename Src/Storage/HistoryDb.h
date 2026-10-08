#pragma once

#include <QString>
#include <QSqlDatabase>
#include <functional>

// Общая инфраструктура доступа к Database/mpiplatform.db — раньше
// databasePath() и обвязка открытия соединения были продублированы по
// HistoryWriter/HistoryReader/Gui::HistoryPresenter независимо (один в
// один, вплоть до комментариев).
namespace HistoryDb {

// Путь до файла на диске дерева исходников, а не в build-директории. Путь
// для production ещё предстоит решить отдельно (это не production-код).
QString databasePath();

// Для одноразовых запросов (HistoryWriter/HistoryReader: открыть — сделать
// один запрос или транзакцию — закрыть). connectionName должен быть
// уникальным на вызов (как раньше) — withConnection сам гарантирует
// db.close() + QSqlDatabase::removeDatabase() даже если fn() кинет/рано
// вернётся по пути исключения не предполагается, но close+remove всегда
// парны открытию. Возвращает false, если соединение не открылось (fn() в
// этом случае не вызывается); иначе — true (сам факт открытия, не успех
// запроса внутри fn — тот результат caller забирает через захваченную
// по ссылке переменную, как уже делает HistoryReader).
bool withConnection(const QString& connectionName, const std::function<void(QSqlDatabase&)>& fn);

// Для долгоживущего соединения, переиспользуемого много раз за время жизни
// объекта (Gui::HistoryPresenter — один connectionName на весь презентер,
// между вызовами не закрывается). Не открывает и не закрывает db — только
// находит/заводит именованное соединение и выставляет databaseName; сам
// db.open()/db.close() остаются на вызывающей стороне, как и раньше.
QSqlDatabase openPersistent(const QString& connectionName);

// Пара к openPersistent() — используется в деструкторе владельца
// долгоживущего соединения.
void removePersistent(const QString& connectionName);

}
