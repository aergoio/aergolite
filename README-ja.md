<p align="right"><a href="README.md">English</a>&nbsp;&nbsp;&nbsp;<a href="README-zh.md">中文</a>&nbsp;&nbsp;&nbsp;<a href="README-pt.md">Português</a>&nbsp;&nbsp;&nbsp;<a href="README-ru.md">Русский</a>&nbsp;&nbsp;&nbsp;<a href="README-ko.md">한국어</a></p>

<p align="center"><img width="65%" src="https://user-images.githubusercontent.com/7624275/92685476-2390b800-f30e-11ea-9edc-980b0e66c0ad.png" alt="AergoLite"></p>

<h1 align="center">Trustless SQLite Replication</h1>

AergoLiteは、プライベートで軽量なブロックチェーンによって保護されたレプリケートSQLiteデータベースを提供します。

各アプリケーションはデータベースのローカルレプリカを持ちます。

新しいデータベーストランザクションはすべてのピアに配布され、実行順序についてコンセンサスに達すると、すべてのノードがトランザクションを実行します。実行順序が同じであるため、すべてのノードは同じ結果のデータベースコンテンツを持ちます。

アプリケーションはオフライン時にもローカルデータベースに書き込むことができます。データベーストランザクションはローカルキューに保存され、接続が再確立されるとネットワークに送信されます。アプリケーションはオフライン変更を含むデータベースの新しい状態を読み取り、オフライントランザクションがグローバルコンセンサスによって処理されたかどうかを確認できます。拒否された場合、データベースは以前の状態に戻ります。

AergoLiteは、**リソース制約のあるデバイス**に焦点を当てた**特殊なブロックチェーン技術**を使用します。

コンセンサスプロトコルは**検証可能なランダム関数（VRF）**を使用して次のブロックを生成するノードを決定し、ノードは事前にどのノードが選択されるかを発見できません。これにより、サービス拒否（DoS）攻撃に対して安全です。

AergoLiteは**絶対的なファイナリティ**を使用します。ノードが新しいブロックについてコンセンサスに達し、トランザクションが確認されると、元に戻す方法はありません。また、処理するトランザクションがない場合（確率的ファイナリティとは異なり）、新しいブロックを作成する必要はありません。

ブロックチェーンとデータベース状態の整合性をチェックするには、最後のブロックのみが必要であるため、ノードはすべてのブロックとトランザクションの履歴を保持および検証する必要はありません。
監査のために一部のノードにすべての履歴を保持するように設定することも可能です。

また、**データベース状態のハッシュ**も使用します。これにより、ノードはデータベースにまったく同じコンテンツがあるかどうかを確認でき、データベースファイルへの意図的な変更から保護し、ストレージメディアの障害を検出するための整合性チェックとしても機能します。

この最終ハッシュは、各新しいブロックで変更されたページのみを使用して更新されます。新しい状態を計算するためにデータベース全体を読み込む必要はありません。整合性チェックも、新しいデータベースページが読み込まれたときにのみ実行されます。これにより、データベースのパフォーマンスが大幅に向上します。

結果として得られるソリューションは、大きなディスクストレージを必要とせず、低いプロセッサ時間と低いRAMを使用します。

ネットワークトラフィックも軽量で、エネルギー消費を削減します。新しいパケットは、新しいデータベーストランザクションがある場合にのみ転送されます。

この技術により、IoTデバイスやモバイルデバイスでプライベートブロックチェーンを実行できます。

使用するためにブロックチェーンの仕組みを知る必要はありません。

サポートされているOS：

* Mac
* Linux
* Windows
* Android
* iOS
* OpenWrt

サポートされているプログラミング言語：

* C
* C++
* Java
* Javascript (Node.js)
* Python
* .Net (C# および VB)
* Ruby
* Swift
* Lua
* Go

おそらく、SQLiteをサポートしている他の言語も使用できます。

これらの言語のほとんどは、既存のラッパーを介してサポートされています。


## プリコンパイル済みバイナリ

[リリース](https://github.com/aergoio/aergolite/releases)を確認してください


## Dockerイメージ

* [Base](https://hub.docker.com/r/aergo/aergolite)（ライブラリとSQLiteシェル付き、C/C++アプリケーションを追加するためにも使用可能）
* [Python](https://hub.docker.com/r/aergo/aergolite-python)
* [Node.js](https://hub.docker.com/r/aergo/aergolite-nodejs)

ローカルでイメージをビルドするには：

```
make docker
```


## コンパイルとインストール

### LinuxとMacの場合

まず、次のコマンドで必要なツールをインストールします：

**UbuntuとDebian**

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

次に、ターミナルにこれをコピーして貼り付けます：

```
# Install libuv

git clone --depth=1 https://github.com/libuv/libuv
cd libuv
./autogen.sh
./configure
make
sudo make install
sudo ldconfig
cd ..

# Install binn

git clone --depth=1 https://github.com/liteserver/binn
cd binn
make
sudo make install
cd ..

# Install libsecp256k1-vrf

git clone --depth=1 https://github.com/aergoio/secp256k1-vrf
cd secp256k1-vrf
./autogen.sh
./configure --disable-benchmark
make
sudo make install
cd ..

# Install AergoLite

git clone --depth=1 https://github.com/aergoio/aergolite
cd aergolite
make
sudo make install
cd -
```

### MinGWを使用したWindowsの場合

MSYS2 MinGWターミナルにこれをコピーして貼り付けます：

```
# Compile libuv

git clone --depth=1 https://github.com/libuv/libuv
cd libuv
./autogen.sh
./configure
make
make install
cd ..

# Compile binn

git clone --depth=1 https://github.com/liteserver/binn
cd binn
make
make install
cd ..

# Compile libsecp256k1-vrf

git clone --depth=1 https://github.com/aergoio/secp256k1-vrf
cd secp256k1-vrf
./autogen.sh
./configure --with-bignum=no --disable-benchmark
make
make install
cd ..

# Compile AergoLite

git clone --depth=1 https://github.com/aergoio/aergolite
cd aergolite
make
make install
```

### Androidの場合

[SQLite Android Bindings](https://github.com/aergoio/aergolite-tools/tree/master/wrappers/SQLite_Android_Bindings)を使用して`aar`ファイルを生成し、Android Studioプロジェクトに含めます。
サンプルプロジェクトは[こちら](https://github.com/aergoio/aergolite-tools/tree/master/projects/AndroidStudio-NativeInterface)で利用できます。


### iOSの場合

次のコマンドで静的ライブラリと動的ライブラリを生成します：

```
./makeios
```

これらは、iOSプロジェクトのモジュールとして含めることができます。
[AergoLite.swift](https://github.com/aergoio/AergoLite.swift)ラッパーの`AergoLite`サブフォルダにコピーすることもできます


## 自動テスト

これらのテストは、コンピューター上で最大100ノードをシミュレートします。

テストを実行する前に、ターミナルで開くファイルの制限を増やす必要があります：

```
ulimit -Sn 16000
```

その後、次のコマンドで自動テストを実行できます：

```
make test
```

デバッグメッセージをログファイルに出力するには、テストを実行する前にデバッグモードでライブラリを再コンパイルする必要があります：

```
make clean
make debug
```

Valgrindを使用したテストの実行も可能です：

```
make valgrind
```


## 使用方法

コンパイルされたライブラリは、ネイティブSQLiteデータベースファイルとブロックチェーンサポート付きSQLiteデータベースの両方をサポートしているため、アプリケーションはネイティブSQLiteデータベースとブロックチェーン付きデータベースを同時に開くことができます。

ライブラリは、通常のSQLiteデータベースと同じように機能します。

ブロックチェーンサポート付きでデータベースを開くには、URIパラメータ`blockchain`を使用します。

例：

```
"file:test.db?blockchain=on"
```


## ブロックチェーン管理者

AergoLiteはプライベートブロックチェーンを実装しています。これは、あなたまたはあなたの組織が独自のプライベートブロックチェーンを持ち、そこで何が起こるかを制御できることを意味します。

ブロックチェーンを制御するエンティティは、ブロックチェーン管理者と呼ばれます。これは、独自の秘密鍵と公開鍵のペアを持つユーザーです。

ブロックチェーン管理者は以下を実行できます：

* ブロックチェーンネットワークにノードを追加
* 任意のSQLコマンドを実行
* ストアドプロシージャを作成

将来のバージョンでは、以下も可能になります：

* ブロックチェーンネットワークからノードを削除
* ネットワークにユーザーを追加

各参加ノードでブロックチェーン管理者の公開鍵を指定する必要があります。

これにより、以下が保証されます：

1. ノードは、受信したコマンドがブロックチェーン管理者からのものであることを確認できます
2. ノードは、あなたが所有していないネットワークに参加しません

これは`admin`パラメータを介して行われ、公開鍵はネイティブ形式または16進形式で指定できます。

例：

```
"file:test.db?blockchain=on&admin=95F9AB75CA1..."
```


## セキュリティ

基本的なレプリケーションソリューションでは、ノードがデータベースで任意のSQLコマンドを実行できます。
これらは安全ではありません。単一のノードを制御する攻撃者がネットワーク全体のデータを削除または上書きできるためです。

真のトラストレスレプリケーションソリューションは、ノードが実行できることを制限し、それらを信頼すべきではありません。

攻撃者はローカルデータベースを変更できる可能性がありますが、これは他のノードには反映されません。

ネットワークへの成功した攻撃には、攻撃者がノードの過半数を制御する必要があります。この設計により、堅牢なレベルのセキュリティが確保され、単一のエンティティがシステムの整合性を侵害することが困難になります。


## ストアドプロシージャ

AergoLiteは、ノードが実行できることを制御するために、SQLで記述されたストアドプロシージャを使用します。

セキュリティ上の理由から：

1. 管理者のみがストアドプロシージャを作成できます。
2. ノードはこれらのストアドプロシージャのみを実行できます。他のSQLコマンドはブロックされます。

したがって、これらはスマートコントラクトと同様に機能します。

以下に例を示します：

```sql
CREATE PROCEDURE add_new_sale(@products) BEGIN
 -- make sure the caller can call this function
 ASSERT txn_sender() IN (SELECT pubkey FROM authorizations WHERE type = 'sale');
 -- insert a new sale
 INSERT INTO sales (time) VALUES (datetime('now'));
 -- retrieve the sale id and store it on a variable
 SET @sale_id = last_insert_rowid();
 -- insert each product, using a reference to the sale id
 FOREACH @prod_id, @qty, @price IN @products DO
   INSERT INTO sale_items (sale_id, prod_id, qty, price) VALUES (@sale_id, @prod_id, @qty, @price);
 END LOOP;
 -- return the sale id
 RETURN @sale_id;
END;
```

SQLから次のように呼び出すことができます：

```sql
CALL add_new_sale( [['123', 1, 10.00],['456', 2, 20.00]] );
```

利用可能なコマンドの完全なリファレンスは[こちら](https://github.com/aergoio/sqlite-stored-procedures)で見つけることができます

コアロジックがストアドプロシージャで行われるため、同じネットワーク上で異なるプログラミング言語で作成されたアプリを使用することが可能です。


## 不変性

ストアドプロシージャを`INSERT INTO`コマンドのみを含むように制限すると、データベースコンテンツは不変になります。

ただし、行の更新と削除を許可する通常の使用でも、変更履歴（すべてのSQLコマンド）は軽量ブロックチェーンに記録され、削除できないため、データの復元と監査の両方が可能です。


## 秘密鍵の保護

各ノードは、異なる秘密鍵と公開鍵のペアを生成します。それらは公開鍵によって識別および承認されます。

現在、各ノードの秘密鍵はデバイス上で暗号化されて保存されています。将来のバージョンでは、ハードウェアベースの秘密鍵保護をサポートする可能性があります。

秘密鍵を復号化するために使用されるパスワードを、`password` URIパラメータを使用して指定する必要があります。

例：

```
"file:test.db?blockchain=on&admin=95F9AB75CA1...&password=testing"
```

パスワードは各ノードで異なる場合があります。

オプションとして、アプリケーションが各ノードの秘密鍵を生成および保存する責任を負うことができます。この場合、`privkey`パラメータを使用してURI経由で16進形式の秘密鍵をライブラリに指定できます：

```
"file:test.db?blockchain=on&admin=95F9AB75CA1...&privkey=AABBCCDD..."
```

これは、各インスタンスが異なる秘密鍵を持たなければならないコンテナでAergoLiteを使用する場合に役立ちます。

ブロックチェーン管理者は、その秘密鍵を安全な方法で保存する責任があります。ブロックチェーンノードの1つに保存したり、プレーンテキスト形式で保存したりしないことをお勧めします。暗号化して外部デバイスまたはメディアに保存する必要があります。ペーパーウォレットも良いアイデアです。最良のオプションは、ハードウェアウォレットを使用することです。


## ハードウェアウォレット

![ledger-app-aergolite-sql](https://user-images.githubusercontent.com/7624275/75842624-98a79180-5daf-11ea-8427-f0c3e7788f41.jpg)

より高いレベルのセキュリティのために、ブロックチェーン管理者はLedger Nano Sを使用してその秘密鍵を保護できます。

この場合、デバイスを使用してトランザクションに署名します。

詳細については、[手順](https://github.com/aergoio/aergolite/wiki/Using-a-Hardware-Wallet)を確認してください


## ノードディスカバリー

ノードは、ブロックチェーンネットワーク上でそのピアを発見する必要があります。

`discovery` URIパラメータを使用してノードディスカバリーメソッドを指定します。

ノードディスカバリーには2つのオプションがあります：

### 1. ローカルUDPブロードキャスト

このメソッドは、指定されたポートにローカルエリアネットワーク上でUDPブロードキャストパケットを送信します。

同じローカルネットワークのすべてのノードは、同じポート番号を使用する必要があります。

例：

```
"file:test.db?blockchain=on&discovery=local:4329"
```

### 2. 既知のノード

このメソッドでは、一部のノードに固定IPアドレスがあり、他のノードがそれらに接続します。

既知のアドレスを持つノードは、定義されたTCPポートにもバインドする必要があります。これは`bind`パラメータを使用して指定されます。

「既知の」ノードのURIの例：

```
"file:test.db?blockchain=on&bind=5501"
```

他のノードは、既知のノードのアドレスを含む明示的な`discovery`パラメータを持つ必要があります。

他のノードのURIの例：

```
"file:test.db?blockchain=on&discovery=<ip-address>:<port>"
```

より多くの既知のノードのアドレスを指定することもできます：

```
"file:test.db?blockchain=on&discovery=<ip-address1>:<port1>,<ip-address2>:<port2>"
```

接続が確立され、ノードが受け入れられると、アクティブなノードアドレスのリストが交換されます。

### 3. 両方のメソッドの混合

上記の2つのメソッドを同時に使用することもできます。これは、LAN上にいくつかのノードがあり、他のノードが外部にある場合に役立ちます。

1つ以上のノードのアドレスを固定して、ローカルネットワーク外のノードから見つけることができるようにします。

LAN上のノードは、UDPブロードキャストを介してローカルノードを発見し、LAN外の既知のノードに接続するか、それらから接続を受信できます。

既知のノードはポートにバインドし、ブロードキャストを介してローカルノードを見つけ、外部の既知のノードに接続することもできます。例：

```
"file:test.db?blockchain=on&bind=1234&discovery=local:1234,<outside_ip1>:<port1>,<outside_ip2>:<port2>"
```

固定アドレスを持たないノードは、ローカルディスカバリーと外部の既知のノードへの接続を使用します：

```
"file:test.db?blockchain=on&discovery=local:1234,<outside_ip1>:<port1>,<outside_ip2>:<port2>"
```

このLAN上のノードが外部からの接続のみを受信している場合、`discovery`パラメータにはローカルディスカバリーメソッドのみを含める必要があります。


## 接続されたノードの一覧表示

次のコマンドを使用して、プライベートブロックチェーンネットワーク上のノードを一覧表示できます：

```
PRAGMA nodes
```

承認されたすべてのノード（接続されているかどうかに関係なく）と、まだ承認されていない接続されたノードが一覧表示されます。


## ネットワークへのノードの追加

上記のコマンドで接続されたノードを一覧表示した後、ブロックチェーン管理者は次のコマンドを使用してノードを承認できます：

```
PRAGMA add_node="<public key>"
```

ブロックチェーン管理者のみがネットワークにノードを追加できます。

承認される最初のノードは、コマンドが実行されているノードである必要があります。

他のノードの承認は、既に承認されているノードで実行する必要があります。

上記のコマンドは、デバイスが接続されている場合はLedgerデバイスに送信されて署名され、それ以外の場合はトランザクション署名コールバックが発生し、ブロックチェーン管理者の秘密鍵を使用してトランザクションに署名する必要があります。


## ノードタイプの指定

デフォルトでは、ノードは**ライト**ノード（ブロックの履歴を保持しない）として承認されます。**フル**ノードとして承認するには、ノードの公開鍵の前に`full:`を追加します：

```
PRAGMA add_node="full:<public key>"
```

ノードが既に承認された後にノードのタイプを変更するには、`node_type`コマンドを使用します。形式は次のとおりです：

```
PRAGMA node_type="<type>:<nodes>"
```

タイプは`full`または`light`です。「nodes」は、ノード識別子（公開鍵またはノードID）のコンマ区切りリスト、またはすべての承認されたノードの`*`です。

以下にいくつかの例を示します：

```
PRAGMA node_type="full:Am12..abc1"
PRAGMA node_type="full:Am12..abc1,Am12..abc2,Am12..abc3"
PRAGMA node_type="full:1287649477,3817592406,2373041549"
PRAGMA node_type="full:*"
PRAGMA node_type="light:1287649477"
```


## トランザクションの署名

AergoLiteでは、ブロックチェーントランザクションはデータベーストランザクションからのSQLコマンドを使用して構築されます。

各データベーストランザクションは、1つのブロックチェーントランザクションを生成します。

これらのトランザクションは、ネットワークによって受け入れられ、ブロックチェーンに含まれるために署名される必要があります。

2つのエンティティがトランザクションに署名できます：

* 管理者
* 各承認されたノード

トランザクションに特別な権限が必要な場合、AergoLiteライブラリは管理者によって署名されるように送信します。それ以外の場合は、ノードの秘密鍵を使用して自動的に署名します。

ネットワークでLedgerデバイスが使用されていない場合、少なくとも1つのノードが管理者からのトランザクションに署名するために使用される関数を登録する必要があります

Pythonの例：

```python
def on_sign_transaction(data):
  print "txn to be signed: " + data
  signature = sign(data, privkey)
  return hex(pubkey) + ":" + hex(signature)

con.create_function("sign_transaction", 1, on_sign_transaction)
```

> **注意：** コールバック関数は**ワーカースレッド**によって呼び出されます！！
> アプリケーションはトランザクションに署名し、できるだけ早く返す必要があります！

管理者権限を必要とする特別なコマンドがノードで実行されたが、管理者によって署名されていない場合、トランザクションは拒否されます。


## ステータスの取得

ステータスを取得するには2つの方法があります：

1. PRAGMAコマンドを介してローカルで
2. UDPパケットを介してリモートでステータスリクエストを送信


### データベースステータス

アプリケーションは、SQLコマンドが実行される前に、ローカルデータベースが読み取りと書き込みの準備ができているかどうかを確認する必要があります。

この確認は、次のコマンドで行われます：

```
PRAGMA db_is_ready
```

アプリケーションがデータベースから読み取り、データベースに書き込むことができる場合は`1`を返し、それ以外の場合は`0`を返します。


### ブロックチェーンステータス

これには、ローカルブロックチェーン、ローカルデータベース、およびネットワークに関する情報が含まれます。

次のコマンドを使用してローカルでクエリできます：

```
PRAGMA blockchain_status
```

次のようなJSON形式で結果を返します：

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

### メンプールステータス

ローカルメンプールの保留中のトランザクションを返します。

```
PRAGMA mempool
```

次のようなJSON形式で結果を返します：

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

### アプリケーション定義のノード情報

アプリケーションは、次のコマンドを使用してノード固有の情報を設定できます：

```
PRAGMA node_info=<text>
```

テキスト値は単一のノード識別子であるか、任意のテキスト形式でシリアル化された多くの情報を含むことができます。アプリケーションのみがそれを使用します。

この情報はローカルでメモリに保持され、接続されたピアにも送信されます。データベースに保存されず、動的です：次のコマンドが異なる値で実行されると、前の値を置き換えます。

このノードの最後に設定された値は、`PRAGMA node_info`コマンドを使用してローカルで取得できます。

接続されたノードからの値は、`PRAGMA nodes`コマンドの結果の`extra`フィールドで表示できます。


### 最後のnonce

特定のノードで生成された各トランザクションには、一意の増分nonceがあります。

次のコマンドで、現在のノードの最後のnonceを取得できます：

```
PRAGMA last_nonce
```

返された数値がゼロの場合、このノードはまだトランザクションを生成していないことを意味します。


### トランザクションステータス

ローカルトランザクションのステータスを取得するには：

```
PRAGMA transaction_status(<nonce>)
```

`<nonce>`はトランザクションのnonceに置き換える必要があります。例：`PRAGMA transaction_status(3)`

フルノードでは次を返します：

* `unprocessed`: トランザクションはまだネットワークによって処理されていません
* `included`: コンセンサスに達し、トランザクションがブロックに含まれました
* `rejected`: コンセンサスに達し、トランザクションが拒否されました

ライトノードでは次を返します：

* `unprocessed`: トランザクションはまだネットワークによって処理されていません
* `processed`: トランザクションがネットワークによって処理され、その結果についてコンセンサスに達しました

ライトノードは特定のトランザクションに関する情報を保持しません。


### トランザクション通知

特定のトランザクションがブロックに含まれたか拒否されたかを通知するには、アプリケーションはコールバック関数を使用する必要があります。これは`ユーザー定義関数`として設定されます：

Pythonの例：

```python
def on_processed_transaction(nonce, status):
  print "transaction " + str(nonce) + ": " + status
  return None

con.create_function("transaction_notification", 2, on_processed_transaction)
```

Cの例：

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

> **注意：** コールバック関数は**ワーカースレッド**によって呼び出されます！！
> アプリケーションはそこでdb接続を使用すべきではなく、できるだけ早く返す必要があります！
> 返す前にメインスレッドに通知を送信できます


### 更新通知

アプリケーションは、ブロックチェーンで新しいブロックを受信したためにローカルデータベースで更新が発生したときに通知を受けることができます。

通知は、`ユーザー定義関数`を使用して設定されるコールバック関数を使用して行われます：

Pythonの例：

```python
def on_db_update(arg):
  print "update received"
  return None

con.create_function("update_notification", 1, on_db_update)
```

Cの例：

```C
static void on_db_update(sqlite3_context *context, int argc, sqlite3_value **argv){
  puts("update received");
  sqlite3_result_null(context);
}

sqlite3_create_function(db, "update_notification", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
  NULL, &on_db_update, NULL, NULL);
```

> **注意：** コールバック関数は**ワーカースレッド**によって呼び出されます！！
> アプリケーションはそこでdb接続を使用すべきではなく、できるだけ早く返す必要があります！
> 返す前にメインスレッドに通知を送信できます


## ブロック間隔

ブロックは、各ラウンドでランダムに選択されたノードによって作成されます。

AergoLiteは空のブロックを生成しません。処理するトランザクションがない場合、ブロックは作成されません。

トランザクションがノードに到着すると（タイマーがまだアクティブでない場合）、新しいブロックを作成するタイマーがアクティブになります。

このタイムアウト間隔は、`block_interval`パラメータを使用してURI経由で設定できます。

値はミリ秒として解釈されます。

```
"file:test.db?blockchain=on&block_interval=1000"
```

ブロック間隔が指定されていない場合、ライブラリはデフォルト値として3秒を使用します。


## 制限事項

このバージョンは、ノード間の通信に完全に接続されたネットワークを使用します。最大200ノード（自動テストで確認）で動作し、おそらくそれ以上でも動作します。将来的には、さらに多くのノードをサポートするためのゴシップベースのプロトコルも含まれる可能性があります。

各データベースファイルへの接続は1つだけです。データベースファイルにアクセスする必要がある多くのアプリケーションがある場合、各アプリケーションは独自のデータベースのコピーを持ち、別のノードとして設定する必要があります。

rowidテーブル（整数を主キーとして使用するテーブル）の行の番号付けは、SQLiteとは異なります。最初の32ビットはノードIDで、残りの32ビットはノードごとに順次です。これは、各ノードが各rowidテーブルで最大2^32行を作成できることも意味します。

マルチマスターレプリケーションシステムと同様に、競合が発生する可能性があります。場合によってはトランザクション全体が中止される可能性があるため、これを考慮してください。アプリがトランザクションのステータスを確認する方法については、上記を参照してください。


## ライセンス

AergoLiteは、以下の2つのオプションのいずれかでリリースされます：

1. AGPLv3

これは、アプリケーションがこのライセンスに準拠し、ソースコードをリリースし、互換性のあるGPLの下で公開する必要があることを意味します。

2. 商用ライセンス

上記の条件が要件に適合しない場合、またはより良いサポートとサービスが必要な場合は、商用ライセンスを取得するためにご連絡ください。


## 私たちについて

AergoLiteは、Bernardo Ramosによって以下で開発されました：

[![aergo logo](https://user-images.githubusercontent.com/7624275/100549737-8e89c500-3253-11eb-96b3-585916ed0883.png)](https://aergo.io/)

Aergoには、Luaで[スマートコントラクト](https://ide.aergo.io/)をサポートする[パブリックブロックチェーン](https://aergoscan.io/)があります。

まもなくリレーショナルデータストレージとスマートコントラクトでのSQLをサポートします。現在、テストネットとプライベートチェーンで利用できます。


## サポート

低優先度のサポートは、[フォーラム](https://aergolite-forum.aergo.io/)で利用できます

特別なエンタープライズサポートも利用できます。メールでお問い合わせください：aergolite *AT* aergo *DOT* io

