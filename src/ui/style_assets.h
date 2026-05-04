#pragma once

#include <QByteArray>
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace dtv {
namespace ui {

// --- Theme Colors (Seer Baseline) ---

namespace Colors {
// Shared
inline const QString Accent = "#0288D1";

// Dark Theme
inline const QString DarkBG = "#121212";
inline const QString DarkSurface = "#1E1E1E";
inline const QString DarkText = "#EEEEEE";
inline const QString DarkTextDim = "#BDBDBD";
inline const QString DarkBorder = "#333333";

// Light Theme
inline const QString LightBG = "#FFFFFF";
inline const QString LightSurface = "#F5F5F5";
inline const QString LightText = "#212121";
inline const QString LightTextDim = "#757575";
inline const QString LightBorder = "#E0E0E0";
} // namespace Colors

// --- SVGs (Material Symbols) ---

// Material Symbol: "Article" (text view button)
constexpr auto g_svg_article = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="M312-300h336v-44H312v44Zm0-160h336v-44H312v44Zm0-160h336v-44H312v44ZM228-156q-29.7 0-50.85-21.15Q156-198.3 156-228v-504q0-29.7 21.15-50.85Q198.3-804 228-804h504q29.7 0 50.85 21.15Q804-761.7 804-732v504q0 29.7-21.15 50.85Q761.7-156 732-156H228Zm0-72h504v-504H228v504Zm0 0v-504 504Z"/>
</svg>)SVG";

// Material Symbol: "Info" Rounded, Outline
constexpr auto g_svg_info = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="M480-120q-75 0-140.5-28.5t-114-77q-48.5-48.5-77-114T120-480q0-75 28.5-140.5t77-114q48.5-48.5 114-77T480-840q75 0 140.5 28.5t114 77q48.5 48.5 77 114T840-480q0 75-28.5 140.5t-77 114q-48.5 48.5-114 77T480-120Zm0-72q120 0 204-84t84-204q0-120-84-204t-204-84q-120 0-204 84t-84 204q0 120 84 204t204 84Zm-40-101h80v-240h-80v240Zm40-327q17 0 28.5-11.5T520-660q0-17-11.5-28.5T480-700q-17 0-28.5 11.5T440-660q0 17 11.5 28.5T480-620Z"/>
</svg>)SVG";

// Material Symbol: "Curly Brackets" (Object icon)
constexpr auto g_svg_object = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
  <path fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"
    d="M10 4H9a2 2 0 0 0-2 2v5a2 2 0 0 1-2 2a2 2 0 0 1 2 2v5a2 2 0 0 0 2 2h1m4-18h1a2 2 0 0 1 2 2v5a2 2 0 0 0 2 2a2 2 0 0 0-2 2v5a2 2 0 0 1-2 2h-1"/>
</svg>)SVG";

// Material Symbol: "Square Brackets" (Array icon)
constexpr auto g_svg_array = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
  <path fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"
    d="M8 5H7a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h1m8-14h1a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2h-1"/>
</svg>)SVG";

// --- QSS (Qt Style Sheets) ---

// Placeholder args: %1: SurfaceBG, %2: Border
constexpr auto g_qss_bottom_bar = R"(
    QWidget#btmBar {
        background-color: %1;
        border-top: 1px solid %2;
    }
    QPushButton#textViewBtn {
        border: none; background: transparent; border-radius: 4px;
    }
    QPushButton#textViewBtn:hover {
        background-color: rgba(128, 128, 128, 40);
    }
    QPushButton#textViewBtn:pressed {
        background-color: rgba(128, 128, 128, 60);
    }
)";

// --- Helpers ---

inline QIcon svgIcon(const char *data, const QColor &color, int iconSz)
{
    QByteArray svg(data);
    svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());
    QSvgRenderer renderer(svg);
    if(!renderer.isValid())
        return {};
    QPixmap pix(iconSz, iconSz);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    renderer.render(&p, QRectF(0, 0, iconSz, iconSz));
    p.end();
    return QIcon(pix);
}

} // namespace ui
} // namespace dtv
