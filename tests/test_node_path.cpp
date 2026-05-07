#include <QAbstractItemModel>
#include <QTest>

#include "core/config_node.h"
#include "ui/node_path.h"
#include "ui/tree_model.h"

class TestNodePath : public QObject {
    Q_OBJECT

private:
    ConfigNode makeTree() const
    {
        ConfigNode root;
        root.type = ConfigNode::Type::Object;

        ConfigNode server;
        server.key = "server";
        server.type = ConfigNode::Type::Object;

        ConfigNode database;
        database.key = "database";
        database.type = ConfigNode::Type::Object;

        ConfigNode host;
        host.key = "host";
        host.type = ConfigNode::Type::String;
        host.scalar = "localhost";
        database.children.push_back(std::move(host));
        server.children.push_back(std::move(database));
        root.children.push_back(std::move(server));

        ConfigNode items;
        items.key = "items";
        items.type = ConfigNode::Type::Array;
        for(int i = 0; i < 3; ++i) {
            ConfigNode item;
            item.type = ConfigNode::Type::Object;
            ConfigNode name;
            name.key = "name";
            name.type = ConfigNode::Type::String;
            name.scalar = "item";
            item.children.push_back(std::move(name));
            items.children.push_back(std::move(item));
        }
        root.children.push_back(std::move(items));

        ConfigNode dotted;
        dotted.key = "a.b";
        dotted.type = ConfigNode::Type::String;
        dotted.scalar = "dot";
        root.children.push_back(std::move(dotted));

        ConfigNode slashTilde;
        slashTilde.key = "a/b~c";
        slashTilde.type = ConfigNode::Type::String;
        slashTilde.scalar = "pointer";
        root.children.push_back(std::move(slashTilde));

        ConfigNode numericKey;
        numericKey.key = "123";
        numericKey.type = ConfigNode::Type::String;
        numericKey.scalar = "not array index";
        root.children.push_back(std::move(numericKey));

        ConfigNode quoted;
        quoted.key = "quote\"key";
        quoted.type = ConfigNode::Type::String;
        quoted.scalar = "quoted";
        root.children.push_back(std::move(quoted));

        return root;
    }

private slots:
    void test_objectPath()
    {
        ConfigNode root = makeTree();
        TreeModel model;
        model.render(root);

        QModelIndex server = model.index(0, 0);
        QModelIndex database = model.index(0, 0, server);
        QModelIndex host = model.index(0, 0, database);

        NodePath path = nodePathFromIndex(host);
        QCOMPARE(path.dotPath, QString("server.database.host"));
        QCOMPARE(path.jsonPointer, QString("/server/database/host"));
    }

    void test_arrayPath()
    {
        ConfigNode root = makeTree();
        TreeModel model;
        model.render(root);

        QModelIndex items = model.index(1, 0);
        QModelIndex thirdItem = model.index(2, 0, items);
        QModelIndex name = model.index(0, 0, thirdItem);

        NodePath path = nodePathFromIndex(name);
        QCOMPARE(path.dotPath, QString("items[2].name"));
        QCOMPARE(path.jsonPointer, QString("/items/2/name"));
    }

    void test_specialObjectKeys()
    {
        ConfigNode root = makeTree();
        TreeModel model;
        model.render(root);

        QCOMPARE(nodePathFromIndex(model.index(2, 0)).dotPath, QString("[\"a.b\"]"));
        QCOMPARE(nodePathFromIndex(model.index(3, 0)).jsonPointer, QString("/a~1b~0c"));
        QCOMPARE(nodePathFromIndex(model.index(4, 0)).dotPath, QString("[\"123\"]"));
        QCOMPARE(nodePathFromIndex(model.index(5, 0)).dotPath, QString("[\"quote\\\"key\"]"));
    }

    void test_invalidIndex()
    {
        NodePath path = nodePathFromIndex(QModelIndex());
        QVERIFY(path.segments.isEmpty());
        QVERIFY(path.dotPath.isEmpty());
        QVERIFY(path.jsonPointer.isEmpty());
    }
};

QTEST_MAIN(TestNodePath)
#include "test_node_path.moc"
