#pragma once

#include <QString>

namespace Report::Html {

// Единственное место, где перечислены "к чему можно прикрепить
// комментарий" и их подписи — раньше это был QString-ключ, продублированный
// отдельно в месте подключения кнопки и отдельно в сборке HTML; здесь
// это один enum и одна функция подписи на него.
enum class CommentSlot {
    Manual,
    MainTest,
    ResponseTest,
    ResolutionTest,
    StepTest,
};

QString titleFor(CommentSlot slot);

}
