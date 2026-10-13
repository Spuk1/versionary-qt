#pragma once

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <functional>

// Runs blocking git work off the GUI thread and marshals the result back.
// Exceptions thrown by the worker (GitException) arrive as onError text.
template <typename T>
void runBackground(QObject *context, std::function<T()> work, std::function<void(T)> onDone,
                   std::function<void(const QString &)> onError = {}) {
    auto *watcher = new QFutureWatcher<T>(context);
    QObject::connect(watcher, &QFutureWatcher<T>::finished, context,
                     [watcher, onDone = std::move(onDone), onError = std::move(onError)] {
                         watcher->deleteLater();
                         try {
                             onDone(watcher->result());
                         } catch (const std::exception &e) {
                             if (onError)
                                 onError(QString::fromStdString(e.what()));
                         } catch (...) {
                             if (onError)
                                 onError("Unknown error");
                         }
                     });
    watcher->setFuture(QtConcurrent::run(std::move(work)));
}

inline void runBackgroundVoid(QObject *context, std::function<void()> work,
                              std::function<void()> onDone = {},
                              std::function<void(const QString &)> onError = {}) {
    auto *watcher = new QFutureWatcher<void>(context);
    QObject::connect(watcher, &QFutureWatcher<void>::finished, context,
                     [watcher, onDone = std::move(onDone), onError = std::move(onError)] {
                         watcher->deleteLater();
                         try {
                             watcher->waitForFinished();
                             if (onDone)
                                 onDone();
                         } catch (const std::exception &e) {
                             if (onError)
                                 onError(QString::fromStdString(e.what()));
                         } catch (...) {
                             if (onError)
                                 onError("Unknown error");
                         }
                     });
    watcher->setFuture(QtConcurrent::run(std::move(work)));
}
