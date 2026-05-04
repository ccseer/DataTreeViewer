#pragma once

#include <QWidget>

class QLineEdit;
class QTimer;

class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(QWidget* parent = nullptr);

    QString text() const;
    void    clear();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    void filterChanged(const QString& text);
    void nextMatch();
    void prevMatch();

private:
    QLineEdit* m_edit  = nullptr;
    QTimer*    m_timer = nullptr;
};
