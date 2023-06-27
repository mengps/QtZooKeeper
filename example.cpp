#include "zookeepermanager.h"
#include <QCoreApplication>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    ZooKeeperManager::instance()->initialize("192.168.0.33:2181,192.168.0.33:2182,192.168.0.33:2183");
    ZooKeeperManager::instance()->addAuth("digest", "super:admin");

    return app.exec();
}
