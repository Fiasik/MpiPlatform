#include "CommentCard.h"

namespace Report::Html {

QString titleFor(CommentSlot slot)
{
    switch (slot) {
    case CommentSlot::Manual:         return QStringLiteral("Ручное управление");
    case CommentSlot::MainTest:       return QStringLiteral("Основной тест");
    case CommentSlot::ResponseTest:   return QStringLiteral("Тест на чувствительность реагирования");
    case CommentSlot::ResolutionTest: return QStringLiteral("Тест на разрешающую способность");
    case CommentSlot::StepTest:       return QStringLiteral("Тест шаговой реакции");
    }
    return {};
}

}
