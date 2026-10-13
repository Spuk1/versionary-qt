#include "LoadingBarWidget.h"

#include <QLinearGradient>
#include <QPainter>
#include <QTimer>
#include <QtMath>

namespace {
constexpr int kBarHeight = 4;
constexpr int kLoopMs = 2200;
constexpr int kFrameMs = 33; // ~30fps
constexpr double kBeamWidth = 0.4;
} // namespace

LoadingBarWidget::LoadingBarWidget(QWidget *parent) : QWidget(parent) {
    setFixedHeight(kBarHeight);
    setVisible(false);
    m_timer = new QTimer(this);
    m_timer->setInterval(kFrameMs);
    connect(m_timer, &QTimer::timeout, this, &LoadingBarWidget::advance);
}

void LoadingBarWidget::setActive(bool active) {
    if (active == m_active)
        return;
    m_active = active;
    setVisible(active);
    if (active) {
        m_phase = 0.0;
        m_timer->start();
    } else {
        m_timer->stop();
    }
}

void LoadingBarWidget::advance() {
    m_phase += double(kFrameMs) / double(kLoopMs);
    if (m_phase >= 1.0)
        m_phase -= 1.0;
    update();
}

void LoadingBarWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    // Track
    p.fillRect(rect(), QColor(10, 40, 70, 140));
    // Beam position: ping-pong with sinusoidal easing (ease-in-out feel).
    const double t = 0.5 - 0.5 * qCos(m_phase * 2.0 * M_PI);
    const double travel = 1.0 + kBeamWidth;
    const double left = t * travel - kBeamWidth;
    const int x0 = int(left * width());
    const int beamW = int(kBeamWidth * width());
    QLinearGradient grad(x0, 0, x0 + beamW, 0);
    grad.setColorAt(0.0, QColor(0x1f, 0x7b, 0xff, 0));
    grad.setColorAt(0.5, QColor(0x7c, 0xc8, 0xff));
    grad.setColorAt(1.0, QColor(0x1f, 0x7b, 0xff, 0));
    p.fillRect(QRect(x0, 0, beamW, height()), grad);
}
