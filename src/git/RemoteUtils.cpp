#include "RemoteUtils.h"

#include <QRegularExpression>
#include <QUrl>

QString RemoteUtils::normalizeRemoteUrl(const QString &url) {
    static const QRegularExpression ssh(R"(^git@([^:]+):(.+?)(\.git)?$)");
    const auto m = ssh.match(url.trimmed());
    if (m.hasMatch())
        return QString("https://%1/%2.git").arg(m.captured(1), m.captured(2));
    return url;
}

bool RemoteUtils::isSshUrl(const QString &url) {
    const QString u = url.trimmed();
    static const QRegularExpression scp(R"(^git@[^:]+:.+)");
    static const QRegularExpression ssh(R"(^ssh://git@[^/]+/.+)");
    return scp.match(u).hasMatch() || ssh.match(u).hasMatch();
}

bool RemoteUtils::isHttpsUrl(const QString &url) {
    return url.trimmed().startsWith("https://", Qt::CaseInsensitive);
}

QString RemoteUtils::stripHttpCredentials(const QString &rawUrl) {
    QUrl u(rawUrl.trimmed());
    if (u.scheme() != "https" && u.scheme() != "http")
        return rawUrl;
    if (u.userName().isEmpty() && u.password().isEmpty())
        return u.toString();
    u.setUserName({});
    u.setPassword({});
    return u.toString();
}

RemoteUtils::RemoteInfo RemoteUtils::parseRemoteUrl(const QString &rawUrl) {
    RemoteInfo info;
    info.rawUrl = rawUrl.trimmed();
    info.httpsUrl = normalizeRemoteUrl(info.rawUrl);
    const QUrl u(info.httpsUrl);
    if (!info.httpsUrl.isEmpty() && u.isValid())
        info.host = u.host();
    return info;
}

RemoteUtils::RepoSlug RemoteUtils::parseSlug(const QString &remoteUrl) {
    const QString url = remoteUrl.trimmed();
    RepoSlug slug;
    static const QRegularExpression scp(R"(^git@([^:]+):([^/]+)/(.+?)(?:\.git)?$)");
    if (auto m = scp.match(url); m.hasMatch()) {
        slug.host = m.captured(1);
        slug.owner = m.captured(2);
        slug.repo = m.captured(3);
        slug.valid = !slug.host.isEmpty() && !slug.owner.isEmpty() && !slug.repo.isEmpty();
        return slug;
    }
    if (url.startsWith("https://") || url.startsWith("http://")) {
        const QUrl u(url);
        if (u.isValid()) {
            slug.host = u.host();
            const QStringList parts =
                u.path().remove(QRegularExpression(R"(^/+)")).remove(QRegularExpression(R"(\.git$)")).split('/');
            if (parts.size() >= 2) {
                slug.owner = parts.at(0);
                slug.repo = parts.at(1);
                slug.valid = true;
            }
        }
    }
    return slug;
}

QString RemoteUtils::pullRequestUrl(const QString &remoteUrl, const QString &branch) {
    const RepoSlug slug = parseSlug(remoteUrl);
    if (!slug.valid)
        return {};
    const QString enc = QString::fromUtf8(QUrl::toPercentEncoding(branch));
    if (slug.host.contains("github.com"))
        return QString("https://github.com/%1/%2/compare/%3?expand=1")
            .arg(slug.owner, slug.repo, enc);
    if (slug.host.contains("bitbucket.org"))
        return QString("https://bitbucket.org/%1/%2/pull-requests/new?source=%3")
            .arg(slug.owner, slug.repo, enc);
    return {};
}

QString RemoteUtils::toProtocolUrl(const QString &inputUrl, const QString &protocol) {
    const QString value = inputUrl.trimmed();
    if (value.isEmpty())
        return {};
    static const QRegularExpression scp(R"(^git@([^:]+):(.+?)(\.git)?$)");
    static const QRegularExpression sshUrl(R"(^ssh://git@([^/]+)/(.+?)(\.git)?$)");
    if (protocol == "https") {
        if (auto m = scp.match(value); m.hasMatch())
            return QString("https://%1/%2.git").arg(m.captured(1), m.captured(2));
        if (auto m = sshUrl.match(value); m.hasMatch())
            return QString("https://%1/%2.git").arg(m.captured(1), m.captured(2));
        return value;
    }
    if (auto m = scp.match(value); m.hasMatch())
        return QString("git@%1:%2.git").arg(m.captured(1), m.captured(2));
    const QUrl parsed(value);
    if (parsed.isValid() && !parsed.host().isEmpty()) {
        QString repoPath = parsed.path();
        repoPath.remove(QRegularExpression(R"(^/+)"));
        repoPath.remove(QRegularExpression(R"(\.git$)"));
        if (!repoPath.isEmpty())
            return QString("git@%1:%2.git").arg(parsed.host(), repoPath);
    }
    return value;
}

QString RemoteUtils::repoNameFromUrl(const QString &url) {
    QString v = url.trimmed();
    while (v.endsWith('/'))
        v.chop(1);
    const int slash = qMax(v.lastIndexOf('/'), v.lastIndexOf(':'));
    QString name = slash >= 0 ? v.mid(slash + 1) : v;
    if (name.endsWith(".git"))
        name.chop(4);
    return name;
}
