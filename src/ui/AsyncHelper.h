#pragma once

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <functional>
#include <optional>

// Runs blocking git work off the GUI thread and marshals the result back.
//
// Exceptions are caught INSIDE the worker and shipped back as text. Relying
// on QFuture to rethrow them loses the message: QtConcurrent wraps throws
// that are not QExceptions, so the GUI thread only ever saw a generic
// "exception" string instead of the real error.
template <typename T>
void runBackground(QObject *context, std::function<T()> work, std::function<void(T)> onDone,
                   std::function<void(const QString &)> onError = {}) {
    struct Outcome {
        std::optional<T> value;
        QString error;
    };
    auto *watcher = new QFutureWatcher<Outcome>(context);
    QObject::connect(watcher, &QFutureWatcher<Outcome>::finished, context,
                     [watcher, onDone = std::move(onDone), onError = std::move(onError)] {
                         watcher->deleteLater();
                         Outcome outcome;
                         try {
                             outcome = watcher->result();
                         } catch (...) {
                             if (onError)
                                 onError("Something went wrong. Please try again.");
                             return;
                         }
                         if (!outcome.error.isEmpty()) {
                             if (onError)
                                 onError(outcome.error);
                             return;
                         }
                         if (!outcome.value.has_value()) {
                             if (onError)
                                 onError("Something went wrong. Please try again.");
                             return;
                         }
                         try {
                             onDone(std::move(*outcome.value));
                         } catch (const std::exception &e) {
                             if (onError) {
                                 const QString msg =
                                     QString::fromStdString(e.what()).trimmed();
                                 onError(msg.isEmpty() ? "Something went wrong. Please try again."
                                                       : msg);
                             }
                         } catch (...) {
                             if (onError)
                                 onError("Something went wrong. Please try again.");
                         }
                     });
    watcher->setFuture(QtConcurrent::run([work = std::move(work)] {
        Outcome outcome;
        try {
            outcome.value = work();
        } catch (const std::exception &e) {
            outcome.error = QString::fromStdString(e.what()).trimmed();
            if (outcome.error.isEmpty())
                outcome.error = "Something went wrong. Please try again.";
        } catch (...) {
            outcome.error = "Something went wrong. Please try again.";
        }
        return outcome;
    }));
}

inline void runBackgroundVoid(QObject *context, std::function<void()> work,
                              std::function<void()> onDone = {},
                              std::function<void(const QString &)> onError = {}) {
    struct Outcome {
        bool done = false;
        QString error;
    };
    auto *watcher = new QFutureWatcher<Outcome>(context);
    QObject::connect(watcher, &QFutureWatcher<Outcome>::finished, context,
                     [watcher, onDone = std::move(onDone), onError = std::move(onError)] {
                         watcher->deleteLater();
                         Outcome outcome;
                         try {
                             outcome = watcher->result();
                         } catch (...) {
                             if (onError)
                                 onError("Something went wrong. Please try again.");
                             return;
                         }
                         if (!outcome.error.isEmpty()) {
                             if (onError)
                                 onError(outcome.error);
                             return;
                         }
                         if (!outcome.done) {
                             if (onError)
                                 onError("Something went wrong. Please try again.");
                             return;
                         }
                         try {
                             if (onDone)
                                 onDone();
                         } catch (const std::exception &e) {
                             if (onError) {
                                 const QString msg =
                                     QString::fromStdString(e.what()).trimmed();
                                 onError(msg.isEmpty() ? "Something went wrong. Please try again."
                                                       : msg);
                             }
                         } catch (...) {
                             if (onError)
                                 onError("Something went wrong. Please try again.");
                         }
                     });
    watcher->setFuture(QtConcurrent::run([work = std::move(work)] {
        Outcome outcome;
        try {
            work();
            outcome.done = true;
        } catch (const std::exception &e) {
            outcome.error = QString::fromStdString(e.what()).trimmed();
            if (outcome.error.isEmpty())
                outcome.error = "Something went wrong. Please try again.";
        } catch (...) {
            outcome.error = "Something went wrong. Please try again.";
        }
        return outcome;
    }));
}
