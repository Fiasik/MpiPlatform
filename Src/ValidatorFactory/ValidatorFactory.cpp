#include "ValidatorFactory.h"
#include "RegexPatterns.h"

#include <QRegularExpressionValidator>

QValidator* ValidatorFactory::create(Type type, QObject* parent)
{
    switch (type) {
    case Type::Percent0to100: {
        auto* v = new QDoubleValidator(0.0, 100.0, 1, parent);
        v->setNotation(QDoubleValidator::StandardNotation);
        return v;
    }
    case Type::FloatNumber: {
        auto* v = new QDoubleValidator(parent);
        v->setNotation(QDoubleValidator::StandardNotation);
        return v;
    }
    // остальные — как у тебя, через RegexPatterns:
    case Type::Digits:
        return new QRegularExpressionValidator(RegexPatterns::digits(), parent);
    case Type::DigitsDot:
        return new QRegularExpressionValidator(RegexPatterns::digitsDot(), parent);
    case Type::DigitsHyphens:
        return new QRegularExpressionValidator(RegexPatterns::digitsHyphens(), parent);
    case Type::LettersHyphens:
        return new QRegularExpressionValidator(RegexPatterns::lettersHyphens(), parent);
    case Type::NoSpecialChars:
        return new QRegularExpressionValidator(RegexPatterns::noSpecialChars(), parent);
    case Type::FloatRange:
        return new QRegularExpressionValidator(RegexPatterns::floatRange(), parent);
    case Type::FloatSequence:
        return new QRegularExpressionValidator(RegexPatterns::floatSequence(), parent);
    }

    // fallback
    return new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^.*$")), parent);
}
