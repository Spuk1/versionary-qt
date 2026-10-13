#pragma once

#include <QWidget>

// Thin sliding loading bar shown under the repo header while git actions
// run. Mirrors the knight-rider header-loading animation in Header.css:
// a blue beam ping-pongs across the bar on a ~2.2s loop.
class LoadingBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoadingBarWidget(QWidget *parent = nullptr);

    void setActive(bool active);
    bool isActive() const { return m_active; }

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void advance();

private:
    bool m_active = false;
    double m_phase = 0.0; // 0..1 across the loop
    class QTimer *m_timer;
};
