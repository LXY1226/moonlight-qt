#include "overlaytoast.h"

#include <QGuiApplication>
#include <QScreen>
#include <QFontMetrics>
#include <QPainterPath>
#include <QtMath>

static int getDevicePixelAlignedStep(qreal dpr)
{
    if (dpr <= 1.0) {
        return 1;
    }

    // Keep the transparent top-level window on whole physical pixels at common
    // fractional scales, otherwise the compositor may resample the whole toast.
    int scaledDpr = qRound(dpr * 100.0);
    int a = qAbs(scaledDpr);
    int b = 100;
    while (b != 0) {
        int r = a % b;
        a = b;
        b = r;
    }

    int step = 100 / qMax(a, 1);
    return step <= 16 ? step : 1;
}

static int alignPositionToDevicePixels(int value, int step)
{
    return qRound((qreal)value / step) * step;
}

static int alignSizeToDevicePixels(int value, int step)
{
    return qCeil((qreal)value / step) * step;
}

OverlayToast::OverlayToast(QWindow* parent)
    : QRasterWindow(parent),
      m_FadeAnimation(nullptr),
      m_ToastHeight(40),
      m_HorizPadding(24),
      m_VertPadding(10),
      m_BorderRadius(8)
{
    setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
             | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);

    QSurfaceFormat fmt;
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);

    m_Font = QFont("Segoe UI", 10);
    m_Font.setWeight(QFont::Medium);

    m_DismissTimer.setSingleShot(true);
    connect(&m_DismissTimer, &QTimer::timeout, this, &OverlayToast::startFadeOut);

    m_FadeAnimation = new QPropertyAnimation(this, "opacity", this);
    m_FadeAnimation->setDuration(400);
    m_FadeAnimation->setStartValue(1.0);
    m_FadeAnimation->setEndValue(0.0);
    connect(m_FadeAnimation, &QPropertyAnimation::finished,
            this, &OverlayToast::onFadeFinished);
}

OverlayToast::~OverlayToast()
{
}

void OverlayToast::showToast(int parentX, int parentY, int parentW, int parentH,
                             const QString& message, int durationMs)
{
    m_Message = message;

    // Stop any ongoing fade / dismiss
    m_DismissTimer.stop();
    m_FadeAnimation->stop();
    setOpacity(1.0);

    QScreen* targetScreen = screen();
    if (targetScreen == nullptr) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    qreal dpr = targetScreen ? targetScreen->devicePixelRatio() : 1.0;
    int devicePixelStep = getDevicePixelAlignedStep(dpr);

    // Calculate dimensions
    QFontMetricsF fm(m_Font);
    int textWidth = qCeil(fm.horizontalAdvance(m_Message)) + m_HorizPadding * 2;
    int toastWidth = qMin(textWidth, 500);
    if (toastWidth < 120) toastWidth = 120;
    toastWidth = alignSizeToDevicePixels(toastWidth, devicePixelStep);
    int toastHeight = alignSizeToDevicePixels(m_ToastHeight, devicePixelStep);

#ifdef Q_OS_MACOS
    // On macOS, SDL and Qt both use points (logical coordinates)
    int qpX = parentX;
    int qpY = parentY;
    int qpW = parentW;
    int qpH = parentH;
#else
    // Convert SDL pixel coords to Qt DIP
    int qpX = qRound(parentX / dpr);
    int qpY = qRound(parentY / dpr);
    int qpW = qRound(parentW / dpr);
    int qpH = qRound(parentH / dpr);
#endif

    // Position at bottom-center, 60px above the bottom
    int x = qpX + (qpW - toastWidth) / 2;
    int y = qpY + qpH - toastHeight - 60;

    setGeometry(alignPositionToDevicePixels(x, devicePixelStep),
                alignPositionToDevicePixels(y, devicePixelStep),
                toastWidth,
                toastHeight);
    show();
    raise();
    requestUpdate();

    m_DismissTimer.start(durationMs);
}

void OverlayToast::startFadeOut()
{
    m_FadeAnimation->start();
}

void OverlayToast::onFadeFinished()
{
    hide();
    setOpacity(1.0);
}

void OverlayToast::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    int w = width();
    int h = height();

    // Clear to transparent
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(0, 0, w, h, Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // Dark semi-transparent rounded background
    QPainterPath bg;
    bg.addRoundedRect(QRectF(0, 0, w, h), m_BorderRadius, m_BorderRadius);
    p.fillPath(bg, QColor(20, 26, 42, 200));

    // White text centered
    p.setFont(m_Font);
    p.setPen(Qt::white);
    p.drawText(QRect(m_HorizPadding, 0, w - m_HorizPadding * 2, h),
               Qt::AlignCenter, m_Message);
}
