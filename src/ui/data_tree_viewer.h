#pragma once

#include "seer/viewerbase.h"

#include <memory>

struct ConfigNode;
struct ParseResult;
class BreadcrumbBar;
class QPushButton;
class SearchBar;
class StatusBar;
class TreeRenderer;

class DataTreeViewer : public ViewerBase {
    Q_OBJECT
public:
    explicit DataTreeViewer(QWidget *parent = nullptr);
    ~DataTreeViewer() override;

    QString name() const override
    {
        return "DataTreeViewer";
    }
    QSize getContentSize() const override
    {
        return {960, 700};
    }

    void updateDPR(qreal r) override;
    void updateTheme(int theme) override;

signals:
    void cancelRequested();

private:
    void loadImpl(QBoxLayout *lay_content, QHBoxLayout *lay_ctrlbar) override;

    void init();
    void doLoadFile(const QString &path);
    void cancel();
    void cancelPending();
    void showLoadingState();
    void onParseCompleted(std::shared_ptr<const ParseResult> result, qint64 elapsedMs);
    void onNodeActivated(const ConfigNode *node);
    void onBreadcrumbClicked(int pathIndex);
    void onFilterChanged(const QString &text);
    void onTextViewBtnClicked();
    void showError(const std::string &message, int errLine, const std::string &title = "ERROR");
    void updateStatusTooltip();
    void reapplyStyles();

    BreadcrumbBar *m_breadcrumb = nullptr;
    TreeRenderer *m_renderer = nullptr;
    SearchBar *m_search = nullptr;
    StatusBar *m_status = nullptr;
    QPushButton *m_btn_text_view = nullptr;

    int m_generation = 0;
    std::shared_ptr<const ParseResult> m_lastResult;

    bool m_isDarkMode = false;
    qreal m_dpr = 1.0;
};

class DTPlugin : public QObject, public ViewerPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ViewerPluginInterface_iid FILE "../../bin/plugin.json")
    Q_INTERFACES(ViewerPluginInterface)
public:
    ViewerBase *createViewer(QWidget *parent = nullptr) override
    {
        return new DataTreeViewer(parent);
    }
};
