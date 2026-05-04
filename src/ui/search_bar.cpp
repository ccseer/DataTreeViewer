#include "search_bar.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTimer>

SearchBar::SearchBar(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText("Search...");
    m_edit->setClearButtonEnabled(true);
    layout->addWidget(m_edit);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(150);

    connect(m_edit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_timer->start();
    });

    connect(m_timer, &QTimer::timeout, this, [this] {
        emit filterChanged(m_edit->text());
    });

    connect(m_edit, &QLineEdit::returnPressed, this, [this] {
        if (m_edit->text().isEmpty()) return;
        emit nextMatch();
    });

    // Shift+Enter for previous match
    m_edit->installEventFilter(this);
}

QString SearchBar::text() const
{
    return m_edit->text();
}

void SearchBar::clear()
{
    m_edit->clear();
}

bool SearchBar::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_edit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return &&
            keyEvent->modifiers() & Qt::ShiftModifier) {
            if (!m_edit->text().isEmpty())
                emit prevMatch();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
