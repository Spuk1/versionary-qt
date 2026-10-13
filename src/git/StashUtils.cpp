#include "StashUtils.h"

#include <QRegularExpression>

int StashUtils::toRefIdx(const QString &input, int fallback) {
    const QString s = input.trimmed();
    static const QRegularExpression braced(R"(^(?:refs/stash@|stash@)\{\s*(\d+)\s*\}$)",
                                           QRegularExpression::CaseInsensitiveOption);
    if (auto m = braced.match(s); m.hasMatch())
        return m.captured(1).toInt();
    static const QRegularExpression plain(R"(^\d+$)");
    if (plain.match(s).hasMatch())
        return s.toInt();
    static const QRegularExpression alias(R"(^(latest|head|top)$)",
                                          QRegularExpression::CaseInsensitiveOption);
    if (alias.match(s).hasMatch())
        return 0;
    return fallback;
}
