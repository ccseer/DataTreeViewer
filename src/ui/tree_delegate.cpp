#include "tree_delegate.h"

#include <QApplication>
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QStyleOptionViewItem>

#include "tree_model.h"

TreeDelegate::TreeDelegate(QObject *parent) : QStyledItemDelegate(parent)
{}

void TreeDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const QString comment = index.data(TreeModel::CommentRole).toString();
    if(index.column() == 0 && !comment.isEmpty()) {
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

        QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);
        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
        if(textRect.isNull())
            textRect = opt.rect.adjusted(4, 3, -4, -3);

        QFont keyFont = opt.font;
        QFont commentFont = keyFont;
        commentFont.setItalic(true);

        QFontMetrics keyFm(keyFont);
        QFontMetrics commentFm(commentFont);
        const int keyHeight = keyFm.height();
        QRect keyRect(textRect.left(), textRect.top() + 3, textRect.width(), keyHeight);
        QRect commentRect(textRect.left(), keyRect.bottom() + 2, textRect.width(),
                          commentFm.height());

        painter->save();
        if(!opt.icon.isNull()) {
            QSize iconSize = opt.decorationSize;
            if(iconSize.width() <= 0 || iconSize.height() <= 0)
                iconSize = QSize(18, 18);
            QPixmap pix = opt.icon.pixmap(iconSize, selected ? QIcon::Selected : QIcon::Normal,
                                          QIcon::On);
            QRect centeredIcon(iconRect.left(),
                               keyRect.top() + (keyRect.height() - iconSize.height()) / 2,
                               iconSize.width(), iconSize.height());
            painter->drawPixmap(centeredIcon, pix);
        }

        painter->setFont(keyFont);
        painter->setPen(keyColor);
        painter->drawText(keyRect, Qt::AlignLeft | Qt::AlignVCenter,
                          keyFm.elidedText(opt.text, Qt::ElideRight, keyRect.width()));

        QString commentText = comment;
        commentText.replace('\n', QStringLiteral(" "));
        commentText.remove('\r');
        commentText = QStringLiteral("// ") + commentText.simplified();

        painter->setFont(commentFont);
        painter->setPen(commentColor);
        painter->drawText(commentRect, Qt::AlignLeft | Qt::AlignVCenter,
                          commentFm.elidedText(commentText, Qt::ElideRight, commentRect.width()));
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
            if (sz.width() <= 0) sz = QSize(18, 18);
            option->icon = option->icon.pixmap(sz, QIcon::Selected, QIcon::On);
        }
    }
}
