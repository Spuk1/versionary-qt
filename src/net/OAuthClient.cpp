#include "OAuthClient.h"
#include "core/AppConfig.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

OAuthClient::OAuthClient(QObject *parent) : QObject(parent) {}

QString OAuthClient::randomBase64Url(int bytes) {
    QByteArray buf(bytes, 0);
    for (int i = 0; i < bytes; ++i)
        buf[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
    return QString::fromLatin1(buf.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString OAuthClient::pkceChallenge(const QString &verifier) {
    const QByteArray digest =
        QCryptographicHash::hash(verifier.toLatin1(), QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

OAuthClient::CodeResult OAuthClient::authorize(const QUrl &authUrl) {
    CodeResult out;
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, kLoopbackPort)) {
        out.error = "OAuth loopback listener failed (is port 6969 busy?): "
            + server.errorString();
        return out;
    }
    QDesktopServices::openUrl(authUrl);

    QEventLoop loop;
    bool done = false;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        if (!done) {
            done = true;
            loop.quit();
        }
    });

    QMap<QTcpSocket *, QByteArray> pending;
    QTcpSocket *captured = nullptr;
    QObject::connect(&server, &QTcpServer::newConnection, &loop, [&] {
        QTcpSocket *socket = server.nextPendingConnection();
        if (!socket)
            return;
        pending.insert(socket, {});
        QObject::connect(socket, &QTcpSocket::disconnected, &loop, [&, socket] {
            const bool wasCaptured = (socket == captured);
            pending.remove(socket);
            socket->deleteLater();
            // Quit once the browser confirmed receipt (graceful close
            // handshake done). The timeout is the backstop.
            if (wasCaptured && !done) {
                done = true;
                loop.quit();
            }
        });
        QObject::connect(socket, &QTcpSocket::readyRead, &loop, [&, socket] {
            if (done) {
                socket->disconnectFromHost();
                return;
            }
            pending[socket] += socket->readAll();
            const CallbackParse parsed = parseCallbackRequest(pending[socket]);
            if (!parsed.complete)
                return; // fragmented request: wait for full headers
            pending.remove(socket);
            const bool hasResult = !parsed.code.isEmpty() || !parsed.error.isEmpty();
            if (hasResult && out.code.isEmpty() && out.error.isEmpty()) {
                out.code = parsed.code;
                out.state = parsed.state;
                out.error = parsed.error;
                captured = socket;
            }
            socket->write(buildCallbackResponse(parsed));
            socket->flush();
            socket->waitForBytesWritten(3000);
            socket->disconnectFromHost();
        });
    });
    timeout.start(300000);
    loop.exec();
    return out;
}

OAuthClient::CallbackParse OAuthClient::parseCallbackRequest(const QByteArray &raw) {
    CallbackParse parsed;
    if (raw.indexOf("\r\n\r\n") < 0)
        return parsed; // headers incomplete
    parsed.complete = true;
    const QString requestLine = QString::fromUtf8(raw.left(raw.indexOf("\r\n")));
    const QString path = requestLine.split(' ').value(1);
    if (path.isEmpty() || !path.startsWith("/oauth/callback"))
        return parsed; // e.g. favicon: complete, but no result
    const QUrl url("http://127.0.0.1" + path);
    const QUrlQuery q(url);
    parsed.code = q.queryItemValue("code");
    parsed.state = q.queryItemValue("state");
    parsed.error = q.queryItemValue("error");
    if (parsed.error.isEmpty() && q.hasQueryItem("error_description"))
        parsed.error = q.queryItemValue("error_description");
    return parsed;
}

namespace {
QByteArray htmlEscape(const QString &s) {
    QString e = s;
    e.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
    return e.toUtf8();
}
} // namespace

QByteArray OAuthClient::buildCallbackResponse(const CallbackParse &parsed) {
    const bool ok = !parsed.code.isEmpty() && parsed.error.isEmpty();
    QByteArray body;
    if (ok) {
        body = "<html><body style=\"font-family:sans-serif\">"
               "Authentication complete. You can close this window.</body></html>";
    } else if (!parsed.error.isEmpty()) {
        body = "<html><body style=\"font-family:sans-serif\">Authentication failed: "
            + htmlEscape(parsed.error) + ". You can close this window.</body></html>";
    } else {
        body = "<html><body style=\"font-family:sans-serif\">"
               "Authentication failed. You can close this window.</body></html>";
    }
    const QByteArray head =
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "
        + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n";
    return head + body;
}

QJsonObject OAuthClient::postJson(const QUrl &url, const QJsonObject &body, QString *error) {
    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    QNetworkReply *reply = mgr.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(60000);
    loop.exec();
    if (reply->error() != QNetworkReply::NoError && !reply->isFinished()) {
        if (error)
            *error = reply->errorString();
        reply->deleteLater();
        return {};
    }
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    return QJsonDocument::fromJson(data).object();
}

QJsonObject OAuthClient::postForm(const QUrl &url, const QString &body,
                                  const QString &authHeader, QString *error) {
    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");
    if (!authHeader.isEmpty())
        req.setRawHeader("Authorization", authHeader.toLatin1());
    QNetworkReply *reply = mgr.post(req, body.toLatin1());
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(60000);
    loop.exec();
    const QByteArray data = reply->readAll();
    const bool netFail = reply->error() != QNetworkReply::NoError && data.isEmpty();
    if (netFail && error)
        *error = reply->errorString();
    reply->deleteLater();
    if (netFail)
        return {};
    return QJsonDocument::fromJson(data).object();
}

namespace {
QUrl buildAuthUrl(const QString &base, const QString &clientId, const QString &redirect,
                  const QString &state, const QString &challenge, const QString &scope,
                  bool withResponseTypeCode = false) {
    QUrl url(base);
    QUrlQuery q;
    q.addQueryItem("client_id", clientId);
    if (withResponseTypeCode)
        q.addQueryItem("response_type", "code");
    q.addQueryItem("redirect_uri", redirect);
    if (!scope.isEmpty())
        q.addQueryItem("scope", scope);
    q.addQueryItem("state", state);
    q.addQueryItem("code_challenge", challenge);
    q.addQueryItem("code_challenge_method", "S256");
    url.setQuery(q);
    return url;
}

QString redirectUri() {
    return QString("http://127.0.0.1:%1/oauth/callback").arg(OAuthClient::kLoopbackPort);
}
} // namespace

OAuthClient::TokenResult OAuthClient::oauthGithub(const QString &clientId,
                                                 const QString &clientSecret) {
    const QString state = randomBase64Url(16);
    const QString verifier = randomBase64Url(32);
    const QUrl authUrl = buildAuthUrl("https://github.com/login/oauth/authorize", clientId,
                                      redirectUri(), state, pkceChallenge(verifier),
                                      "repo workflow");
    const CodeResult cb = authorize(authUrl);
    if (!cb.error.isEmpty() || cb.code.isEmpty() || cb.state != state)
        throw std::runtime_error(
            ("OAuth: " + (!cb.error.isEmpty() ? cb.error : QString("code/state mismatch")))
                .toStdString());
    QJsonObject body;
    body.insert("client_id", clientId);
    body.insert("grant_type", "authorization_code");
    if (!clientSecret.isEmpty())
        body.insert("client_secret", clientSecret);
    body.insert("code", cb.code);
    body.insert("redirect_uri", redirectUri());
    body.insert("code_verifier", verifier);
    QString err;
    const QJsonObject json =
        postJson(QUrl("https://github.com/login/oauth/access_token"), body, &err);
    const QString token = json.value("access_token").toString();
    if (token.isEmpty())
        throw std::runtime_error(
            (json.value("error_description").toString("GitHub token exchange failed") + (err.isEmpty() ? "" : " (" + err + ")")).toStdString());
    return {token};
}

OAuthClient::TokenResult OAuthClient::oauthBitbucket(const QString &clientId,
                                                    const QString &clientSecret) {
    if (clientSecret.isEmpty())
        throw std::runtime_error("Bitbucket client secret missing. Set CLIENT_BITBUCKET_SECRET in your .env");
    const QString state = randomBase64Url(16);
    const QString verifier = randomBase64Url(32);
    // No scope param: scopes live on the OAuth consumer.
    const QUrl authUrl = buildAuthUrl("https://bitbucket.org/site/oauth2/authorize", clientId,
                                      redirectUri(), state, pkceChallenge(verifier), {}, true);
    const CodeResult cb = authorize(authUrl);
    if (!cb.error.isEmpty() || cb.code.isEmpty() || cb.state != state)
        throw std::runtime_error(
            ("OAuth: " + (!cb.error.isEmpty() ? cb.error : QString("code/state mismatch")))
                .toStdString());
    QUrlQuery form;
    form.addQueryItem("grant_type", "authorization_code");
    form.addQueryItem("code", cb.code);
    form.addQueryItem("redirect_uri", redirectUri());
    form.addQueryItem("code_verifier", verifier);
    const QString basic =
        "Basic " + QString::fromLatin1((clientId + ":" + clientSecret).toLatin1().toBase64());
    QString err;
    const QJsonObject json = postForm(QUrl("https://bitbucket.org/site/oauth2/access_token"),
                                      form.toString(QUrl::FullyEncoded), basic, &err);
    const QString token = json.value("access_token").toString();
    if (token.isEmpty())
        throw std::runtime_error(
            (json.value("error_description").toString("Bitbucket token exchange failed")).toStdString());
    TokenResult r;
    r.accessToken = token;
    r.refreshToken = json.value("refresh_token").toString();
    const double expiresIn = json.value("expires_in").toDouble(7200);
    r.expiresAt = QDateTime::currentMSecsSinceEpoch()
        + static_cast<qint64>(qMax(0.0, expiresIn - 60) * 1000);
    return r;
}

OAuthClient::TokenResult OAuthClient::oauthGitlab(const QString &clientId,
                                                 const QString &clientSecret) {
    if (clientSecret.isEmpty())
        throw std::runtime_error("GitLab client secret missing. Set CLIENT_GITLAB_SECRET in your .env");
    const QString state = randomBase64Url(16);
    const QString verifier = randomBase64Url(32);
    const QUrl authUrl = buildAuthUrl("https://gitlab.com/oauth/authorize", clientId,
                                      redirectUri(), state, pkceChallenge(verifier), "api",
                                      true);
    const CodeResult cb = authorize(authUrl);
    if (!cb.error.isEmpty() || cb.code.isEmpty() || cb.state != state)
        throw std::runtime_error(
            ("OAuth: " + (!cb.error.isEmpty() ? cb.error : QString("code/state mismatch")))
                .toStdString());
    QUrlQuery form;
    form.addQueryItem("client_id", clientId);
    form.addQueryItem("client_secret", clientSecret);
    form.addQueryItem("grant_type", "authorization_code");
    form.addQueryItem("code", cb.code);
    form.addQueryItem("redirect_uri", redirectUri());
    form.addQueryItem("code_verifier", verifier);
    QString err;
    const QJsonObject json = postForm(QUrl("https://gitlab.com/oauth/token"),
                                      form.toString(QUrl::FullyEncoded), {}, &err);
    const QString token = json.value("access_token").toString();
    if (token.isEmpty())
        throw std::runtime_error(
            (json.value("error_description").toString("GitLab token exchange failed")).toStdString());
    return {token};
}
