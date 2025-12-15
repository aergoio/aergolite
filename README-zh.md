<p align="right"><a href="README.md">English</a></p>
<p align="center"><img width="65%" src="https://user-images.githubusercontent.com/7624275/92685476-2390b800-f30e-11ea-9edc-980b0e66c0ad.png" alt="AergoLite"></p>

<h1 align="center">支持区块链的SQLite数据库</h1>

<blockquote align="center"><p><em>以最简单的方法在应用或设备上部署用于关系数据存储的区块链</em></p></blockquote>

AergoLite允许我们拥有一个由私有轻量级区块链保护的可复制的SQLite数据库。

每个应用程序都有数据库的本地副本。

新的数据库交易将分发给所有对等方，一旦它们对执行交易达成共识，所有节点将执行交易。 由于这些交易的执行顺序相同，因此所有节点都具有相同的结果。

应用程序也可以在脱机时写入本地数据库。 重新建立连接后，数据库交易将存储在本地队列中并发送到网络。 离线修改后，该应用程序将读取数据库的新状态，并且它可以检查离线交易是否已由全局共识处理。 如果被拒绝，数据库将返回到先前的状态。

AergoLite使用**专门针对**资源受限设备**的特殊区块链技术**。

共识协议使用**可验证随机函数（VRF）**来确定哪个节点将产生下一个块，并且这些节点无法预知即将选择了哪个节点。 使其可以抵御拒绝服务（DoS）攻击。

AergoLite具有**不可篡改**的特性。 一旦节点在新区块上达成共识并确认交易，就无法改变。 此外，如果没有要处理的交易，则无需创建新块（与概率确定性不同）。

只需要最后一个块即可检查区块链和数据库状态完整性，因此节点无需保留并验证所有块和交易的历史记录。
也可以设置某些节点以保留所有历史记录，以进行审核。

AergoLite**数据库的状态是哈希形式的**。 这使节点可以检查它们在数据库上的内容是否完全相同，可以防止对数据库文件的有意修改，还可以用作完整性检查以检测存储介质上是否有错误。

仅使用每个新块上的已修改页面来更新此最终哈希。 它不需要加载整个数据库来计算新状态。 完整性检查也仅在加载新的数据库页面时进行。 这大大提高了数据库性能。

最终的解决方案不需要大量的磁盘存储，可以节省处理器消耗和内存。

网络流量也很小，以减少能耗。 仅当有新的交易时才传输新的数据包。

这项技术使我们能够在IoT和移动设备上运行真正的私有区块链。

AergoLite也易于使用。 您无需知道区块链的工作原理。

支持的系统：

* Mac
* Linux
* Windows
* Android
* iOS
* OpenWrt

支持的编程语言：

* C
* C++
* Java
* Javascript (Node.js)
* Python
* .Net (C# 和 VB)
* Ruby
* Swift
* Lua
* Go

未来还会有其他系统和编程语言被支持。

现有包装类支持其中大多数语言。


## 预编译二进制文件

请查看 [这里](https://github.com/aergoio/aergolite/releases)


## Docker 镜像

* [Base](https://hub.docker.com/r/aergo/aergolite) (包含库和SQLite Shell，也可以用于添加C / C ++应用程序)
* [Python](https://hub.docker.com/r/aergo/aergolite-python)
* [Node.js](https://hub.docker.com/r/aergo/aergolite-nodejs)

在本地生成图像命令：

```
make docker
```


## 编译安装

### Linux 和 Mac

首先用命令安装需要的工具：

**Ubuntu 和 Debian**

```
sudo apt-get install git gcc make automake libtool libreadline-dev
```

**CentOS**

```
sudo yum install git gcc make automake libtool readline-devel
```

**Alpine**

```
sudo apk add git gcc make automake libtool readline-dev
```

**Mac**

```
brew install automake libtool readline
```

然后将以下命令复制并粘贴到终端上运行：

```
# 安装 libuv

git clone --depth=1 https://github.com/libuv/libuv
cd libuv
./autogen.sh
./configure
make
sudo make install
sudo ldconfig
cd ..

# 安装 binn

git clone --depth=1 https://github.com/liteserver/binn
cd binn
make
sudo make install
cd ..

# 安装 libsecp256k1-vrf

git clone --depth=1 https://github.com/aergoio/secp256k1-vrf
cd secp256k1-vrf
./autogen.sh
./configure --disable-benchmark
make
sudo make install
cd ..

# 安装 AergoLite

git clone --depth=1 https://github.com/aergoio/aergolite
cd aergolite
make
sudo make install
cd -
```

### 在 Windows 中需要使用 MinGW

然后将以下命令复制并粘贴到 MSYS2 MinGW 终端上运行：

```
# 编译 libuv

git clone --depth=1 https://github.com/libuv/libuv
cd libuv
./autogen.sh
./configure
make
make install
cd ..

# 编译 binn

git clone --depth=1 https://github.com/liteserver/binn
cd binn
make
make install
cd ..

# 编译 libsecp256k1-vrf

git clone --depth=1 https://github.com/aergoio/secp256k1-vrf
cd secp256k1-vrf
./autogen.sh
./configure --with-bignum=no --disable-benchmark
make
make install
cd ..

# 编译 AergoLite

git clone --depth=1 https://github.com/aergoio/aergolite
cd aergolite
make
make install
```

### Android

使用 [SQLite Android Bindings](https://github.com/aergoio/aergolite-tools/tree/master/wrappers/SQLite_Android_Bindings)
生成 `aar` 文件，然后在Android Studio中引用即可。
 [这里](https://github.com/aergoio/aergolite-tools/tree/master/projects/AndroidStudio-NativeInterface) 有一个简单的示例。


### iOS

使用以下命令生成静态和动态库：

```
./makeios
```

这样之后在IOS项目中就可以作为模块引用。
你也可以直接将 [AergoLite.swift](https://github.com/aergoio/AergoLite.swift) 复制到`AergoLite`的子目录。


## 自动测试

这些测试可在您的计算机上模拟多达100个节点。

在运行测试之前，您需要增加终端上打开文件的限制：

```
ulimit -Sn 16000
```

然后，您可以使用以下命令运行自动测试：

```
make test
```

如果要生成debug日志文件，必须在运行测试之前以debug模式重新编译该库：

```
make clean
make debug
```

也可以使用Valgrind运行测试：

```
make valgrind
```


## 使用

编译之后的库同时支持本地SQLite数据库文件和支持区块链的SQLite数据库。

所以可以同时打开本地SQLite数据库和支持区块链的数据库。

该库的使用方法与一般的SQLite数据库一样。

为了能使打开的数据库支持区块链，我们需要使用URI参数： `blockchain`

示例：

```
"file:test.db?blockchain=on"
```


## 区块链管理员

AergoLite实现了私有区块链。 这意味着您或您的组织可以拥有您自己的私有区块链，您可以在其中控制可能发生的事情。

控制区块链的实体称为区块链管理员。 它是具有自己的私钥+公钥对的用户。

区块链管理员可以：

* 将节点添加到区块链网络
* 执行预设的SQL命令

在将来的版本中，它还将能够：

* 从区块链网络中删除节点
* 将用户添加到网络
* 创建智能合约以使节点执行被禁止的SQL命令

您将需要在每个参与节点上验证区块链管理员的公钥。

这样可以确保：

1. 节点可以验证收到的命令是来自区块链管理员的
2. 不属于您的节点将不会加入网络

设定区块链管理员通过`admin`参数完成的，其中公共密钥可以是本机或hex格式。

示例：

```
"file:test.db?blockchain=on&admin=95F9AB75CA1..."
```


## 恒定性

基于信任的解决方案允许特定的用户或节点在数据库上执行任何SQL命令。

这样是不安全的，因为获得单个节点控制权的攻击者可以删除或者重写覆盖整个网络上的数据。

真正的去中心化的和不可篡改的区块链要限制节点的权限。

在AergoLite上，节点只能将数据追加到数据库。 他们无法修改它。 节点只能执行` INSERT INTO` SQL命令。只有管理员才能执行所有SQL命令。

未来版本可能允许节点执行管理员创建的智能合约，该合约可以包括任何SQL命令。

攻击者需要控制网络上的大多数节点才能对其进行攻击。


## 私钥保护

每个节点生成一个不同的私钥+公钥对。 它们通过其公共密钥进行标识和授权。

目前，每个节点的私钥都以加密方式存储在设备上。 将来的版本可能支持基于硬件的私钥保护。

我们需要使用` password` URI参数确定用于解密私钥的密码。

示例：

```
"file:test.db?blockchain=on&admin=95F9AB75CA1...&password=testing"
```

每个节点上的密码可以不同。

您的应用程序可以选择为每个节点生成和存储私钥。 在这种情况下，它可以使用`privkey`参数通过URI将hex格式的私钥写进数据库中。

```
"file:test.db?blockchain=on&admin=95F9AB75CA1...&privkey=AABBCCDD..."
```

在每个实例必须具有不同私钥的容器上使用AergoLite时，这很有用。

区块链管理员应该以安全的方式存储其私钥。 我们建议不要将其存储在区块链任一节点上，并且不要以纯格式存储。 它应该被加密并存储在外部设备或媒体上。 纸钱包也是个不错的选择。 最好的选择是使用硬件钱包。


## 硬件钱包

![ledger-app-aergolite-sql](https://user-images.githubusercontent.com/7624275/75842624-98a79180-5daf-11ea-8427-f0c3e7788f41.jpg)

为了获得更高的安全性，区块链管理员可以使用Ledger Nano S保护私钥。

在这种情况下，他/她将使用设备签署交易。

有关更多详细信息，请参见 [说明](https://github.com/aergoio/aergolite/wiki/Using-a-Hardware-Wallet)


## 节点发现

节点需要在区块链网络上发现其对等节点。

我们使用` discovery` URI参数指定节点发现方法。

节点发现有2个选项：

### 1. 本地UDP广播

此方法将局域网上的UDP广播数据包发送到指定的端口。

来自同一局域网的所有节点必须使用相同的端口号。

示例：

```
"file:test.db?blockchain=on&discovery=local:4329"
```

### 2. 已知节点

当某些节点具有固定ip时，可以使用此方法。

地址已知的节点也必须绑定到已定义的TCP端口。 这是通过`bind`参数确定的。

“已知”节点的示例URI：

```
"file:test.db?blockchain=on&bind=5501"
```

其他节点必须具有包含已知节点地址的显式`discovery`参数。

其他节点的示例URI：

```
"file:test.db?blockchain=on&discovery=<ip-address>:<port>"
```

我们还可以指定更多已知节点的地址：

```
"file:test.db?blockchain=on&discovery=<ip-address1>:<port1>,<ip-address2>:<port2>"
```

建立连接并接受节点后，它们将交换活动节点地址列表。

### 3. 两种方法都使用

我们也可以同时使用上述两种方法。 当我们在LAN上有一些节点而其他节点在外部时，这将非常有效。

我们可以固定一个或多个节点的地址，以便本地网络外部的节点可以找到它们。

LAN上的节点将通过UDP广播发现本地节点，并且可以连接到LAN外部的已知节点或从它们接收连接。

已知节点可以绑定到端口，通过广播找到本地节点，也可以连接到外部已知节点。

示例：

```
"file:test.db?blockchain=on&bind=1234&discovery=local:1234,<outside_ip1>:<port1>,<outside_ip2>:<port2>"
```

没有固定地址的节点将使用本地发现来连接外部已知节点：

```
"file:test.db?blockchain=on&discovery=local:1234,<outside_ip1>:<port1>,<outside_ip2>:<port2>"
```

如果该LAN上的节点只是从外部接收连接，则`discovery`参数必须仅需要本地发现方法。


## 列出连接的节点

您可以使用以下命令列出私有区块链网络上的节点：

```
PRAGMA nodes
```

它将列出所有已授权的节点（已连接或未连接的节点）以及尚未授权的已连接节点。


## 将节点添加到网络

使用上述命令列出连接的节点后，区块链管理员可以使用以下命令授权节点：

```
PRAGMA add_node="<public key>"
```

只有区块链管理员才能将节点添加到网络。

被授权的第一个节点必须是正在执行命令的节点。

其他节点的授权必须在已经授权的节点上执行。

如果连接了设备，以上命令将发送到Ledger设备进行签名，否则将触发交易签名回调，其中必须使用区块链管理员私钥对交易进行签名。


## 指定节点类型

默认情况下，节点被授权为**轻型**节点（不保留块的历史记录）。 要将其授权为**完全**节点，请在该节点的公共密钥（`<public key>`）之前添加`full:`：

```
PRAGMA add_node="full:<public key>"
```

要在节点被授权后修改其类型，请使用` node_type`命令。 它具有以下格式：

```
PRAGMA node_type="<type>:<nodes>"
```

type可以是`full`或`light`。 “节点”是所有授权节点的节点标识符（公用密钥或节点ID）或`*`的逗号分隔列表。

这里有些示例：

```
PRAGMA node_type="full:Am12..abc1"
PRAGMA node_type="full:Am12..abc1,Am12..abc2,Am12..abc3"
PRAGMA node_type="full:1287649477,3817592406,2373041549"
PRAGMA node_type="full:*"
PRAGMA node_type="light:1287649477"
```


## 签署交易

在AergoLite上，使用来自数据库交易的SQL命令构建区块链交易。

每个数据库交易都会生成一个区块链交易。

这些交易需要签名以被网络接受并包含在区块链中。

两个实体可以签署交易：

* 管理员
* 所有授权节点

如果交易需要特殊权限，则AergoLite库会将其发送给管理员，以供其签名。 否则，它将使用节点的私钥自动对其进行签名。

如果您的网络上没有使用Ledger设备，则至少一个节点需要注册一项功能，该功能将用于签署管理员的交易记录

Python中的示例：

```python
def on_sign_transaction(data):
  print "txn to be signed: " + data
  signature = sign(data, privkey)
  return hex(pubkey) + ":" + hex(signature)

con.create_function("sign_transaction", 1, on_sign_transaction)
```

> **注意：**回调函数是由**工作线程**调用的！
> 您的应用程序必须签署交易并尽快返回！

如果在节点上执行了需要管理员权限的特殊命令，但该命令未由该节点签名，则该交易将被拒绝。

## 检索状态

有两种方法检索状态

1. 在本地通过PRAGMA命令检索
2. 通过UDP数据包远程发送状态请求


### 数据库状态

您的应用程序必须在执行任何SQL命令之前检查本地数据库是否已准备好进行读写。

该检查通过以下命令完成：

```
PRAGMA db_is_ready
```

当应用程序可以读取和写入数据库时，它返回` 1`，否则返回` 0`。


### 区块链状态

区块链状态包含本地区块链，本地数据库和网络的信息。

可以使用以下命令在本地查询：

```
PRAGMA blockchain_status
```

它将以JSON格式返回结果，如下所示：

```
{
  "use_blockchain": true,

  "blockchain": {
    "last_block": 150,
    "state_hash": "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855",
    "integrity": {
      "state": "OK",
      "chain": "pruned"
    }
  },

  "node": {
    "id": 1366464921,
    "pubkey": "AmNdtXoBk6mYwgq2XDsx8pW9cvmoTQ3bp7v7kJxBcckvrC8HWBrE",
    "type": "light",
    "local_transactions": {
      "processed": 17,
      "unprocessed": 2,
      "total": 19
    }
  },

  "mempool": {
    "num_transactions": 2
  },

  "network": {
    num_authorized_nodes: 25,
    num_connected_peers: 21
  },

  "downstream_state": "in sync",
  "upstream_state": "in sync"
}
```

### 内存池状态

以下命令可以查询本地内存池上的未决事务。

```
PRAGMA mempool
```

它将以JSON格式返回结果，如下所示：

```
[{
  "id": 17698765927658,
  "node_id": 123,
  "nonce": 18,
  "timestamp": "2020-11-30 09:55:13",
  "commands": [
    "INSERT INTO test VALUES ('hello world!')"
  ]
}]
```

### 应用程序定义的节点信息

您的应用程序可以使用以下命令来设置特定于节点的信息：

```
PRAGMA node_info=<text>
```

取值可以是单个节点标识符，也可以包含以任何文本格式序列化的许多信息。 只有您的应用程序会使用它。

此信息将本地保存在内存中，也将发送到连接的对等方。 它不会保存在数据库中，而是动态的：下次使用不同的值执行此命令时，它将替换上一个命令。

可以使用`PRAGMA node_info`命令在本地检索该节点的最后一个值。

可以在`extra`字段的`PRAGMA nodes`命令的结果中查看连接的节点的值。

### 最后的nonce

Nonce：在信息安全中，Nonce是一个在加密通信只能使用一次的数字。在认证协议中，它往往是一个随机或伪随机数，以避免重放攻击。

在特定节点上生成的每个交易都有一个唯一的nonce。

可以使用以下命令检索当前节点的最后一个nonce：

```
PRAGMA last_nonce
```

如果返回的数字为0，则表示此节点尚未生成任何交易。


### 交易状态

检索本地交易的状态命令：

```
PRAGMA transaction_status(<nonce>)
```

此处的`<nonce>`应该被交易的随机数代替。 例如：`PRAGMA transaction_status（3）`



在完整节点上它将返回：

* `unprocessed`:交易尚未被网络处理
* `included`: 达成共识，交易被包括在一个区块中
* `rejected`: 达成共识，交易被拒绝

在轻节点上它将返回：

* `unprocessed`: 交易尚未被网络处理
* `processed`: 交易已由网络处理，并就其结果达成共识

轻节点不保留有关特定交易的信息。


### 交易通知

要知道特定交易是否在块中，应用程序必须使用回调函数。 设置为`user defined function`(用户定义函数)：

Python中的示例：

```python
def on_processed_transaction(nonce, status):
  print "transaction " + str(nonce) + ": " + status
  return None

con.create_function("transaction_notification", 2, on_processed_transaction)
```

C中的示例：

```C
static void on_processed_transaction(sqlite3_context *context, int argc, sqlite3_value **argv){
  long long int nonce = sqlite3_value_int64(argv[0]);
  char *status = sqlite3_value_text(argv[1]);

  printf("transaction %lld: %s\n", nonce, status);

  sqlite3_result_null(context);
}

sqlite3_create_function(db, "transaction_notification", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
  NULL, &on_processed_transaction, NULL, NULL);
```

> **注意：**回调函数是由**工作线程**调用的！
> 您的应用程序不应在那里使用db连接，并且必须尽快返回！
> 它可以在返回之前将通知发送到主线程


### 更新通知

每当在本地数据库上发生更新时，由于在区块链上收到一个新块，都可以通知您的应用程序。

通知是通过使用`user defined function`(用户定义函数)设置的回调函数发出的：

Python中的示例：

```python
def on_db_update(arg):
  print "update received"
  return None

con.create_function("update_notification", 1, on_db_update)
```

C中的示例：

```C
static void on_db_update(sqlite3_context *context, int argc, sqlite3_value **argv){
  puts("update received");
  sqlite3_result_null(context);
}

sqlite3_create_function(db, "update_notification", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
  NULL, &on_db_update, NULL, NULL);
```

> **注意：**回调函数是由**工作线程**调用的！
> 您的应用程序不应在那里使用db连接，并且必须尽快返回！
> 它可以在返回之前将通知发送到主线程


## 区块间隔

区块是由每一轮随机选择的节点创建的。

AergoLite不会产生空块。 如果没有要处理的交易，则不会创建任何块。

在交易到达节点时，将激活用于创建新块的计时器。

超时间隔可以通过URI参数`block_interval` （块间隔）来配置。

`block_interval`的单位是毫秒，1000就是1秒。

```
"file:test.db?blockchain=on&block_interval=1000"
```

如果未指定块间隔，则库将使用默认值3秒。


## 局限性

第一个版本使用完全连接的网络进行节点之间的通信。 在自动化测试中，它最多可与200个节点一起使用。 在以后的版本，它将添加一个基于gossip的协议，以支持数百万个节点。

每个数据库文件只有1个连接。 如果有许多应用程序需要访问db文件，则每个应用程序必须具有自己的数据库副本，并配置为单独的节点。

rowid表中的行编号（那些使用整数作为主键的行）与SQLite不同。 前32bit是节点ID，后面32bit是每个节点的顺序。 这也意味着每个节点可以在每个rowid表上最多创建2 ^ 32行。

和其他的多主复制系统一样，可能产生某些冲突。在某些情况下，整个交易可能会被取消，因此请考虑到这一点。为了避免交易停止，请参阅上面的应用程序如何检索交易状态。


## 许可协议

AergoLite 是通过以下任一协议发布的：

1. AGPLv3

这意味着您的应用程序必须遵守此许可，包括发布其源代码并在兼容的GPL下发布。

2. COMMERCIAL LICENSE

如果上述条件不符合您的要求，或者您想要更好的支持和服务，请与我们联系以获取商业许可证。


## 关于我们

AergoLite由Bernardo Ramos基于Aergo开发:

[![aergo logo](https://user-images.githubusercontent.com/7624275/100549737-8e89c500-3253-11eb-96b3-585916ed0883.png)](https://aergo.io/)

Aergo拥有[公共区块链](https://aergoscan.io/)的支持，并且在Lua中支持[智能合约](https://ide.aergo.io/)

我们很快会支持智能合约上的关系型数据存储和SQL。目前我们只在测试网和私有链提供。


## 支持

更多的信息和问题请来我们的[论坛](https://aergolite-forum.aergo.io/)

特殊的/企业级的支持请通过邮件联系我们： aergolite *AT* aergo *DOT* io
