#ifndef ZOOKEEPERMANAGER_H
#define ZOOKEEPERMANAGER_H

#include <QObject>

namespace ZooKeeper
{
    Q_NAMESPACE

    enum class ZooKeeperError
    {
        NoError = 0, /*!< 一切正常 */
        /** 系统和服务器端错误。
         * 这永远不会被服务器抛出，除了
         * 表示范围。特别是比这更大的错误代码
         * 值，但小于 {@link #api_error}，是系统错误。
        */
        SystemError = -1,
        RuntimeInconsistency = -2, /*!< 发现运行时不一致 */
        DataInconsistency = -3, /*!< 发现数据不一致 */
        ConnectionLoss = -4, /*!< 与服务器的连接已丢失 */
        MarshallingError = -5, /*!< 编组或解组数据时出错 */
        Unimplemented = -6, /*!< 操作未实现 */
        OperationTimeout = -7, /*!< 操作超时 */
        BadArguments = -8, /*!< 无效参数 */
        InvliadState = -9, /*!< 无效 zhandle 状态 */
        /** API 错误。
         * 这永远不会被服务器抛出，除了
         * 表示范围。特别是比这更大的错误代码
         * 值是 API 错误（而小于此值的值表示
         * {@link #system_error}）。
         */
        ApiError = -100,
        NoNode = -101, /*!< 节点不存在 */
        NoAuth = -102, /*!< 未认证 */
        BadVersion = -103, /*!< 版本冲突 */
        NoChildrenForEphemeral = -108, /*!< 临时节点可能没有子节点 */
        NodeExists = -110, /*!< 节点已经存在 */
        NotEmpty = -111, /*!< 该节点有孩子 */
        SessionExpired = -112, /*!< 会话已被服务器过期 */
        InvalidCallback = -113, /*!< 指定的回调无效 */
        InvalidAcl = -114, /*!< 指定的 ACL 无效 */
        AuthFailed = -115, /*!< 客户端认证失败 */
        Closure = -116, /*!< ZooKeeper 正在关闭 */
        Nothing = -117, /*!< (不是错误)没有服务器响应处理 */
        SessionMoved = -118 /*!< 会话移动到另一台服务器，因此操作被忽略 */
    };

    enum class ZooKeeperType
    {
        CreatedEvent = 1,
        DeletedEvent = 2,
        ChangedEvent = 3,
        ChildEvent = 4,
        SessionEvent = -1,
        NotWatchingEvent = -2
    };

    enum class ZooKeeperState
    {
        Closed = 0,
        Connecting = 1,
        Assoctating = 2,
        Connected = 3,
        ExpiredSession = -112,
        AuthFailed = -113,
        NotConnected = 999
    };

    Q_ENUM_NS(ZooKeeperError);
    Q_ENUM_NS(ZooKeeperType);
    Q_ENUM_NS(ZooKeeperState);
};

QT_FORWARD_DECLARE_CLASS(ZooKeeperManagerPrivate);

QT_FORWARD_DECLARE_CLASS(ZooKeeperNodePrivate);

class ZooKeeperNode : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool exists READ exists WRITE setExists NOTIFY existsChanged)
    Q_PROPERTY(QByteArray value READ value WRITE setValue NOTIFY valueChanged)

public:
    enum class ZooKeeperNodeType
    {
        //持久节点
        PersistentNode = 1,
        //顺序持久节点
        SequenceNode = 2,
        //临时节点
        EphemeralNode = 4,
        //顺序临时节点
        SequenceAndEphemeralNode = SequenceNode | EphemeralNode
    };
    Q_ENUM(ZooKeeperNodeType);

    ~ZooKeeperNode();

    QString path() const;

    ZooKeeperNodeType type() const;

    void setValue(const QByteArray &value);
    QByteArray value() const;

    void setExists(bool exists);
    bool exists() const;

    ZooKeeperNode *addChildNode(ZooKeeperNodeType type, const QString &name, const QByteArray &value);

signals:
    void created();
    void valueChanged();
    void existsChanged();

private:
    explicit ZooKeeperNode(QObject *parent = nullptr);

    ZooKeeperNodePrivate *d = nullptr;

    friend class ZooKeeperManager;
    friend class ZooKeeperManagerPrivate;
};

class ZooKeeperManager : public QObject
{
    Q_OBJECT

public:
    enum class ZooKeeperDebugLevel
    {
        Error = 1,
        Warn = 2,
        Info = 3,
        Debug = 4
    };
    Q_ENUM(ZooKeeperDebugLevel);

    ~ZooKeeperManager();
    static ZooKeeperManager *instance();

    qint64 zooKeeperId() const;

    void initialize(const QString &host, int timeout = 30000);

    void addAuth(const QString &scheme, const QString &cert);

    void setDebugLevel(ZooKeeperDebugLevel level);

    void quit();

    bool isConnected();

    ZooKeeperNode *findNode(const QString &path);

    ZooKeeperNode *createNode(ZooKeeperNode::ZooKeeperNodeType type, const QString &path, const QByteArray &value = ""
                              , ZooKeeper::ZooKeeperError *error = nullptr);
    ZooKeeperNode *createNodeSync(ZooKeeperNode::ZooKeeperNodeType type, const QString &path, const QByteArray &value = ""
                                  , ZooKeeper::ZooKeeperError *error = nullptr);

    ZooKeeper::ZooKeeperError deleteNode(ZooKeeperNode *node);
    ZooKeeper::ZooKeeperError deleteNode(const QString &path);
    ZooKeeper::ZooKeeperError deleteNodeSync(ZooKeeperNode *node);
    ZooKeeper::ZooKeeperError deleteNodeSync(const QString &path);

    ZooKeeper::ZooKeeperError existsNode(const QString &path);
    ZooKeeper::ZooKeeperError existsNodeSync(const QString &path);

    ZooKeeper::ZooKeeperError setNodeValue(ZooKeeperNode *node, const QByteArray &value);
    ZooKeeper::ZooKeeperError setNodeValue(const QString &path, const QByteArray &value);
    ZooKeeper::ZooKeeperError setNodeValueSync(ZooKeeperNode *node, const QByteArray &value);
    ZooKeeper::ZooKeeperError setNodeValueSync(const QString &path, const QByteArray &value);

    ZooKeeperNode *getNodeValue(const QString &path, const std::function<void (ZooKeeper::ZooKeeperError, const QByteArray &)> &callback
                                , ZooKeeper::ZooKeeperError *error = nullptr);
    ZooKeeperNode *getNodeValue(const QString &path, ZooKeeper::ZooKeeperError *error = nullptr);
    ZooKeeperNode *getNodeValueSync(const QString &path, ZooKeeper::ZooKeeperError *error = nullptr);

    ZooKeeper::ZooKeeperError getChildrenNode(const QString &path, bool watch, const std::function<void (ZooKeeper::ZooKeeperError, const QStringList &)> &callback);
    ZooKeeper::ZooKeeperError getChildrenNode(const QString &path, bool watch);
    ZooKeeper::ZooKeeperError getChildrenNodeSync(const QString &path, bool watch, QStringList *children);

    ZooKeeperNode *wgetNodeValue(const QString &path, const std::function<void(ZooKeeper::ZooKeeperError, const QString &path, const QByteArray &value
                                                                               , ZooKeeper::ZooKeeperType, ZooKeeper::ZooKeeperState)> &callback);

    ZooKeeper::ZooKeeperError wgetChildrenNode(const QString &path, QStringList *children
                                               , const std::function<void(ZooKeeper::ZooKeeperError, const QString &path, const QStringList &children
                                               , ZooKeeper::ZooKeeperType, ZooKeeper::ZooKeeperState)> &callback);

signals:
    void error(const QString &errorString);
    void connected();
    void disconnected();

    void watcher(ZooKeeper::ZooKeeperType type, ZooKeeper::ZooKeeperState state, const QString &path);
    void addAuthFinished(ZooKeeper::ZooKeeperError code);
    void createNodeFinished(ZooKeeper::ZooKeeperError code, const QString &path);
    void deleteNodeFinished(ZooKeeper::ZooKeeperError code, const QString &path);
    void existsNodeFinished(ZooKeeper::ZooKeeperError code, const QString &path);
    void setNodeValueFinished(ZooKeeper::ZooKeeperError code, const QString &path);
    void getChildrenNodeFinished(ZooKeeper::ZooKeeperError code, const QString &path, const QStringList &children);
    void wgetNodeValueFinished(ZooKeeper::ZooKeeperError code, const QString &path, const QByteArray &value
                               , ZooKeeper::ZooKeeperType type, ZooKeeper::ZooKeeperState state);
    void wgetChildrenNodeFinished(ZooKeeper::ZooKeeperError code, const QString &path, const QStringList &children
                                  , ZooKeeper::ZooKeeperType type, ZooKeeper::ZooKeeperState state);

private:
    ZooKeeperManager(QObject *parent = nullptr);

    QScopedPointer<ZooKeeperManagerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(ZooKeeperManager);
};

#endif // ZOOKEEPERMANAGER_H
