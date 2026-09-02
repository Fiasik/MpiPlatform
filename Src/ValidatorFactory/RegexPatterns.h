#pragma once

#include <QRegularExpression>

namespace RegexPatterns {
// только цифры 0–9
inline const QRegularExpression& digits() {
    static const QRegularExpression re(QStringLiteral("^[0-9]*$"));
    return re;
}

// цифры с точкой (положительное число)
inline const QRegularExpression& digitsDot() {
    static const QRegularExpression re(QStringLiteral("^[0-9]+(\\.[0-9]+)?$"));
    return re;
}

// цифры и дефис
inline const QRegularExpression& digitsHyphens() {
    static const QRegularExpression re(QStringLiteral("^[0-9-]*$"));
    return re;
}

// буквы рус и латин
inline const QRegularExpression& lettersHyphens() {
    static const QRegularExpression re(QStringLiteral("^[A-Za-zА-Яа-яЁё]*$"));
    return re;
}

// запрет символов
inline const QRegularExpression& noSpecialChars() {
    static const QRegularExpression re(QStringLiteral("^[^@!^\\/\\?\\*\\:\\;\\{\\}\\\\]*$"));
    return re;
}

// число с плавающей точкой (с возможным знаком): -12, 3.14, 0.5
inline const QRegularExpression& floatNumber() {
    static const QRegularExpression re(
        QStringLiteral("^-?[0-9]+(\\.[0-9]+)?$")
        );
    return re;
}

// число с плавающей точкой (с возможным знаком): -12-10.5, 3.14, 0.5
inline const QRegularExpression& floatRange() {
    static const QRegularExpression re(
        QStringLiteral("^-?[0-9]+(\\.[0-9]*)?(\\s*-\\s*-?[0-9]+(\\.[0-9]*)?)?$")
        );
    return re;
}

// число с плавающей точкой: 12-55.5-36.5-60
inline const QRegularExpression& floatSequence() {
    static const QRegularExpression re(
        QStringLiteral(R"(^(?:\d+(?:\.\d+)?)(?:-\d+(?:\.\d+)?)*$)")
        );
    return re;
}
}
