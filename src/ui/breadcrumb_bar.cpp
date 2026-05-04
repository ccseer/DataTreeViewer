#include "breadcrumb_bar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

BreadcrumbBar::BreadcrumbBar(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);
    setVisible(false);
}

void BreadcrumbBar::setPath(const QStringList& segments)
{
    m_segments = segments;
    rebuild(segments);
}

void BreadcrumbBar::clear()
{
    m_segments.clear();
    rebuild({});
}

void BreadcrumbBar::rebuild(const QStringList& segments)
{
    // Clear existing widgets
    QLayout* lay = layout();
    if (!lay) return;
    while (QLayoutItem* item = lay->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (segments.isEmpty()) {
        setVisible(false);
        return;
    }

    setVisible(true);

    for (int i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            auto* sep = new QLabel(">", this);
            sep->setStyleSheet("color: #808080;");
            static_cast<QHBoxLayout*>(lay)->addWidget(sep);
        }

        auto* btn = new QToolButton(this);
        btn->setText(segments[i]);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setAutoRaise(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QToolButton { border: none; padding: 2px 4px; color: #4a86cf; font-weight: bold; }"
            "QToolButton:hover { text-decoration: underline; color: #2a5db0; }");

        connect(btn, &QToolButton::clicked, this, [this, i] {
            emit segmentClicked(i);
        });

        static_cast<QHBoxLayout*>(lay)->addWidget(btn);
    }

    static_cast<QHBoxLayout*>(lay)->addStretch();
}
