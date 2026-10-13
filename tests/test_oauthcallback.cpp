#include "net/OAuthClient.h"

#include <QFuture>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <QtTest/QtTest>

class TestOAuthCallback : public QObject {
    Q_OBJECT
private slots:
    void incompleteHeadersAreNotComplete() {
        const auto p = OAuthClient::parseCallbackRequest("GET /oauth/callback?code=abc HTTP/1.1\r\n");
        QVERIFY(!p.complete);
        QVERIFY(p.code.isEmpty());
    }

    void parsesCodeStateAndExtras() {
        // Mirrors a real GitHub redirect (note the extra iss parameter).
        const QByteArray raw = "GET /oauth/callback?code=5a31ca7c&"
                               "iss=https%3A%2F%2Fgithub.com%2Flogin%2Foauth&"
                               "state=xyz123 HTTP/1.1\r\nHost: 127.0.0.1:6969\r\n\r\n";
        const auto p = OAuthClient::parseCallbackRequest(raw);
        QVERIFY(p.complete);
        QCOMPARE(p.code, QString("5a31ca7c"));
        QCOMPARE(p.state, QString("xyz123"));
        QVERIFY(p.error.isEmpty());
    }

    void parsesProviderError() {
        const QByteArray raw = "GET /oauth/callback?error=access_denied&"
                               "error_description=denied+by+user HTTP/1.1\r\n\r\n";
        const auto p = OAuthClient::parseCallbackRequest(raw);
        QVERIFY(p.complete);
        QVERIFY(p.code.isEmpty());
        QCOMPARE(p.error, QString("access_denied"));
    }

    void ignoresNonCallbackPaths() {
        const QByteArray raw = "GET /favicon.ico HTTP/1.1\r\n\r\n";
        const auto p = OAuthClient::parseCallbackRequest(raw);
        QVERIFY(p.complete);
        QVERIFY(p.code.isEmpty());
        QVERIFY(p.error.isEmpty());
    }

    void responseIsWellFormed() {
        OAuthClient::CallbackParse ok;
        ok.complete = true;
        ok.code = "abc";
        const QByteArray resp = OAuthClient::buildCallbackResponse(ok);
        QVERIFY(resp.startsWith("HTTP/1.1 200 OK\r\n"));
        QVERIFY(resp.contains("Content-Length: "));
        QVERIFY(resp.contains("Connection: close\r\n\r\n"));
        QVERIFY(resp.contains("Authentication complete"));
        // Content-Length must match the body.
        const int headerEnd = resp.indexOf("\r\n\r\n") + 4;
        const QByteArray body = resp.mid(headerEnd);
        for (const QByteArray &line : resp.left(headerEnd).split('\n')) {
            if (line.startsWith("Content-Length:")) {
                QCOMPARE(line.split(':').last().trimmed().toInt(), body.size());
            }
        }
    }

    void failureResponseShowsError() {
        OAuthClient::CallbackParse fail;
        fail.complete = true;
        fail.error = "access_denied";
        const QByteArray resp = OAuthClient::buildCallbackResponse(fail);
        QVERIFY(resp.contains("Authentication failed"));
        QVERIFY(resp.contains("access_denied"));
    }

    // Full handshake over a real loopback socket with a fragmented
    // request: the browser side must get a complete success page and the
    // authorizer must capture code + state.
    void fullLoopbackRoundTrip() {
        QTcpServer busyProbe;
        if (!busyProbe.listen(QHostAddress::LocalHost, OAuthClient::kLoopbackPort))
            QSKIP("loopback port 6969 busy (another OAuth flow running?)");
        busyProbe.close();
        QFuture<OAuthClient::CodeResult> fut = QtConcurrent::run([] {
            OAuthClient client;
            return client.authorize(QUrl("https://example.com/start"));
        });
        QTcpSocket sock;
        bool connected = false;
        for (int i = 0; i < 200 && !connected; ++i) {
            sock.connectToHost(QHostAddress::LocalHost, OAuthClient::kLoopbackPort);
            connected = sock.waitForConnected(50);
            if (!connected)
                QThread::msleep(25);
        }
        QVERIFY(connected);
        QVERIFY(sock.write("GET /oauth/callback?code=abc123&state=s3cr3t HT") > 0);
        QVERIFY(sock.waitForBytesWritten(2000));
        QThread::msleep(50);
        sock.write("TP/1.1\r\nHost: 127.0.0.1:6969\r\n\r\n");
        QVERIFY(sock.waitForBytesWritten(2000));
        QByteArray reply;
        while (sock.state() == QAbstractSocket::ConnectedState && reply.size() < 65536) {
            if (!sock.waitForReadyRead(2000))
                break;
            reply += sock.readAll();
        }
        sock.waitForDisconnected(5000);
        QVERIFY(reply.startsWith("HTTP/1.1 200 OK"));
        QVERIFY(reply.contains("Authentication complete"));
        const OAuthClient::CodeResult res = fut.result();
        QCOMPARE(res.code, QString("abc123"));
        QCOMPARE(res.state, QString("s3cr3t"));
        QVERIFY(res.error.isEmpty());
    }
};

QTEST_MAIN(TestOAuthCallback)
#include "test_oauthcallback.moc"
