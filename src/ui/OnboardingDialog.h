#pragma once

#include <QDialog>

// First-run wizard: git availability check, then identity hint.
// Mirrors Onboarding.tsx.
class OnboardingDialog : public QDialog {
    Q_OBJECT
public:
    explicit OnboardingDialog(QWidget *parent = nullptr);

    static bool shouldShow(bool hasRepositories);
    static void dismiss();

private slots:
    void checkGit();
    void goToSettings();

private:
    class QLabel *m_status;
    class QPushButton *m_primary;
    class QPushButton *m_refresh;
};
