#include "core/AuthStore.h"
#include "core/IdentityStore.h"
#include "core/SshStore.h"
#include "core/WorkspaceStore.h"
#include "net/ApiClient.h"
#include "net/OAuthClient.h"
#include "ui/BlameDialog.h"
#include "ui/BranchDialog.h"
#include "ui/CloneDialog.h"
#include "ui/ConflictDialog.h"
#include "ui/FileHistoryDialog.h"
#include "ui/OnboardingDialog.h"
#include "ui/QuickOpenDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/StashDialog.h"
#include "ui/StashViewerDialog.h"
#include "ui/CompareDialog.h"
#include "ui/ThemeManager.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

// Every dialog must at least construct and show without crashing
// (regression test for the Settings identity-tab nullptr crash).
class TestDialogs : public QObject {
    Q_OBJECT
private slots:
    void constructAll() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AuthStore auth(tmp.filePath("tokens.json"));
        SshStore ssh(tmp.filePath("ssh.json"));
        ApiClient api;
        IdentityStore ident;
        OAuthClient oauth;
        ThemeManager themes;
        WorkspaceState ws;

        StashDialog stash({"a.txt"});
        BranchDialog branch("main");
        CloneDialog clone(&auth, &api, &ssh, ws);
        SettingsDialog settings(&auth, &api, &ident, &oauth, &ssh, &themes, {});
        OnboardingDialog onboarding;
        ConflictDialog conflict(nullptr, {"a.txt"});
        BlameDialog blame("a.txt", {});
        FileHistoryDialog history(nullptr, "a.txt", {});
        QuickOpenDialog quick({{"demo", "/tmp/demo"}});
        StashViewerDialog stashViewer("{0}: WIP on main");
        CompareDialog compare(nullptr, "feature", {"main", "feature"});
        const QList<QDialog *> dialogs = {&stash, &branch, &clone, &settings,
                                                &onboarding, &conflict, &blame, &history,
                                                &quick, &stashViewer, &compare};
        for (QDialog *dlg : dialogs) {
            dlg->show();
            QVERIFY(QTest::qWaitForWindowExposed(dlg, 15000));
            dlg->close();
        }
    }
};

QTEST_MAIN(TestDialogs)
#include "test_dialogs.moc"
