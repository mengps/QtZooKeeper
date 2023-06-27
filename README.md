# QtZooKeeper

  `QtZooKeeper` 是基于 Qt 的 ZooKeeper Client 包装库。

  它简化了 ZooKeeper Client C API 的使用。

  `ZooKeeper` 地址：https://github.com/apache/zookeeper

----

### 如何构建

 1. 构建 `ZooKeeper` 动态库, 构建文件 `zookeeper/ZooKeeper.pro`。

 2. 包含 `zkwrap/zkwrap.pri` 即可使用。

---

### 如何使用

ZooKeeperManager 被实现为单例(方便回调, 如果觉得不便可自行修改)。

使用相当简单：

```C++
ZooKeeperManager::instance()->initialize("192.168.0.33:2181,192.168.0.33:2182,192.168.0.33:2183");
ZooKeeperManager::instance()->addAuth("digest", "super:admin");
```

异步回调/监听回调有两种形式，一种 Qt Signal，另一种则为 std::function。

----

### 注意

并没有包括所有 C API, 仅仅实现了部分常用的。

有需要或有兴趣欢迎贡献代码{风格保持一致即可}。

---

### 许可证

   使用 `MIT LICENSE`

---

### 开发环境

  Windows 11，Qt 5.15.2
