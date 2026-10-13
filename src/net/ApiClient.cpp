#include "ApiClient.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr int kTimeoutMs = 30000;
const char kUserAgent[] = "OpenGit";
} // namespace

ApiClient::ApiClient(QObject *parent) : QObject(parent) {}

ApiClient::Response ApiClient::get(const QUrl &url,
                                   const QList<QPair<QString, QString>> &headers) {
    Response out;
    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", kUserAgent);
    for (const auto &[name, value] : headers)
        req.setRawHeader(name.toLatin1(), value.toLatin1());
    QNetworkReply *reply = mgr.get(req);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(kTimeoutMs);
    loop.exec();
    if (!reply->isFinished()) {
        out.error = "Request timed out";
        reply->abort();
        reply->deleteLater();
        return out;
    }
    out.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    out.json = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    return out;
}

ApiClient::Response ApiClient::postForm(const QUrl &url, const QString &body,
                                        const QString &authHeader) {
    Response out;
    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    if (!authHeader.isEmpty())
        req.setRawHeader("Authorization", authHeader.toLatin1());
    QNetworkReply *reply = mgr.post(req, body.toLatin1());
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(kTimeoutMs);
    loop.exec();
    if (!reply->isFinished()) {
        out.error = "Request timed out";
        reply->abort();
        reply->deleteLater();
        return out;
    }
    out.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    out.json = QJsonDocument::fromJson(reply->readAll());
    if (reply->error() != QNetworkReply::NoError && out.json.isNull())
        out.error = reply->errorString();
    reply->deleteLater();
    return out;
}

ApiClient::Validation ApiClient::validateGithub(const QString &token) {
    if (token.isEmpty())
        return {Status::Missing, "No token saved for this host/account."};
    const Response r = get(QUrl("https://api.github.com/user"),
                           {{"Accept", "application/vnd.github+json"},
                            {"Authorization", "token " + token}});
    if (!r.error.isEmpty())
        return {Status::Error, r.error};
    if (r.status == 401)
        return {Status::Invalid, "GitHub token is not valid."};
    if (r.status == 403)
        return {Status::Error, "GitHub API access denied (rate limited or forbidden)."};
    if (r.status < 200 || r.status >= 300)
        return {Status::Error, QString("GitHub API error: %1").arg(r.status)};
    return {Status::Valid, {}};
}

ApiClient::Validation ApiClient::validateBitbucket(const QString &token) {
    if (token.isEmpty())
        return {Status::Missing, "No token saved for this host/account."};
    const Response r = get(QUrl("https://api.bitbucket.org/2.0/user"),
                           {{"Authorization", "Bearer " + token}});
    if (!r.error.isEmpty())
        return {Status::Error, r.error};
    if (r.status == 401)
        return {Status::Invalid, "Bitbucket token is not valid."};
    if (r.status == 403)
        return {Status::Error,
                "Bitbucket API access denied (rate limited or forbidden)."};
    if (r.status < 200 || r.status >= 300)
        return {Status::Error, QString("Bitbucket API error: %1").arg(r.status)};
    return {Status::Valid, {}};
}

ApiClient::Validation ApiClient::validateGitlab(const QString &token) {
    if (token.isEmpty())
        return {Status::Missing, "No token saved for this host/account."};
    const Response r = get(QUrl("https://gitlab.com/api/v4/user"),
                           {{"Authorization", "Bearer " + token}});
    if (!r.error.isEmpty())
        return {Status::Error, r.error};
    if (r.status == 401)
        return {Status::Invalid, "GitLab token is not valid."};
    if (r.status == 403)
        return {Status::Error, "GitLab API access denied (rate limited or forbidden)."};
    if (r.status < 200 || r.status >= 300)
        return {Status::Error, QString("GitLab API error: %1").arg(r.status)};
    return {Status::Valid, {}};
}

ApiClient::BitbucketTokens ApiClient::refreshBitbucket(const QString &clientId,
                                                       const QString &clientSecret,
                                                       const QString &refreshToken) {
    if (clientId.isEmpty() || clientSecret.isEmpty())
        throw std::runtime_error(
            "Bitbucket client credentials missing for refresh. Ensure BITBUCKET_CLIENT_ID and CLIENT_BITBUCKET_SECRET are set.");
    QUrlQuery form;
    form.addQueryItem("grant_type", "refresh_token");
    form.addQueryItem("refresh_token", refreshToken);
    const QString basic = "Basic "
        + QString::fromLatin1((clientId + ":" + clientSecret).toLatin1().toBase64());
    const Response r = postForm(QUrl("https://bitbucket.org/site/oauth2/access_token"),
                                form.toString(QUrl::FullyEncoded), basic);
    const QJsonObject json = r.json.object();
    const QString access = json.value("access_token").toString();
    if (access.isEmpty()) {
        const QString msg = json.value("error_description").toString(
            json.value("error").toString("Bitbucket refresh failed"));
        throw std::runtime_error(msg.toStdString());
    }
    BitbucketTokens t;
    t.accessToken = access;
    t.refreshToken = json.value("refresh_token").toString(refreshToken);
    t.expiresAt = QDateTime::currentMSecsSinceEpoch()
        + static_cast<qint64>(qMax(0.0, json.value("expires_in").toDouble(7200) - 60) * 1000);
    return t;
}

QString ApiClient::ensureGithubToken(const QString &token) {
    if (token.isEmpty())
        return {};
    const Validation v = validateGithub(token);
    if (v.status == Status::Invalid)
        throw std::runtime_error("GitHub token is not valid. Please sign in again.");
    if (v.status == Status::Error)
        throw std::runtime_error(v.error.toStdString());
    return token;
}

QString ApiClient::ensureGitlabToken(const QString &token) {
    if (token.isEmpty())
        return {};
    const Validation v = validateGitlab(token);
    if (v.status == Status::Invalid)
        throw std::runtime_error("GitLab token is not valid. Please sign in again.");
    if (v.status == Status::Error)
        throw std::runtime_error(v.error.toStdString());
    return token;
}

QString ApiClient::ensureBitbucketToken(const QString &token, const QString &refreshToken,
                                        qint64 expiresAt, const QString &clientId,
                                        const QString &clientSecret,
                                        std::function<void(const BitbucketTokens &)> onRefreshed) {
    if (token.isEmpty())
        throw std::runtime_error("No token saved for this host/account.");
    const bool canRefresh = !refreshToken.isEmpty() && !clientId.isEmpty() && !clientSecret.isEmpty();
    QString current = token;

    auto tryRefresh = [&] {
        const BitbucketTokens t = refreshBitbucket(clientId, clientSecret, refreshToken);
        if (onRefreshed)
            onRefreshed(t);
        current = t.accessToken;
    };

    if (expiresAt > 0 && QDateTime::currentMSecsSinceEpoch() >= expiresAt - 60000 && canRefresh)
        tryRefresh();

    Validation first = validateBitbucket(current);
    if (first.status == Status::Valid)
        return current;
    if (first.status == Status::Error && first.error.contains("403"))
        return current; // 403 = token works but lacks user scope; still usable for git
    if (first.status == Status::Invalid && canRefresh) {
        tryRefresh();
        const Validation second = validateBitbucket(current);
        if (second.status == Status::Valid)
            return current;
        if (second.status == Status::Error && second.error.contains("403"))
            return current;
        if (second.status == Status::Invalid)
            throw std::runtime_error("Bitbucket token is not valid. Please sign in again.");
        throw std::runtime_error(second.error.toStdString());
    }
    if (first.status == Status::Invalid)
        throw std::runtime_error("Bitbucket token is not valid. Please sign in again.");
    throw std::runtime_error(first.error.toStdString());
}

QList<ApiClient::RepoItem> ApiClient::searchRepos(
    const QString &host, const QString &token, const QString &refreshToken,
    qint64 expiresAt, const QString &query, const QString &clientId,
    const QString &clientSecret,
    std::function<void(const BitbucketTokens &)> onRefreshed) {
    if (host == "github.com")
        return searchGithub(ensureGithubToken(token), query);
    if (host == "bitbucket.org")
        return searchBitbucket(ensureBitbucketToken(token, refreshToken, expiresAt, clientId,
                                                    clientSecret, onRefreshed),
                               query);
    if (host == "gitlab.com")
        return searchGitlab(ensureGitlabToken(token), query);
    throw std::runtime_error(("Unsupported host: " + host).toStdString());
}

QList<ApiClient::RepoItem> ApiClient::searchGithub(const QString &token, const QString &query) {
    const Response r = get(QUrl("https://api.github.com/user/repos?per_page=100&sort=updated&direction=desc"),
                           {{"Accept", "application/vnd.github+json"},
                            {"Authorization", "token " + token}});
    if (r.status < 200 || r.status >= 300)
        throw std::runtime_error(QString("GitHub API error: %1").arg(r.status).toStdString());
    QList<RepoItem> items;
    const QString q = query.toLower();
    for (const QJsonValue &v : r.json.array()) {
        const QJsonObject o = v.toObject();
        RepoItem item{o.value("name").toString(), o.value("full_name").toString(),
                      o.value("html_url").toString(), o.value("clone_url").toString(),
                      "github"};
        if (!q.isEmpty() && !item.name.toLower().contains(q)
            && !item.fullName.toLower().contains(q))
            continue;
        items << item;
    }
    return items;
}

QList<ApiClient::RepoItem> ApiClient::searchBitbucket(const QString &token, const QString &query) {
    const QList<QPair<QString, QString>> headers = {{"Authorization", "Bearer " + token}};
    const Response ws = get(QUrl("https://api.bitbucket.org/2.0/user/workspaces?pagelen=100"), headers);
    if (ws.status == 403)
        throw std::runtime_error(
            "Bitbucket needs the \"account\" scope to list repositories. Enable the Account permission on the Bitbucket OAuth consumer, then sign out and sign in again.");
    if (ws.status < 200 || ws.status >= 300)
        throw std::runtime_error(QString("Bitbucket API error: %1").arg(ws.status).toStdString());
    QStringList slugs;
    for (const QJsonValue &v : ws.json.object().value("values").toArray()) {
        const QJsonObject o = v.toObject();
        const QString slug = o.value("slug").toString(o.value("workspace").toObject().value("slug").toString());
        if (!slug.isEmpty())
            slugs << slug;
    }
    const QString q = query.toLower();
    QString qParam;
    if (!q.isEmpty())
        qParam = "&q=name~%22" + QString::fromUtf8(QUrl::toPercentEncoding(query)) + "%22";
    QList<RepoItem> items;
    QSet<QString> seen;
    for (const QString &slug : slugs) {
        const Response r = get(
            QUrl(QString("https://api.bitbucket.org/2.0/repositories/%1?pagelen=50%2")
                     .arg(QString::fromUtf8(QUrl::toPercentEncoding(slug)), qParam)),
            headers);
        if (r.status < 200 || r.status >= 300)
            continue;
        for (const QJsonValue &v : r.json.object().value("values").toArray()) {
            const QJsonObject o = v.toObject();
            QString httpsClone;
            for (const QJsonValue &c : o.value("links").toObject().value("clone").toArray()) {
                const QJsonObject co = c.toObject();
                if (co.value("name").toString() == "https") {
                    httpsClone = co.value("href").toString();
                    break;
                }
            }
            if (httpsClone.isEmpty())
                continue;
            RepoItem item{o.value("name").toString(),
                          o.value("full_name").toString(o.value("slug").toString()),
                          o.value("links").toObject().value("html").toObject().value("href").toString(),
                          httpsClone, "bitbucket"};
            if (!q.isEmpty() && !item.name.toLower().contains(q)
                && !item.fullName.toLower().contains(q))
                continue;
            if (seen.contains(item.fullName))
                continue;
            seen.insert(item.fullName);
            items << item;
        }
    }
    return items;
}

QList<ApiClient::RepoItem> ApiClient::searchGitlab(const QString &token, const QString &query) {
    QUrl url("https://gitlab.com/api/v4/projects?membership=true&simple=true&order_by=last_activity_at&sort=desc&per_page=100");
    if (!query.isEmpty())
        url.setQuery(url.query() + "&search=" + QString::fromUtf8(QUrl::toPercentEncoding(query)));
    const Response r = get(url, {{"Authorization", "Bearer " + token}});
    if (r.status < 200 || r.status >= 300)
        throw std::runtime_error(QString("GitLab API error: %1").arg(r.status).toStdString());
    QList<RepoItem> items;
    for (const QJsonValue &v : r.json.array()) {
        const QJsonObject o = v.toObject();
        RepoItem item{o.value("name").toString(), o.value("path_with_namespace").toString(),
                      o.value("web_url").toString(), o.value("http_url_to_repo").toString(),
                      "gitlab"};
        if (!item.cloneUrl.isEmpty())
            items << item;
    }
    return items;
}
