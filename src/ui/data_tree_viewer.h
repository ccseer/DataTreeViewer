#pragma once

#include <QWidget>

#include <memory>

struct ConfigNode;
struct ParseResult;
class BreadcrumbBar;
class SearchBar;
class StatusBar;
class TreeRenderer;

class DataTreeViewer : public QWidget {
    Q_OBJECT
public:
    explicit DataTreeViewer(QWidget* parent = nullptr);
    ~DataTreeViewer() override;

    void loadFile(const QString& path);
    void cancel();

signals:
    void cancelRequested();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void cancelPending();
    void showLoadingState();
    void onParseCompleted(std::shared_ptr<const ParseResult> result,
                          qint64 elapsedMs);
    void onNodeActivated(const ConfigNode* node);
    void onBreadcrumbClicked(int pathIndex);
    void onFilterChanged(const QString& text);
    void showError(const std::string& message, int errLine);

    BreadcrumbBar* m_breadcrumb = nullptr;
    TreeRenderer*  m_renderer   = nullptr;
    SearchBar*     m_search     = nullptr;
    StatusBar*     m_status     = nullptr;

    int          m_generation = 0;
    std::shared_ptr<const ParseResult> m_lastResult;

    QString      m_pendingLoad;
};
