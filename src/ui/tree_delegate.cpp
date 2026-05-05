#include "tree_delegate.h"

#include <QApplication>
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QStyle>
#include <QStyleOptionViewItem>

#include "tree_model.h"

TreeDelegate::TreeDelegate(QObject *parent) : QStyledItemDelegate(parent)
{}

void TreeDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const QString comment = index.data(TreeModel::CommentRole).toString();
    if(index.column() == 0) {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();

        QStyleOptionViewItem bgOpt = opt;
        bgOpt.text.clear();
        bgOpt.icon = QIcon();
        style->drawControl(QStyle::CE_ItemViewItem, &bgOpt, painter, opt.widget);

        const bool selected = opt.state & QStyle::State_Selected;
        QColor keyColor = opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        QColor commentColor =
            opt.palette.color(selected ? QPalette::HighlightedText : QPalette::PlaceholderText);
        commentColor.setAlpha(selected ? 190 : 155);

        const qreal dpr = opt.widget ? opt.widget->devicePixelRatioF() : 1.0;
        const int iconSize = qRound(18 * dpr);
        const int iconGap = qRound(4 * dpr);
        const int topPad = qRound(3 * dpr);
        const int sidePad = qRound(4 * dpr);
        QRect contentRect = opt.rect.adjusted(sidePad, topPad, -sidePad, -topPad);
        const int iconX = contentRect.left();
        const int textX = iconX + iconSize + iconGap;
        QFont keyFont = opt.font;
        QFont commentFont = keyFont;
        commentFont.setBold(false);
        commentFont.setItalic(true);

        QFontMetrics keyFm(keyFont);
        QFontMetrics commentFm(commentFont);
        const int keyHeight = keyFm.height();
        const int commentGap = comment.isEmpty() ? 0 : qRound(2 * dpr);
        const int commentHeight = comment.isEmpty() ? 0 : commentFm.height();
        const int totalTextHeight = keyHeight + commentGap + commentHeight;
        const int startY = contentRect.top() + qMax(0, (contentRect.height() - totalTextHeight) / 2);
        QRect keyRect(textX, startY, contentRect.right() - textX + 1, keyHeight);
        QRect commentRect(textX, keyRect.bottom() + commentGap,
                          contentRect.right() - textX + 1, commentHeight);

        painter->save();
        if(!opt.icon.isNull()) {
            QSize drawIconSize = opt.decorationSize;
            if(drawIconSize.width() <= 0 || drawIconSize.height() <= 0)
                drawIconSize = QSize(iconSize, iconSize);
            QPixmap pix = opt.icon.pixmap(drawIconSize,
                                          selected ? QIcon::Selected : QIcon::Normal,
                                          QIcon::On);
            QRect centeredIcon(iconX,
                               startY + (keyHeight - drawIconSize.height()) / 2,
                               drawIconSize.width(), drawIconSize.height());
            painter->drawPixmap(centeredIcon, pix);
        }

        painter->setFont(keyFont);
        painter->setPen(keyColor);
        painter->drawText(keyRect, Qt::AlignLeft | Qt::AlignVCenter,
                          keyFm.elidedText(opt.text, Qt::ElideRight, keyRect.width()));

        if(!comment.isEmpty()) {
            QString commentText = comment;
            commentText.replace('\n', QStringLiteral(" "));
            commentText.remove('\r');
            commentText = QStringLiteral("// ") + commentText.simplified();

            painter->setFont(commentFont);
            painter->setPen(commentColor);
            painter->drawText(commentRect, Qt::AlignLeft | Qt::AlignVCenter,
                              commentFm.elidedText(commentText, Qt::ElideRight,
                                                   commentRect.width()));
        }
        painter->restore();
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    QStyledItemDelegate::paint(painter, opt, index);
}

void TreeDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);

    if(option->state & QStyle::State_Selected) {
        option->palette.setColor(QPalette::Text, option->palette.color(QPalette::HighlightedText));
        option->palette.setColor(QPalette::WindowText, option->palette.color(QPalette::HighlightedText));
        option->palette.setColor(QPalette::ButtonText, option->palette.color(QPalette::HighlightedText));
        option->palette.setColor(QPalette::All, QPalette::Text, option->palette.color(QPalette::HighlightedText));

        // Force the icon to use its 'Selected' state pixmap
        if(!option->icon.isNull()) {
            QSize sz = option->decorationSize;
            if(sz.width() <= 0 || sz.height() <= 0)
                sz = QSize(18, 18);
            option->decorationSize = sz;
            option->icon = option->icon.pixmap(sz, QIcon::Selected, QIcon::On);
        }
    }
}
