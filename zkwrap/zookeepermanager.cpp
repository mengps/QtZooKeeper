#define int_fast16_t int_fast16_t_
#define uint_fast16_t uint_fast16_t_
#include "zookeeper.h"
#undef int_fast16_t
#undef uint_fast16_t

#include "zookeepermanager.h"

#include <QDebug>
#include <QHash>
#include <QMetaEnum>
#include <QTimer>
#include <QThread>

using namespace ZooKeeper;

struct GetNodeValueParam
{
    QString path;
    std::function<void(ZooKeeperError, const QByteArray &)> callback;
};

struct GetChildrenNodeParam
{
    QString path;
    std::function<void(ZooKeeperError, const QStringList &)> callback;
};

struct WgetNodeValueParam
{
    QString path;
    std::function<void(ZooKeeperError, const QString &, const QByteArray &, ZooKeeperType, ZooKeeperState)> callback;
};

struct WgetChildrenNodeParam
{
    QString path;
    std::function<void(ZooKeeperError, const QString &, const QStringList &, ZooKeeperType, ZooKeeperState)> callback;
};

static QString error2string(ZooKeeperState state)
{
    switch (state) {
    case ZooKeeperState::AuthFailed:
        return QStringLiteral("ZooKeeper Error: 认证失败.");
        break;
    case ZooKeeperState::ExpiredSession:
        return QStringLiteral("ZooKeeper Error: 会话已经过期.");
        break;
    case ZooKeeperState::Closed:
        return QStringLiteral("ZooKeeper Error: 连接关闭.");
        break;
    case ZooKeeperState::NotConnected:
        return QStringLiteral("ZooKeeper Error: 未连接.");
        break;
    default:
        return QString("ZooKeeper Error: State %1.").arg(QMetaEnum::fromType<ZooKeeperState>().valueToKey(int(state)));
    }
}

class ZooKeeperNodePrivate
{
public:
    ZooKeeperNodePrivate() { }

    bool m_exists = false;
    QString m_path;
    QByteArray m_value;
    ZooKeeperNode::ZooKeeperNodeType m_type;
};


ZooKeeperNode *ZooKeeperNode::addChildNode(ZooKeeperNode::ZooKeeperNodeType type, const QString &name, const QByteArray &value)
{
    if (d->m_type == ZooKeeperNodeType::PersistentNode || d->m_type == ZooKeeperNodeType::SequenceNode) {
        return ZooKeeperManager::instance()->createNode(type, d->m_path + "/" + name, value);
    } else {
        return nullptr;
    }
}

ZooKeeperNode::ZooKeeperNode(QObject *parent)
    : QObject(parent)
{
    d = new ZooKeeperNodePrivate;
}

ZooKeeperNode::~ZooKeeperNode()
{
    if (d) delete d;
}

QString ZooKeeperNode::path() const
{
    return d->m_path;
}

ZooKeeperNode::ZooKeeperNodeType ZooKeeperNode::type() const
{
    return d->m_type;
}

void ZooKeeperNode::setValue(const QByteArray &value)
{
    ZooKeeperManager::instance()->setNodeValue(this, value);
}

QByteArray ZooKeeperNode::value() const
{
    return d->m_value;
}

void ZooKeeperNode::setExists(bool exists)
{
    if (d->m_exists != exists) {
        d->m_exists = exists;
        emit existsChanged();
    }
}

bool ZooKeeperNode::exists() const
{
    return d->m_exists;
}

class ZooKeeperManagerPrivate
{
public:
    static void processDisconnected();
    static void watcher(zhandle_t *zzh, int type, int state, const char *path, void* context);
    static void addAuthCompletion(int rc, const void *data);
    static void acreateCompletion(int rc, const char *name, const void *data);
    static void adeleteCompletion(int rc, const void *data);
    static void aexistsCompletion(int rc, const struct Stat *stat, const void *data);
    static void asetCompletion(int rc, const struct Stat *stat, const void *data);
    static void agetCompletion(int rc, const char *value, int value_len, const struct Stat *stat, const void *data);
    static void agetChildrenCompletion(int rc, const struct String_vector *strings, const void *data);
    static void wgetNodeValue(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);
    static void wgetChildrenNode(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);

    zhandle_t *m_zooHandle = nullptr;
    clientid_t m_clientId;
    int m_timeout = 30000;
    QString m_host = "";
    bool m_connected = false;
    QHash<QString, ZooKeeperNode *> m_nodePool;
};

void ZooKeeperManagerPrivate::processDisconnected()
{
    ZooKeeperManager *_this = ZooKeeperManager::instance();

    //断开连接
    _this->d_func()->m_connected = false;

    //清空节点池中的临时节点
    for (auto it = _this->d_func()->m_nodePool.begin(); it != _this->d_func()->m_nodePool.end(); it++) {
        it.value()->deleteLater();;
    }
    _this->d_func()->m_nodePool.clear();

    emit _this->disconnected();
}

void ZooKeeperManagerPrivate::watcher(zhandle_t *zzh, int type, int state, const char *path, void *context)
{
    ZooKeeperManager *_this = ZooKeeperManager::instance();

    if (ZooKeeperType(type) == ZooKeeperType::SessionEvent) {
        ZooKeeperState zkState = ZooKeeperState(state);
        switch (zkState) {
        case ZooKeeperState::Connected: {
            const clientid_t *id = zoo_client_id(zzh);
            if (_this->d_func()->m_clientId.client_id == 0 || _this->d_func()->m_clientId.client_id != id->client_id) {
                _this->d_func()->m_clientId = *id;
            }
            _this->d_func()->m_connected = true;
            emit _this->connected();
            qDebug() << u8"ZooKeeper Connect Success: id =" << id->client_id;
            break;
        }
        case ZooKeeperState::Connecting: break;
        case ZooKeeperState::AuthFailed:
        case ZooKeeperState::ExpiredSession:
        case ZooKeeperState::Closed:
        case ZooKeeperState::NotConnected:
        default:
            zookeeper_close(zzh);
            emit _this->error(error2string(zkState));
            processDisconnected();
            break;
        }
    }

    emit _this->watcher(ZooKeeperType(type), ZooKeeperState(state), path);

    qDebug() << __func__ << ZooKeeperType(type) << ZooKeeperState(state) << "path =" << path;
}

void ZooKeeperManagerPrivate::addAuthCompletion(int rc, const void *)
{
    ZooKeeperManager *_this = ZooKeeperManager::instance();
    ZooKeeperError error = ZooKeeperError(rc);

    emit _this->addAuthFinished(error);

    qDebug().noquote() << __func__ << "rc =" << error;
}

void ZooKeeperManagerPrivate::acreateCompletion(int rc, const char *name, const void *data)
{
    ZooKeeperManager *_this = ZooKeeperManager::instance();

    ZooKeeperError error = ZooKeeperError(rc);

    QString path = QString(name);
    QString pathOld = *(QString *)data;
    if (!pathOld.isEmpty()) {
        ZooKeeperNode *node = nullptr;

        if (path.isEmpty()) path = pathOld;

        if (path != pathOld) {
            if (_this->d_func()->m_nodePool.contains(pathOld)) {
                node = _this->d_func()->m_nodePool[pathOld];
                node->d->m_path = path;
                _this->d_func()->m_nodePool[path] = node;
                _this->d_func()->m_nodePool.remove(pathOld);
            }
        } else {
            if (_this->d_func()->m_nodePool.contains(path)) {
                node = _this->d_func()->m_nodePool[path];
            }
        }

        if (error == ZooKeeperError::NoError) {
            node->setExists(true);
            emit node->created();
        } else if (error == ZooKeeperError::NodeExists) {
            node->setExists(true);
        } else {
            _this->d_func()->m_nodePool.remove(path);
            node->deleteLater();
        }
    }

    delete (QString *)data;

    emit _this->createNodeFinished(error, path);

    qDebug().noquote() << __func__ << "[" + path + "]" << "rc =" << error << "data =" << pathOld;
}

void ZooKeeperManagerPrivate::adeleteCompletion(int rc, const void *data)
{
    ZooKeeperManager *_this = ZooKeeperManager::instance();
    ZooKeeperError error = ZooKeeperError(rc);
    QString path = *(QString *)data;

    delete (QString *)data;

    emit _this->deleteNodeFinished(error, path);

    qDebug() << __func__ << "[" + path + "]" << "rc =" << error;
}

void ZooKeeperManagerPrivate::aexistsCompletion(int rc, const Stat *stat, const void *data)
{
    ZooKeeperManager *_this = (ZooKeeperManager *)(data);
    ZooKeeperError error = ZooKeeperError(rc);
    QString path = *(QString *)data;

    delete (QString *)data;

    emit _this->existsNodeFinished(error, path);

    qDebug() << __func__ << "[" + path + "]" << "rc =" << error;
}

void ZooKeeperManagerPrivate::asetCompletion(int rc, const Stat *stat, const void *data)
{
    ZooKeeperManager *_this = ZooKeeperManager::instance();
    ZooKeeperError error = ZooKeeperError(rc);
    QString path = *(QString *)data;

    delete (QString *)data;

    emit _this->setNodeValueFinished(error, path);

    qDebug() << __func__ << "[" + path + "]" << "rc =" << error;
}

void ZooKeeperManagerPrivate::agetCompletion(int rc, const char *value, int value_len, const Stat *stat, const void *data)
{
    ZooKeeperManager *_this = ZooKeeperManager::instance();
    ZooKeeperError error = ZooKeeperError(rc);

    auto param = reinterpret_cast<const GetNodeValueParam *>(data);

    QString path = param->path;
    QByteArray nodeValue(value, value_len);
    ZooKeeperNode *node = _this->d_func()->m_nodePool[path];

    if (param->callback) {
        param->callback(error, nodeValue);
    }

    if (error == ZooKeeper::ZooKeeperError::NoError) {
        if (node->d->m_value != nodeValue) {
            node->d->m_value = nodeValue;
            emit node->valueChanged();
        }
        node->setExists(true);
    } else if (error == ZooKeeper::ZooKeeperError::NoNode) {
        node->setExists(false);
    }

    delete param;

    qDebug() << __func__ << "rc =" << error << "path =" << path << "value =" << nodeValue;
}

void ZooKeeperManagerPrivate::agetChildrenCompletion(int rc, const String_vector *strings, const void *data)
{
    ZooKeeperManager *_this = ZooKeeperManager::instance();
    ZooKeeperError error = ZooKeeperError(rc);

    auto param = reinterpret_cast<const GetChildrenNodeParam *>(data);
    auto path = param->path;

    QStringList children;
    if (error == ZooKeeperError::NoError) {
        for (int i = 0; i < strings->count; i++) {
            children.append(strings->data[i]);
        }
    }

    if (param->callback) {
        param->callback(error, children);
    } else {
        emit _this->getChildrenNodeFinished(error, path, children);
    }

    delete param;

    qDebug() << __func__ << "rc =" << error << "path =" << path << "children =" << children;
}

void ZooKeeperManagerPrivate::wgetNodeValue(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{
    ZooKeeperManager *_this = ZooKeeperManager::instance();
    auto param = reinterpret_cast<const WgetNodeValueParam *>(watcherCtx);

    int len = 65535;
    char buffer[65535];
    Stat stat;

    auto error = ZooKeeperError(zoo_wget(_this->d_func()->m_zooHandle, path
                                         , &ZooKeeperManagerPrivate::wgetNodeValue
                                         , watcherCtx, buffer, &len, &stat));

    if (param->callback) {
        param->callback(error, param->path, QByteArray(buffer), ZooKeeperType(type), ZooKeeperState(state));
    } else {
        emit _this->wgetNodeValueFinished(error, param->path, QByteArray(buffer), ZooKeeperType(type), ZooKeeperState(state));
    }

    qDebug() << __func__ << zh << ZooKeeperType(type) << ZooKeeperState(state) << path << buffer;
}

void ZooKeeperManagerPrivate::wgetChildrenNode(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{
    ZooKeeperManager *_this = ZooKeeperManager::instance();
    auto param = reinterpret_cast<const WgetChildrenNodeParam *>(watcherCtx);

    String_vector strings;
    auto error = ZooKeeperError(zoo_wget_children(_this->d_func()->m_zooHandle
                                                  , path, &ZooKeeperManagerPrivate::wgetChildrenNode
                                                  , watcherCtx, &strings));

    QStringList children;
    if (error == ZooKeeperError::NoError) {
        for (int i = 0; i < strings.count; i++) {
            children.append(strings.data[i]);
        }
    }

    if (param->callback) {
        param->callback(ZooKeeperError(error), param->path, children, ZooKeeperType(type), ZooKeeperState(state));
    } else {
        emit _this->wgetChildrenNodeFinished(error, param->path, children, ZooKeeperType(type), ZooKeeperState(state));
    }

    qDebug() << __func__ << zh << ZooKeeperType(type) << ZooKeeperState(state) << path << children;
}

ZooKeeperManager::~ZooKeeperManager()
{
    quit();
}

ZooKeeperManager *ZooKeeperManager::instance()
{
    static ZooKeeperManager manager;
    return &manager;
}

qint64 ZooKeeperManager::zooKeeperId() const
{
    Q_D(const ZooKeeperManager);

    return d->m_clientId.client_id;
}

void ZooKeeperManager::initialize(const QString &host, int timeout)
{
    Q_D(ZooKeeperManager);

    d->m_host = host;
    d->m_timeout = timeout;
    d->m_clientId.client_id = 0;
    d->m_zooHandle = zookeeper_init(host.toLatin1().constData(), &ZooKeeperManagerPrivate::watcher,
                                    timeout, &d->m_clientId, this, 0);
    QTimer::singleShot(timeout, [this, d]{
        ZooKeeperState state = ZooKeeperState(zoo_state(d->m_zooHandle));
        if (state != ZooKeeperState::Connected && state != ZooKeeperState::Connecting)
            emit error(QStringLiteral("ZooKeeper Error: 连接超时."));
    });
}

void ZooKeeperManager::addAuth(const QString &scheme, const QString &cert)
{
    Q_D(ZooKeeperManager);

    if (scheme == QStringLiteral("digest")) {
        auto stdstr = cert.toStdString();
        zoo_add_auth(d->m_zooHandle, "digest", stdstr.c_str(), stdstr.length(), &ZooKeeperManagerPrivate::addAuthCompletion, nullptr);
    }
}

void ZooKeeperManager::setDebugLevel(ZooKeeperManager::ZooKeeperDebugLevel level)
{
    zoo_set_debug_level(ZooLogLevel(level));
}

ZooKeeperNode *ZooKeeperManager::createNode(ZooKeeperNode::ZooKeeperNodeType type, const QString &path, const QByteArray &value
                                            , ZooKeeperError *error)
{
    Q_D(ZooKeeperManager);

    int flag = 0;
    switch (type) {
    case ZooKeeperNode::ZooKeeperNodeType::PersistentNode:
        break;
    case ZooKeeperNode::ZooKeeperNodeType::SequenceNode:
        flag = ZOO_SEQUENCE;
        break;
    case ZooKeeperNode::ZooKeeperNodeType::EphemeralNode:
        flag = ZOO_EPHEMERAL;
        break;
    case ZooKeeperNode::ZooKeeperNodeType::SequenceAndEphemeralNode:
        flag = ZOO_SEQUENCE | ZOO_EPHEMERAL;
        break;
    default:
        break;
    }

    if (!d->m_nodePool.contains(path)) {
        int ret = zoo_acreate(d->m_zooHandle, path.toLatin1().constData()
                              , value.toStdString().c_str(), int(value.toStdString().length())
                              , &ZOO_OPEN_ACL_UNSAFE, flag, &ZooKeeperManagerPrivate::acreateCompletion, new QString(path));
        if (error)
            *error = ZooKeeperError(ret);

        if (ZooKeeperError(ret) == ZooKeeperError::NoError) {
            ZooKeeperNode *node = new ZooKeeperNode(this);
            node->d->m_path = path;
            node->d->m_value = value;
            node->d->m_type = type;
            d->m_nodePool[path] = node;
            return node;
        } else {
            return nullptr;
        }
    } else {
        return d->m_nodePool[path];
    }
}

ZooKeeperNode *ZooKeeperManager::createNodeSync(ZooKeeperNode::ZooKeeperNodeType type, const QString &path, const QByteArray &value
                                                , ZooKeeperError *error)
{
    Q_D(ZooKeeperManager);

    int flag = 0;
    switch (type) {
    case ZooKeeperNode::ZooKeeperNodeType::PersistentNode:
        break;
    case ZooKeeperNode::ZooKeeperNodeType::SequenceNode:
        flag = ZOO_SEQUENCE;
        break;
    case ZooKeeperNode::ZooKeeperNodeType::EphemeralNode:
        flag = ZOO_EPHEMERAL;
        break;
    case ZooKeeperNode::ZooKeeperNodeType::SequenceAndEphemeralNode:
        flag = ZOO_SEQUENCE | ZOO_EPHEMERAL;
        break;
    default:
        break;
    }

    int len = 1024;
    char newPath[1024] = { '\0' };

    if (!d->m_nodePool.contains(path)) {
        int ret = zoo_create(d->m_zooHandle, path.toLatin1().constData()
                              , value.toStdString().c_str(), int(value.toStdString().length())
                              , &ZOO_OPEN_ACL_UNSAFE, flag, newPath, len);
        if (error)
            *error = ZooKeeperError(ret);

        qDebug() << __func__ << "rc =" << ZooKeeperError(ret) << "path =" << QString(newPath);

        if (ZooKeeperError(ret) == ZooKeeperError::NoError) {
            ZooKeeperNode *node = new ZooKeeperNode(this);
            node->d->m_exists = true;
            node->d->m_path = QString(newPath);
            node->d->m_value = value;
            node->d->m_type = type;
            d->m_nodePool[newPath] = node;
            d->m_nodePool.remove(path);
            emit node->created();
            return node;
        } else {
            return nullptr;
        }
    } else {
        return d->m_nodePool[path];
    }
}

ZooKeeperError ZooKeeperManager::deleteNode(ZooKeeperNode *node)
{
    return deleteNode(node->path());
}

ZooKeeperError ZooKeeperManager::deleteNode(const QString &path)
{
    Q_D(ZooKeeperManager);

    if (d->m_nodePool.contains(path)) {
        d->m_nodePool[path]->deleteLater();
        d->m_nodePool.remove(path);
    }

    int ret = zoo_adelete(d->m_zooHandle, path.toLatin1().constData()
                          , -1, &ZooKeeperManagerPrivate::adeleteCompletion, new QString(path));

    return ZooKeeperError(ret);
}

ZooKeeper::ZooKeeperError ZooKeeperManager::deleteNodeSync(ZooKeeperNode *node)
{
    return deleteNodeSync(node->path());
}

ZooKeeper::ZooKeeperError ZooKeeperManager::deleteNodeSync(const QString &path)
{
    Q_D(ZooKeeperManager);

    if (d->m_nodePool.contains(path)) {
        d->m_nodePool[path]->deleteLater();
        d->m_nodePool.remove(path);
    }

    int ret = zoo_delete(d->m_zooHandle, path.toUtf8().constData(), -1);

    return ZooKeeperError(ret);
}

ZooKeeperError ZooKeeperManager::existsNode(const QString &path)
{
    Q_D(ZooKeeperManager);

    int ret = zoo_aexists(d->m_zooHandle, path.toLatin1().constData(), 0
                          , &ZooKeeperManagerPrivate::aexistsCompletion, new QString(path));

    return ZooKeeperError(ret);
}

ZooKeeper::ZooKeeperError ZooKeeperManager::existsNodeSync(const QString &path)
{
    Q_D(ZooKeeperManager);

    Stat stat;

    int ret = zoo_exists(d->m_zooHandle, path.toLatin1().constData(), 0, &stat);

    qDebug() << __func__ << ZooKeeperError(ret);

    return ZooKeeperError(ret);
}

ZooKeeper::ZooKeeperError ZooKeeperManager::setNodeValue(ZooKeeperNode *node, const QByteArray &value)
{
    return setNodeValue(node->path(), value);
}

ZooKeeperError ZooKeeperManager::setNodeValue(const QString &path, const QByteArray &value)
{
    Q_D(ZooKeeperManager);

    /**
     * @warnning QThread::msleep 避免设置过快
     */
    QThread::msleep(10);
    int ret = zoo_aset(d->m_zooHandle, path.toLatin1().constData()
                       , value.toStdString().c_str(), int(value.toStdString().length())
                       , -1, &ZooKeeperManagerPrivate::asetCompletion, new QString(path));

    return ZooKeeperError(ret);
}

ZooKeeper::ZooKeeperError ZooKeeperManager::setNodeValueSync(ZooKeeperNode *node, const QByteArray &value)
{
    return setNodeValueSync(node->path(), value);
}

ZooKeeper::ZooKeeperError ZooKeeperManager::setNodeValueSync(const QString &path, const QByteArray &value)
{
    Q_D(ZooKeeperManager);

    int ret = zoo_set(d->m_zooHandle, path.toLatin1().constData()
                      , value.toStdString().c_str(), int(value.toStdString().length()), -1);

    qDebug() << __func__ << ZooKeeperError(ret);

    return ZooKeeperError(ret);
}

ZooKeeperNode * ZooKeeperManager::getNodeValue(const QString &path, const std::function<void (ZooKeeper::ZooKeeperError, const QByteArray &)> &callback
                                              , ZooKeeperError *error)
{
    Q_D(ZooKeeperManager);

    GetNodeValueParam *param = new GetNodeValueParam { path, callback };

    ZooKeeperNode *node = nullptr;
    if (d->m_nodePool.contains(path)) {
        node = d->m_nodePool[path];
    } else {
        node = new ZooKeeperNode(this);
        node->d->m_path = path;
        d->m_nodePool[path] = node;
    }

    int ret = zoo_aget(d->m_zooHandle, path.toLatin1().constData(), 0, &ZooKeeperManagerPrivate::agetCompletion, param);

    if (error)
        *error = ZooKeeperError(ret);

    return node;
}

ZooKeeperNode *ZooKeeperManager::getNodeValue(const QString &path, ZooKeeperError *error)
{
    Q_D(ZooKeeperManager);

    GetNodeValueParam *param = new GetNodeValueParam;
    param->path = path;

    ZooKeeperNode *node = nullptr;
    if (d->m_nodePool.contains(path)) {
        node = d->m_nodePool[path];
    } else {
        node = new ZooKeeperNode(this);
        node->d->m_path = path;
        d->m_nodePool[path] = node;
    }

    int ret = zoo_aget(d->m_zooHandle, path.toLatin1().constData(), 0
                       , &ZooKeeperManagerPrivate::agetCompletion, param);

    if (error)
        *error = ZooKeeperError(ret);

    return node;
}

ZooKeeperNode *ZooKeeperManager::getNodeValueSync(const QString &path, ZooKeeperError *error)
{
    Q_D(ZooKeeperManager);

    ZooKeeperNode *node = nullptr;
    if (d->m_nodePool.contains(path)) {
        node = d->m_nodePool[path];
    } else {
        node = new ZooKeeperNode(this);
        node->d->m_path = path;
        d->m_nodePool[path] = node;
    }

    int len = 65535;
    char buffer[65535];
    Stat stat;

    int ret = zoo_get(d->m_zooHandle, path.toLatin1().constData(), 0, buffer, &len, &stat);

    if (error)
        *error = ZooKeeperError(ret);

    if (ZooKeeperError(ret) == ZooKeeperError::NoError) {
        auto value = QByteArray(buffer, len);
        if (node->d->m_value != value) {
            node->d->m_value = value;
            emit node->valueChanged();
        }
        node->setExists(true);
    } else if (ZooKeeperError(ret) == ZooKeeperError::NoNode) {
        node->setExists(false);
    }

    qDebug() << __func__ << ZooKeeperError(ret) << "value =" << QString(node->value());

    return node;
}

ZooKeeper::ZooKeeperError ZooKeeperManager::getChildrenNode(const QString &path, bool watch, const std::function<void (ZooKeeper::ZooKeeperError, const QStringList &)> &callback)
{
    Q_D(ZooKeeperManager);

    GetChildrenNodeParam *param = new GetChildrenNodeParam { path, callback };

    int ret = zoo_aget_children(d->m_zooHandle, path.toLatin1().constData(), watch
                       , &ZooKeeperManagerPrivate::agetChildrenCompletion, param);

    return ZooKeeperError(ret);
}

ZooKeeper::ZooKeeperError ZooKeeperManager::getChildrenNode(const QString &path, bool watch)
{
    Q_D(ZooKeeperManager);

    GetChildrenNodeParam *param = new GetChildrenNodeParam { path };

    int ret = zoo_aget_children(d->m_zooHandle, path.toLatin1().constData(), watch
                       , &ZooKeeperManagerPrivate::agetChildrenCompletion, param);

    return ZooKeeperError(ret);
}

ZooKeeper::ZooKeeperError ZooKeeperManager::getChildrenNodeSync(const QString &path, bool watch, QStringList *children)
{
    Q_D(ZooKeeperManager);

    String_vector strings;
    ZooKeeperError error = ZooKeeperError(zoo_get_children(d->m_zooHandle, path.toLatin1().constData(), watch, &strings));

    children->clear();
    if (error == ZooKeeperError::NoError) {
        for (int i = 0; i < strings.count; i++) {
            children->append(strings.data[i]);
        }
    }

    qDebug() << __func__ << error << "children =" << *children;

    return error;
}

ZooKeeperNode *ZooKeeperManager::wgetNodeValue(const QString &path, const std::function<void (ZooKeeper::ZooKeeperError, const QString &, const QByteArray &, ZooKeeper::ZooKeeperType, ZooKeeper::ZooKeeperState)> &callback)
{
    Q_D(ZooKeeperManager);

    WgetNodeValueParam *param = new WgetNodeValueParam { path, callback };

    ZooKeeperNode *node = nullptr;
    if (d->m_nodePool.contains(path)) {
        node = d->m_nodePool[path];
    } else {
        node = new ZooKeeperNode(this);
        node->d->m_path = path;
        d->m_nodePool[path] = node;
    }

    int len = 65535;
    char buffer[65535];
    Stat stat;

    int ret = zoo_wget(d->m_zooHandle, path.toLatin1().constData(), &ZooKeeperManagerPrivate::wgetNodeValue, param, buffer, &len, &stat);

    if (ZooKeeperError(ret) == ZooKeeperError::NoError) {
        auto value = QByteArray(buffer, len);
        if (node->d->m_value != value) {
            node->d->m_value = value;
            emit node->valueChanged();
        }
        node->setExists(true);
    } else if (ZooKeeperError(ret) == ZooKeeperError::NoNode) {
        node->setExists(false);
    }

    qDebug() << __func__ << ZooKeeperError(ret) << "value =" << QString(node->value());

    return node;
}

ZooKeeper::ZooKeeperError ZooKeeperManager::wgetChildrenNode(const QString &path, QStringList *children, const std::function<void (ZooKeeper::ZooKeeperError, const QString &, const QStringList &, ZooKeeper::ZooKeeperType, ZooKeeper::ZooKeeperState)> &callback)
{
    Q_D(ZooKeeperManager);

    WgetChildrenNodeParam *param = new WgetChildrenNodeParam { path, callback };

    String_vector strings;
    ZooKeeperError error = ZooKeeperError(zoo_wget_children(d->m_zooHandle, path.toLatin1().constData(), &ZooKeeperManagerPrivate::wgetChildrenNode, param, &strings));

    children->clear();
    if (error == ZooKeeperError::NoError) {
        for (int i = 0; i < strings.count; i++) {
            children->append(strings.data[i]);
        }
    }

    qDebug() << __func__ << error << "children =" << *children;

    return error;
}

void ZooKeeperManager::quit()
{
    Q_D(ZooKeeperManager);

    if (d->m_zooHandle) {
        d->m_connected = false;
        zookeeper_close(d->m_zooHandle);
        d->m_zooHandle = nullptr;
    }
}

bool ZooKeeperManager::isConnected()
{
    Q_D(ZooKeeperManager);

    return d->m_connected;
}

ZooKeeperNode *ZooKeeperManager::findNode(const QString &path)
{
    Q_D(ZooKeeperManager);

    if (d->m_nodePool.contains(path)) {
        return d->m_nodePool[path];
    } else {
        /*! TODO */
        return nullptr;
    }
}

ZooKeeperManager::ZooKeeperManager(QObject *parent)
    : QObject(parent)
    , d_ptr(new ZooKeeperManagerPrivate)
{
    qRegisterMetaType<ZooKeeperError>("ZooKeeperError");
    qRegisterMetaType<ZooKeeperType>("ZooKeeperType");
    qRegisterMetaType<ZooKeeperState>("ZooKeeperState");

    setDebugLevel(ZooKeeperDebugLevel::Warn);
    zoo_deterministic_conn_order(1);
}
