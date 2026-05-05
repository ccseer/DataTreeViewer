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
inline constexpr const char *Accent = "#0288D1";

// Dark Theme
inline constexpr const char *DarkBG = "#121212";
inline constexpr const char *DarkSurface = "#1E1E1E";
inline constexpr const char *DarkText = "#EEEEEE";
inline constexpr const char *DarkTextDim = "#BDBDBD";
inline constexpr const char *DarkBorder = "#333333";
inline constexpr const char *DarkInput = "#1E1E1E";

// Light Theme
inline constexpr const char *LightBG = "#FFFFFF";
inline constexpr const char *LightSurface = "#F5F5F5";
inline constexpr const char *LightText = "#212121";
inline constexpr const char *LightTextDim = "#757575";
inline constexpr const char *LightBorder = "#E0E0E0";
inline constexpr const char *LightInput = "#FFFFFF";
} // namespace Colors

// --- SVGs (Material Symbols) ---

// Material Symbol: "Article" (text view button)
inline constexpr auto g_svg_article = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="M312-300h336v-44H312v44Zm0-160h336v-44H312v44Zm0-160h336v-44H312v44ZM228-156q-29.7 0-50.85-21.15Q156-198.3 156-228v-504q0-29.7 21.15-50.85Q198.3-804 228-804h504q29.7 0 50.85 21.15Q804-761.7 804-732v504q0 29.7-21.15 50.85Q761.7-156 732-156H228Zm0-72h504v-504H228v504Zm0 0v-504 504Z"/>
</svg>)SVG";

// Material Symbol: "Info" Rounded, Outline
inline constexpr auto g_svg_info = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="M480-120q-75 0-140.5-28.5t-114-77q-48.5-48.5-77-114T120-480q0-75 28.5-140.5t77-114q48.5-48.5 114-77T480-840q75 0 140.5 28.5t114 77q48.5 48.5 77 114T840-480q0 75-28.5 140.5t-77 114q-48.5 48.5-114 77T480-120Zm0-72q120 0 204-84t84-204q0-120-84-204t-204-84q-120 0-204 84t-84 204q0 120 84 204t204 84Zm-40-101h80v-240h-80v240Zm40-327q17 0 28.5-11.5T520-660q0-17-11.5-28.5T480-700q-17 0-28.5 11.5T440-660q0 17 11.5 28.5T480-620Z"/>
</svg>)SVG";

// Material Symbol: "Curly Brackets" (Object icon)
inline constexpr auto g_svg_object = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
  <path fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"
    d="M10 4H9a2 2 0 0 0-2 2v5a2 2 0 0 1-2 2a2 2 0 0 1 2 2v5a2 2 0 0 0 2 2h1m4-18h1a2 2 0 0 1 2 2v5a2 2 0 0 0 2 2a2 2 0 0 0-2 2v5a2 2 0 0 1-2 2h-1"/>
</svg>)SVG";

// Material Symbol: "Square Brackets" (Array icon)
inline constexpr auto g_svg_array = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
  <path fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"
    d="M8 5H7a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h1m8-14h1a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2h-1"/>
</svg>)SVG";

// --- QSS (Qt Style Sheets) ---

// Placeholder args: %1: SurfaceBG, %2: Border, %3: InputBG, %4: Text, %5:
// Accent, %6: Radius, %7: PaddingV, %8: PaddingH
inline constexpr auto g_qss_top_bar = R"(
    QWidget#topBar { background-color: %1; border: none; }
    QLineEdit {
        background-color: %3; border: 1px solid %2; border-radius: %6px;
        color: %4; padding: %7px %8px; selection-background-color: %5;
    }
    QLineEdit:focus { border: 1px solid %5; }
)";

// Placeholder args: %1: SurfaceBG, %2: Border
inline constexpr auto g_qss_bottom_bar = R"(
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

inline QIcon createMultiStateIcon(const char *data, const QColor &normalColor,
                                  const QColor &selectedColor, int iconSz = 20)
{
    QIcon icon;

    auto render = [&](const QColor &c) {
        QByteArray svg(data);
        svg.replace("currentColor", c.name(QColor::HexRgb).toUtf8());
        QSvgRenderer renderer(svg);
        QPixmap pix(iconSz, iconSz);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p);
        return pix;
    };

    QPixmap normal = render(normalColor);
    QPixmap selected = render(selectedColor);

    // Normal state (Off/On)
    icon.addPixmap(normal, QIcon::Normal, QIcon::Off);
    icon.addPixmap(normal, QIcon::Normal, QIcon::On);

    // Active state (Off/On) - used when the window is focused
    icon.addPixmap(selected, QIcon::Active, QIcon::Off);
    icon.addPixmap(selected, QIcon::Active, QIcon::On);

    // Selected state (Off/On) - used when the item is selected
    icon.addPixmap(selected, QIcon::Selected, QIcon::Off);
    icon.addPixmap(selected, QIcon::Selected, QIcon::On);

    // Also add to Disabled just in case, using a dimmed version
    QColor disabledColor = normalColor;
    disabledColor.setAlpha(100);
    icon.addPixmap(render(disabledColor), QIcon::Disabled, QIcon::Off);

    return icon;
}

inline QIcon createIcon(const char *data, const QColor &color, int iconSz = 20)
{
    return createMultiStateIcon(data, color, Qt::white, iconSz);
}

} // namespace ui
} // namespace dtv
