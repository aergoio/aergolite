<p align="right"><a href="README.md">English</a>&nbsp;&nbsp;&nbsp;<a href="README-zh.md">中文</a>&nbsp;&nbsp;&nbsp;<a href="README-pt.md">Português</a>&nbsp;&nbsp;&nbsp;<a href="README-ru.md">Русский</a>&nbsp;&nbsp;&nbsp;<a href="README-ja.md">日本語</a></p>

<p align="center"><img width="65%" src="https://user-images.githubusercontent.com/7624275/92685476-2390b800-f30e-11ea-9edc-980b0e66c0ad.png" alt="AergoLite"></p>

<h1 align="center">신뢰 불필요한 SQLite 복제</h1>

AergoLite는 개인용 경량 블록체인으로 보안이 강화된 복제 SQLite 데이터베이스를 제공합니다.

각 앱은 데이터베이스의 로컬 복제본을 가지고 있습니다.

새로운 데이터베이스 트랜잭션은 모든 피어에 배포되며, 실행 순서에 대한 합의에 도달하면 모든 노드가 트랜잭션을 실행합니다. 실행 순서가 동일하므로 모든 노드는 동일한 결과 데이터베이스 내용을 갖게 됩니다.

앱은 오프라인일 때 로컬 데이터베이스에 쓸 수도 있습니다. 데이터베이스 트랜잭션은 로컬 큐에 저장되고 연결이 재설정되면 네트워크로 전송됩니다. 애플리케이션은 오프라인 수정 사항이 포함된 데이터베이스의 새 상태를 읽을 수 있으며, 오프라인 트랜잭션이 글로벌 합의에 의해 처리되었는지 확인할 수 있습니다. 거부된 경우 데이터베이스는 이전 상태로 돌아갑니다.

AergoLite는 **리소스 제약이 있는 장치**에 중점을 둔 **특수 블록체인 기술**을 사용합니다.

합의 프로토콜은 **검증 가능한 랜덤 함수(VRF)**를 사용하여 다음 블록을 생성할 노드를 결정하며, 노드는 미리 선택된 노드를 발견할 수 없습니다. 이를 통해 서비스 거부(DoS) 공격에 대해 안전합니다.

AergoLite는 **절대적 최종성**을 사용합니다. 노드가 새 블록에 대한 합의에 도달하고 트랜잭션이 확인되면 되돌릴 수 없습니다. 또한 처리할 트랜잭션이 없으면 새 블록을 생성할 필요가 없습니다(확률적 최종성과 달리).

블록체인 및 데이터베이스 상태 무결성을 확인하려면 마지막 블록만 필요하므로, 노드는 모든 블록 및 트랜잭션 기록을 유지하고 검증할 필요가 없습니다.
감사 목적으로 일부 노드를 설정하여 모든 기록을 유지할 수도 있습니다.

또한 **데이터베이스 상태의 해시**를 사용합니다. 이를 통해 노드는 데이터베이스에 정확히 동일한 내용이 있는지 확인할 수 있고, 데이터베이스 파일의 의도적인 수정으로부터 보호하며, 저장 매체의 오류를 감지하는 무결성 검사로도 작동합니다.

이 최종 해시는 각 새 블록에서 수정된 페이지만 사용하여 업데이트됩니다. 새 상태를 계산하기 위해 전체 데이터베이스를 로드할 필요가 없습니다. 무결성 검사는 새 데이터베이스 페이지가 로드될 때만 수행됩니다. 이를 통해 데이터베이스 성능이 크게 향상됩니다.

결과적으로 이 솔루션은 큰 디스크 저장 공간이 필요하지 않으며, 낮은 프로세서 시간과 낮은 RAM을 사용합니다.

네트워크 트래픽도 에너지 소비를 줄이기 위해 경량입니다. 새로운 데이터베이스 트랜잭션이 있을 때만 새 패킷이 전송됩니다.

이 기술을 통해 IoT 및 모바일 장치에서 개인 블록체인을 실행할 수 있습니다.

사용하기 위해 블록체인이 어떻게 작동하는지 알 필요가 없습니다.

지원 OS:

* Mac
* Linux
* Windows
* Android
* iOS
* OpenWrt

지원 프로그래밍 언어:

* C
* C++
* Java
* Javascript (Node.js)
* Python
* .Net (C# 및 VB)
* Ruby
* Swift
* Lua
* Go

그리고 아마도 SQLite를 지원하는 다른 언어도 가능합니다.

이러한 언어 대부분은 기존 래퍼를 통해 지원됩니다.


## 사전 컴파일된 바이너리

[릴리스](https://github.com/aergoio/aergolite/releases)를 확인하세요


## Docker 이미지

* [Base](https://hub.docker.com/r/aergo/aergolite) (라이브러리 및 SQLite 셸 포함, C/C++ 애플리케이션 추가에도 사용 가능)
* [Python](https://hub.docker.com/r/aergo/aergolite-python)
* [Node.js](https://hub.docker.com/r/aergo/aergolite-nodejs)

로컬에서 이미지를 빌드하려면:

```
make docker
```


## 컴파일 및 설치

### Linux 및 Mac에서

먼저 다음 명령으로 필요한 도구를 설치하세요:

**Ubuntu 및 Debian**

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

그런 다음 터미널에 다음을 복사하여 붙여넣으세요:

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

### MinGW를 사용한 Windows에서

MSYS2 MinGW 터미널에 다음을 복사하여 붙여넣으세요:

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

### Android용

[SQLite Android Bindings](https://github.com/aergoio/aergolite-tools/tree/master/wrappers/SQLite_Android_Bindings)를 사용하여 `aar` 파일을 생성한 다음 Android Studio 프로젝트에 포함하세요.
샘플 프로젝트는 [여기](https://github.com/aergoio/aergolite-tools/tree/master/projects/AndroidStudio-NativeInterface)에서 사용할 수 있습니다.


### iOS용

다음 명령으로 정적 및 동적 라이브러리를 생성하세요:

```
./makeios
```

이들은 iOS 프로젝트의 모듈로 포함할 수 있습니다.
또한 [AergoLite.swift](https://github.com/aergoio/AergoLite.swift) 래퍼의 `AergoLite` 하위 폴더에 복사할 수 있습니다.


## 자동화된 테스트

이 테스트는 컴퓨터에서 최대 100개의 노드를 시뮬레이션합니다.

테스트를 실행하기 전에 터미널에서 열린 파일의 제한을 늘려야 합니다:

```
ulimit -Sn 16000
```

그런 다음 다음 명령으로 자동화된 테스트를 실행할 수 있습니다:

```
make test
```

로그 파일에 디버그 메시지를 인쇄하려면 테스트를 실행하기 전에 디버그 모드에서 라이브러리를 다시 컴파일해야 합니다:

```
make clean
make debug
```

Valgrind로 테스트를 실행하는 것도 가능합니다:

```
make valgrind
```


## 사용

컴파일된 라이브러리는 네이티브 SQLite 데이터베이스 파일과 블록체인 지원이 있는 SQLite 데이터베이스를 모두 지원하므로 애플리케이션은 네이티브 SQLite 데이터베이스와 블록체인이 있는 데이터베이스를 동시에 열 수 있습니다.

라이브러리는 일반 SQLite 데이터베이스와 정확히 동일한 방식으로 작동합니다.

블록체인 지원으로 데이터베이스를 열려면 URI 매개변수 `blockchain`을 사용합니다.

예제:

```
"file:test.db?blockchain=on"
```


## 블록체인 관리자

AergoLite는 개인 블록체인을 구현합니다. 즉, 귀하 또는 귀하의 조직이 발생할 수 있는 것을 제어할 수 있는 자체 개인 블록체인을 가질 수 있습니다.

블록체인을 제어하는 엔티티를 블록체인 관리자라고 합니다. 자체 개인 키 + 공개 키 쌍을 가진 사용자입니다.

블록체인 관리자는 다음을 수행할 수 있습니다:

* 블록체인 네트워크에 노드 추가
* 모든 SQL 명령 실행
* 저장 프로시저 생성

향후 버전에서는 다음도 가능할 예정입니다:

* 블록체인 네트워크에서 노드 제거
* 네트워크에 사용자 추가

각 참여 노드에서 블록체인 관리자의 공개 키를 알려야 합니다.

이를 통해 다음을 보장합니다:

1. 노드는 수신된 명령이 블록체인 관리자로부터 온 것인지 확인할 수 있습니다
2. 노드는 귀하가 소유하지 않은 네트워크에 가입하지 않습니다

이는 `admin` 매개변수를 통해 수행되며, 공개 키는 네이티브 또는 16진수 형식일 수 있습니다.

예제:

```
"file:test.db?blockchain=on&admin=95F9AB75CA1..."
```


## 보안

기본 복제 솔루션은 노드가 데이터베이스에서 모든 SQL 명령을 실행할 수 있도록 합니다.
단일 노드의 제어를 획득한 공격자가 전체 네트워크의 데이터를 삭제 및/또는 덮어쓸 수 있기 때문에 안전하지 않습니다.

실제 신뢰 불필요한 복제 솔루션은 노드가 수행할 수 있는 작업을 제한하고 노드를 신뢰하지 않아야 합니다.

공격자는 로컬 데이터베이스를 수정할 수 있지만, 이것은 다른 노드에 반영되지 않습니다.

네트워크에 대한 성공적인 공격을 위해서는 공격자가 대다수 노드의 제어를 획득해야 합니다. 이 설계는 강력한 수준의 보안을 보장하여 단일 엔티티가 시스템의 무결성을 손상시키기 어렵게 만듭니다.


## 저장 프로시저

AergoLite는 노드가 수행할 수 있는 작업을 제어하기 위해 SQL로 작성된 저장 프로시저를 사용합니다.

보안상의 이유로:

1. 관리자만 저장 프로시저를 생성할 수 있습니다.
2. 노드는 이러한 저장 프로시저만 실행할 수 있습니다. 다른 SQL 명령은 차단됩니다.

따라서 스마트 컨트랙트와 유사하게 작동합니다.

다음은 예제입니다:

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

다음과 같이 SQL에서 호출할 수 있습니다:

```sql
CALL add_new_sale( [['123', 1, 10.00],['456', 2, 20.00]] );
```

사용 가능한 명령의 전체 참조는 [여기](https://github.com/aergoio/sqlite-stored-procedures)에서 찾을 수 있습니다.

핵심 로직이 저장 프로시저로 수행되므로 동일한 네트워크에서 다른 프로그래밍 언어로 만든 앱을 사용할 수 있습니다.


## 불변성

저장 프로시저를 `INSERT INTO` 명령만 포함하도록 제한하면 데이터베이스 내용이 불변입니다.

그러나 행 업데이트 및 삭제를 허용하는 일반적인 사용에서도 변경 기록(모든 SQL 명령)이 경량 블록체인에 기록되며 삭제할 수 없어 데이터 복원 및 감사가 가능합니다.


## 개인 키 보호

각 노드는 고유한 개인 키 + 공개 키 쌍을 생성합니다. 공개 키를 통해 식별되고 인증됩니다.

현재 각 노드의 개인 키는 장치에 암호화되어 저장됩니다. 향후 버전에서는 하드웨어 기반 개인 키 보호를 지원할 수 있습니다.

`password` URI 매개변수를 사용하여 개인 키를 복호화하는 데 사용되는 비밀번호를 알려야 합니다.

예제:

```
"file:test.db?blockchain=on&admin=95F9AB75CA1...&password=testing"
```

비밀번호는 각 노드마다 다를 수 있습니다.

선택적으로 애플리케이션이 각 노드의 개인 키를 생성하고 저장할 책임을 질 수 있습니다. 이 경우 URI의 `privkey` 매개변수를 통해 16진수 형식의 개인 키를 라이브러리에 알릴 수 있습니다:

```
"file:test.db?blockchain=on&admin=95F9AB75CA1...&privkey=AABBCCDD..."
```

이는 각 인스턴스가 다른 개인 키를 가져야 하는 컨테이너에서 AergoLite를 사용할 때 유용할 수 있습니다.

블록체인 관리자는 개인 키를 안전한 방식으로 저장할 책임이 있습니다. 블록체인 노드 중 하나에 저장하거나 일반 형식으로 저장하지 않는 것을 권장합니다. 암호화되어 외부 장치나 미디어에 저장되어야 합니다. 페이퍼 지갑도 좋은 아이디어입니다. 가장 좋은 옵션은 하드웨어 지갑을 사용하는 것입니다.


## 하드웨어 지갑

![ledger-app-aergolite-sql](https://user-images.githubusercontent.com/7624275/75842624-98a79180-5daf-11ea-8427-f0c3e7788f41.jpg)

최고 수준의 보안을 위해 블록체인 관리자는 Ledger Nano S를 사용하여 개인 키를 보호할 수 있습니다.

이 경우 장치를 사용하여 트랜잭션에 서명합니다.

자세한 내용은 [지침](https://github.com/aergoio/aergolite/wiki/Using-a-Hardware-Wallet)을 확인하세요.


## 노드 발견

노드는 블록체인 네트워크에서 피어를 발견해야 합니다.

`discovery` URI 매개변수를 사용하여 노드 발견 방법을 지정합니다.

노드 발견에는 2가지 옵션이 있습니다:

### 1. 로컬 UDP 브로드캐스트

이 방법은 지정된 포트로 로컬 영역 네트워크에 UDP 브로드캐스트 패킷을 보냅니다.

동일한 로컬 네트워크의 모든 노드는 동일한 포트 번호를 사용해야 합니다.

예제:

```
"file:test.db?blockchain=on&discovery=local:4329"
```

### 2. 알려진 노드

이 방법에서는 일부 노드가 고정 IP 주소를 가지고 다른 노드가 이들에 연결합니다.

알려진 주소를 가진 노드는 정의된 TCP 포트에도 바인딩해야 합니다. 이것은 `bind` 매개변수를 사용하여 알려집니다.

"알려진" 노드의 예제 URI:

```
"file:test.db?blockchain=on&bind=5501"
```

다른 노드는 알려진 노드의 주소를 포함하는 명시적인 `discovery` 매개변수를 가져야 합니다.

다른 노드의 예제 URI:

```
"file:test.db?blockchain=on&discovery=<ip-address>:<port>"
```

더 많은 알려진 노드의 주소를 지정할 수도 있습니다:

```
"file:test.db?blockchain=on&discovery=<ip-address1>:<port1>,<ip-address2>:<port2>"
```

연결이 설정되고 노드가 수락되면 활성 노드 주소 목록을 교환합니다.

### 3. 두 방법 혼합

위의 두 방법을 동시에 사용할 수도 있습니다. LAN에 일부 노드가 있고 외부에 다른 노드가 있을 때 유용할 수 있습니다.

하나 이상의 노드 주소를 고정하여 로컬 네트워크 외부의 노드에서 찾을 수 있도록 할 수 있습니다.

LAN의 노드는 UDP 브로드캐스트를 통해 로컬 노드를 발견하고 LAN 외부의 알려진 노드에 연결하거나 이들로부터 연결을 받을 수 있습니다.

알려진 노드는 포트에 바인딩하고 브로드캐스트를 통해 로컬 노드를 찾고 외부 알려진 노드에도 연결할 수 있습니다. 예제:

```
"file:test.db?blockchain=on&bind=1234&discovery=local:1234,<outside_ip1>:<port1>,<outside_ip2>:<port2>"
```

고정 주소가 없는 노드는 로컬 발견 및 외부 알려진 노드에 대한 연결을 사용합니다:

```
"file:test.db?blockchain=on&discovery=local:1234,<outside_ip1>:<port1>,<outside_ip2>:<port2>"
```

이 LAN의 노드가 외부에서 연결만 수신하는 경우 `discovery` 매개변수는 로컬 발견 방법만 포함해야 합니다.


## 연결된 노드 나열

다음 명령을 사용하여 개인 블록체인 네트워크의 노드를 나열할 수 있습니다:

```
PRAGMA nodes
```

모든 인증된 노드(연결되었거나 연결되지 않은)와 아직 인증되지 않은 연결된 노드를 나열합니다.


## 네트워크에 노드 추가

위 명령으로 연결된 노드를 나열한 후 블록체인 관리자는 다음 명령을 사용하여 노드를 인증할 수 있습니다:

```
PRAGMA add_node="<public key>"
```

블록체인 관리자만 네트워크에 노드를 추가할 수 있습니다.

인증될 첫 번째 노드는 명령이 실행되는 노드여야 합니다.

다른 노드에 대한 인증은 이미 인증된 노드에서 실행되어야 합니다.

위 명령은 장치가 연결된 경우 Ledger 장치로 전송되어 서명되며, 그렇지 않으면 블록체인 관리자 개인 키를 사용하여 트랜잭션에 서명해야 하는 트랜잭션 서명 콜백을 발생시킵니다.


## 노드 유형 지정

기본적으로 노드는 **라이트** 노드(블록 기록을 유지하지 않음)로 인증됩니다. **풀** 노드로 인증하려면 노드의 공개 키 앞에 `full:`을 추가하세요:

```
PRAGMA add_node="full:<public key>"
```

노드가 이미 인증된 후 노드 유형을 수정하려면 `node_type` 명령을 사용하세요. 형식은 다음과 같습니다:

```
PRAGMA node_type="<type>:<nodes>"
```

유형은 `full` 또는 `light`일 수 있습니다. "nodes"는 노드 식별자(공개 키 또는 노드 ID)의 쉼표로 구분된 목록이거나 모든 인증된 노드에 대한 `*`입니다.

다음은 몇 가지 예제입니다:

```
PRAGMA node_type="full:Am12..abc1"
PRAGMA node_type="full:Am12..abc1,Am12..abc2,Am12..abc3"
PRAGMA node_type="full:1287649477,3817592406,2373041549"
PRAGMA node_type="full:*"
PRAGMA node_type="light:1287649477"
```


## 트랜잭션 서명

AergoLite에서 블록체인 트랜잭션은 데이터베이스 트랜잭션의 SQL 명령을 사용하여 구축됩니다.

각 데이터베이스 트랜잭션은 하나의 블록체인 트랜잭션을 생성합니다.

이러한 트랜잭션은 네트워크에서 수락되고 블록체인에 포함되려면 서명되어야 합니다.

두 엔티티가 트랜잭션에 서명할 수 있습니다:

* 관리자
* 각 인증된 노드

트랜잭션에 특별한 권한이 필요한 경우 AergoLite 라이브러리는 관리자가 서명하도록 전송합니다. 그렇지 않으면 노드의 개인 키를 사용하여 자동으로 서명합니다.

네트워크에서 Ledger 장치를 사용하지 않는 경우, 최소한 하나의 노드가 관리자의 트랜잭션에 서명하는 데 사용될 함수를 등록해야 합니다

Python 예제:

```python
def on_sign_transaction(data):
  print "txn to be signed: " + data
  signature = sign(data, privkey)
  return hex(pubkey) + ":" + hex(signature)

con.create_function("sign_transaction", 1, on_sign_transaction)
```

> **주의:** 콜백 함수는 **작업자 스레드**에서 호출됩니다!!
> 애플리케이션은 트랜잭션에 서명하고 가능한 한 빨리 반환해야 합니다!

관리자 권한이 필요한 특수 명령이 노드에서 실행되지만 관리자가 서명하지 않은 경우 트랜잭션이 거부됩니다.


## 상태 검색

상태를 검색하는 방법은 2가지가 있습니다:

1. PRAGMA 명령을 통한 로컬 검색
2. UDP 패킷을 통한 원격 상태 요청 전송


### 데이터베이스 상태

애플리케이션은 SQL 명령이 실행되기 전에 로컬 데이터베이스가 읽기 및 쓰기에 준비되었는지 확인해야 합니다.

이 확인은 다음 명령으로 수행됩니다:

```
PRAGMA db_is_ready
```

애플리케이션이 데이터베이스에서 읽고 쓸 수 있으면 `1`을 반환하고, 그렇지 않으면 `0`을 반환합니다.


### 블록체인 상태

로컬 블록체인, 로컬 데이터베이스 및 네트워크에 대한 정보를 포함합니다.

다음 명령을 사용하여 로컬에서 쿼리할 수 있습니다:

```
PRAGMA blockchain_status
```

다음과 같은 JSON 형식의 결과를 반환합니다:

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

### Mempool 상태

로컬 mempool의 보류 중인 트랜잭션을 반환합니다.

```
PRAGMA mempool
```

다음과 같은 JSON 형식의 결과를 반환합니다:

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

### 애플리케이션 정의 노드 정보

애플리케이션은 다음 명령을 사용하여 노드별 정보를 설정할 수 있습니다:

```
PRAGMA node_info=<text>
```

텍스트 값은 단일 노드 식별자일 수 있거나 임의의 텍스트 형식으로 직렬화된 많은 정보를 포함할 수 있습니다. 애플리케이션만 사용합니다.

이 정보는 로컬에서 메모리에 유지되며 연결된 피어에도 전송됩니다. 데이터베이스에 저장되지 않으며 동적입니다: 다른 값으로 이 명령이 다음에 실행되면 이전 값을 대체합니다.

이 노드에 대해 설정된 마지막 값은 `PRAGMA node_info` 명령을 사용하여 로컬에서 검색할 수 있습니다.

연결된 노드의 값은 `PRAGMA nodes` 명령 결과의 `extra` 필드에서 볼 수 있습니다.


### 마지막 nonce

특정 노드에서 생성된 각 트랜잭션에는 고유한 증분 nonce가 있습니다.

다음 명령으로 현재 노드의 마지막 nonce를 검색할 수 있습니다:

```
PRAGMA last_nonce
```

반환된 숫자가 0이면 이 노드가 아직 트랜잭션을 생성하지 않았음을 의미합니다.


### 트랜잭션 상태

로컬 트랜잭션의 상태를 검색하려면:

```
PRAGMA transaction_status(<nonce>)
```

여기서 `<nonce>`는 트랜잭션의 nonce로 대체되어야 합니다. 예: `PRAGMA transaction_status(3)`

다음을 반환합니다

풀 노드에서:

* `unprocessed`: 트랜잭션이 아직 네트워크에서 처리되지 않음
* `included`: 합의에 도달하고 트랜잭션이 블록에 포함됨
* `rejected`: 합의에 도달하고 트랜잭션이 거부됨

라이트 노드에서:

* `unprocessed`: 트랜잭션이 아직 네트워크에서 처리되지 않음
* `processed`: 트랜잭션이 네트워크에서 처리되고 결과에 대한 합의에 도달함

라이트 노드는 특정 트랜잭션에 대한 정보를 유지하지 않습니다.


### 트랜잭션 알림

특정 트랜잭션이 블록에 포함되었는지 또는 거부되었는지 알리기 위해 애플리케이션은 콜백 함수를 사용해야 합니다. `사용자 정의 함수`로 설정됩니다:

Python 예제:

```python
def on_processed_transaction(nonce, status):
  print "transaction " + str(nonce) + ": " + status
  return None

con.create_function("transaction_notification", 2, on_processed_transaction)
```

C 예제:

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

> **주의:** 콜백 함수는 **작업자 스레드**에서 호출됩니다!!
> 애플리케이션은 거기서 db 연결을 사용하지 않아야 하며 가능한 한 빨리 반환해야 합니다!
> 반환하기 전에 알림을 메인 스레드로 보낼 수 있습니다


### 업데이트 알림

블록체인에서 새 블록을 수신하여 로컬 데이터베이스에 업데이트가 발생할 때마다 애플리케이션에 알릴 수 있습니다.

알림은 `사용자 정의 함수`를 사용하여 설정되는 콜백 함수를 사용하여 수행됩니다:

Python 예제:

```python
def on_db_update(arg):
  print "update received"
  return None

con.create_function("update_notification", 1, on_db_update)
```

C 예제:

```C
static void on_db_update(sqlite3_context *context, int argc, sqlite3_value **argv){
  puts("update received");
  sqlite3_result_null(context);
}

sqlite3_create_function(db, "update_notification", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
  NULL, &on_db_update, NULL, NULL);
```

> **주의:** 콜백 함수는 **작업자 스레드**에서 호출됩니다!!
> 애플리케이션은 거기서 db 연결을 사용하지 않아야 하며 가능한 한 빨리 반환해야 합니다!
> 반환하기 전에 알림을 메인 스레드로 보낼 수 있습니다


## 블록 간격

블록은 각 라운드에서 무작위로 선택된 노드에 의해 생성됩니다.

AergoLite는 빈 블록을 생성하지 않습니다. 처리할 트랜잭션이 없으면 블록이 생성되지 않습니다.

트랜잭션이 노드에 도착하면(타이머가 아직 활성화되지 않은 경우) 새 블록을 생성하기 위한 타이머가 활성화됩니다.

이 타임아웃 간격은 `block_interval` 매개변수를 사용하여 URI를 통해 구성할 수 있습니다.

값은 밀리초로 해석됩니다.

```
"file:test.db?blockchain=on&block_interval=1000"
```

블록 간격이 지정되지 않으면 라이브러리는 기본값인 3초를 사용합니다.


## 제한 사항

이 버전은 노드 간 통신을 위해 완전히 연결된 네트워크를 사용합니다. 최대 200개의 노드(자동화된 테스트를 통해 확인됨) 및 아마도 더 많은 노드에서 작동합니다. 향후 더 많은 노드를 지원하기 위해 고스립 기반 프로토콜도 포함할 수 있습니다.

각 데이터베이스 파일에 대한 연결은 1개만 가능합니다. db 파일에 액세스해야 하는 많은 애플리케이션이 있는 경우 각 애플리케이션은 별도의 노드로 구성된 자체 데이터베이스 복사본을 가져야 합니다.

rowid 테이블(정수를 기본 키로 사용하는 테이블)의 행 번호는 SQLite와 다릅니다. 첫 32비트는 노드 ID이고 나머지 32비트는 노드당 순차적입니다. 이것은 또한 각 노드가 각 rowid 테이블에서 최대 2^32개의 행을 생성할 수 있음을 의미합니다.

모든 다중 마스터 복제 시스템에서와 같이 충돌이 발생할 수 있습니다. 일부 경우에는 전체 트랜잭션이 중단될 수 있으므로 이를 고려하세요. 위에서 앱이 트랜잭션 상태를 확인하는 방법을 참조하세요.


## 라이선스

AergoLite는 다음 두 옵션 중 하나로 릴리스됩니다:

1. AGPLv3

이것은 애플리케이션이 소스 코드를 릴리스하고 호환되는 GPL 하에 게시하는 것을 포함하여 이 라이선스를 준수해야 함을 의미합니다.

2. 상업용 라이선스

위 조건이 요구 사항에 맞지 않거나 더 나은 지원 및 서비스를 원하는 경우 상업용 라이선스를 취득하려면 문의하세요.


## 우리에 대해

AergoLite는 Bernardo Ramos가 다음에서 개발했습니다:

[![aergo logo](https://user-images.githubusercontent.com/7624275/100549737-8e89c500-3253-11eb-96b3-585916ed0883.png)](https://aergo.io/)

후원:

[![HPP logo](https://github.com/user-attachments/assets/fc0549c7-2a8d-4df9-aa74-b72156a8eae8)](https://hpp.io/)


## 지원

우리 [포럼](https://aergolite-forum.aergo.io/)에서 낮은 우선순위 지원을 제공합니다.

특별한 엔터프라이즈 지원도 제공됩니다. 이메일로 문의하세요: aergolite *AT* aergo *DOT* io

