# AergoLite: SQLite with blockchain!

> *The easiest way to deploy a blockchain for data storage on your app or device*

AergoLite allows us to have a replicated SQLite database secured by a private and lightweight blockchain.

Each app has a local replica of the database.

New database transactions are distributed to all the peers and once they reach a consensus on the order of execution all the nodes can execute the transactions.

As the order of execution of these transactions is the same, all the nodes have the same resulting database content.

Apps can also write to the local database when they are off-line. The database transactions are stored on a local queue and sent to the network once the connectivity is reestablished.

The application will read the new state of the database after the off-line modifications, and it can check if the off-line transactions were processed by the global consensus. If rejected, the database will return to the previous state.

Once the consensus is reached, the internal previous states are deleted.

AergoLite uses **special blockchain technology** focused on **resource constrained devices**.

It is not like Bitcoin! No proof-of-work is used and the nodes do not need to keep all the history of blocks and transactions.

AergoLite uses *absolute finality*. Once the nodes reach consensus on a new block they can discard the previous one. Only the last block is kept on the nodes. Optionally we can setup some nodes to keep all the history for audit reasons.

It also uses a *hash of the database state*. This lets the nodes to check if they have exactly the same content on the database, protects against intentional modifications on the database file and also works as an integrity check to detect failures on the storage media.

This final hash is updated using only the modified pages on each new block. It does not need to load the entire database to calculate the new state. The integrity check is also only made when a new db page is loaded. This drastically increases the database performance.

The resulting solution does not require big disk storage, uses low processor time and low RAM.

The network traffic is also lightweight to reduce energy consumption. New packets are transferred only when there are new database transactions.

This technology allows us to run a real private blockchain on IoT and mobile devices.

AergoLite is also easy to use. You do not need to know how a blockchain works to use it.

Supported OS:

* Mac
* Linux
* Windows
* Android
* iOS
* OpenWrt

Supported programming languages:

* C
* C++
* Java
* Javascript / node.js
* Python
* .Net (C# and VB)
* Ruby
* Swift
* Lua
* Go

And probably any other that has support for SQLite.

Most of these languages are supported via wrappers.


## Compiling and installing

### On Linux and Mac

Copy and paste this on a terminal:

```
# Install tools

sudo apt-get install git gcc make automake libtool libreadline-dev -y

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
./configure
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

### On Windows using MinGW

Copy and paste this on a MinGW terminal:

```
# Compile libuv

git clone --depth=1 https://github.com/libuv/libuv
cd libuv
sh autogen.sh
./configure
make
cd ..

# Compile binn

git clone --depth=1 https://github.com/liteserver/binn
cd binn
make
cd ..

# Compile libsecp256k1-vrf

git clone --depth=1 https://github.com/aergoio/secp256k1-vrf
cd secp256k1-vrf
./autogen.sh
./configure
make
cd ..

# Compile AergoLite

git clone --depth=1 https://github.com/aergoio/aergolite
cd aergolite
make
cd -
```

### For Android

Use the [SQLite Android Bindings](https://github.com/aergoio/aergolite-tools/tree/master/wrappers/SQLite_Android_Bindings)
to generate an `aar` file and then include it on the Android Studio project.


### For iOS

Generate static and dynamic libraries with the command:

```
./makeios
```

They can be included as a module on iOS projects.


## Automated Tests

These tests simulate up to 100 nodes on your computer.

Before running the tests you will need to increase the limit of open files on your terminal:

```
ulimit -Sn 16000
```

Then you can run the automated tests with:

```
make test
```

For printing debug messages to a log file you must recompile the library in debug mode before running the tests:

```
make clean
make debug
```

Running the tests with Valgrind is also available:

```
make valgrind
```


## Using

The compiled library has support for both native SQLite database files and for SQLite databases with blockchain support, so the application can open native SQLite databases and ones with blockchain at the same time.

The library works exactly the same way for a normal SQLite database.

For opening a database with blockchain support we use a URI parameter: `blockchain`

Example:

```
"file:test.db?blockchain=on"
```


## Blockchain Administrator

AergoLite implements a private blockchain. This means that you or your organization can have
your own private blockchain(s) in which you have control of what can happen.

The entity who has the control over the blockchain is called the blockchain administrator. It
is an user that is identified by a private + public key pair.

The blockchain administrator can:

* Add nodes to the blockchain network
* Remove nodes from the blockchain network
* Execute reserved SQL commands

In future versions it will also be able to:

* Authorize nodes to execute blocked SQL commands
* Add users to the network
* Authorize users to execute specific SQL commands
* Create smart contracts

You will need to inform the public key of the blockchain administrator on each participating node.

This ensures that:

1. The nodes can verify that the received commands come from the blockchain administrator
2. A node will not join a network that is not owned by you

This is done via the `admin` parameter, where the public key must be in hex format.

Example:

```
"file:test.db?blockchain=on&admin=95F9AB75CA1..."
```


## Immutability

Trust based solutions are not secure as an attacker having control of a single node can delete and
overwrite data on the database.

A real trustless and immutable blockchain should control what nodes can do.

AergoLite is by default append-only for nodes. They can only execute `INSERT INTO` SQL commands.
Only the administrator is able to execute all the SQL commands.

Future versions may allow nodes to execute smart contracts created by the administrator that can
include any SQL command.


## Private key protection

Each node generates a distinct private + public key pair. They are identified and authorized via their public key.

For now the private key for each node is stored encrypted on the device. Future versions may support hardware based private key protection.

We need to inform the password used for decrypting the private key using the `password` URI parameter.

Example:

```
"file:test.db?blockchain=on&admin=95F9AB75CA1...&password=testing"
```

The password can be different on each node.

The blockchain administrator is responsible for storing its private key in a secure way. We recommend not storing it on one of the blockchain nodes and not in plain format. It should be encrypted and stored on an external device or media. A paper wallet is also a good idea. The best option is to use a hardware wallet.


## Hardware wallet

![ledger-app-aergolite](https://user-images.githubusercontent.com/7624275/73798570-ec639280-4791-11ea-8a1f-7cb3ea836ec8.jpg)

For the higher level of security the blockchain administrator can protect its private key using a Ledger Nano S.

On this case he/she would use the device to sign its transactions.

For more details check the [instructions](https://github.com/aergoio/aergolite/wiki/Using-a-Hardware-Wallet)


## Node discovery

A node needs to discover its peers on the blockchain network.

We specify the node discovery method using the `discovery` URI parameter.

There are 2 options of node discovery:

### Local UDP broadcast

This method sends an UDP broadcast packet on the local area network to the specified port.

All nodes from the same local network must use the same port number.

Example:

```
"file:test.db?blockchain=on&discovery=local:4329"
```

### Known nodes

On this method some nodes have a fixed IP address and the other nodes connect to them.

The nodes with known address must also bind to a defined TCP port. This is informed using the `bind` parameter.

Example URI for a "known" node:

```
"file:test.db?blockchain=on&bind=5501"
```

The other nodes must have an explicit `discovery` parameter containing the address of the known nodes.

Example URI for the other nodes:

```
"file:test.db?blockchain=on&discovery=<ip-address>:<port>"
```

We can also specify the addresses of more known nodes:

```
"file:test.db?blockchain=on&discovery=<ip-address1>:<port1>,<ip-address2>:<port2>"
```

Once a connection is established and the node is accepted they exchange a list of active nodes addresses. 

### Mixing both methods

We can also use the 2 above methods at the same time. This can be useful when we have some nodes on the LAN and others that are outside.

We can fix the address of one or more nodes so they can be found by nodes from outside the local network.

Nodes on the LAN will discover local nodes via UDP broadcast and can either connect to known nodes outside the LAN or receive connections from them.

Known nodes can bind to a port, find local nodes via broadcast and also connect to external known nodes. Example:

```
"file:test.db?blockchain=on&bind=1234&discovery=local:1234,<outside_ip1>:<port1>,<outside_ip2>:<port2>"
```

Nodes without fixed address will use the local discovery and the connection to outside known nodes:

```
"file:test.db?blockchain=on&discovery=local:1234,<outside_ip1>:<port1>,<outside_ip2>:<port2>"
```

If nodes on this LAN are just receiving connections from outside, then the `discovery` parameter must contain just the local discovery method.


## Listing connected nodes

You can list the nodes on your private blockchain network using the command:

```
PRAGMA nodes
```

It will list all authorized nodes, connected or not, and also connected nodes that are not yet authorized.


## Adding nodes to the network

After listing the connected nodes with the above command the blockchain administrator can authorize nodes using the command:

```
PRAGMA add_node=<public key>
```

The public key must be in hex format.

Only the blockchain administrator can add nodes to the network.

The first node to be authorized must be the one in which the command is being executed.

The authorizations for new nodes must be executed on nodes that are already authorized.

The above command will fire the user transaction signature callback where the transaction must be signed using the blockchain administrator private key.


## Signing transactions

On AergoLite the blockchain transactions are built using the SQL commands from the database transactions.

Each database transaction generates one blockchain transaction.

These transactions need to be signed to be accepted by the network and included on the blockchain.

Two entities can sign transactions:

* the administrator
* each authorized node

If the transaction requires special rights, the AergoLite library will fire the user transaction sign callback function. Otherwise it will automatically sign it using the node's private key.

At least one node on your network need to register a function that will be used to sign transactions from the administrator

Example in Python:

```python
def on_sign_transaction(data):
  print "txn to be signed: " + data
  signature = sign(data, privkey)
  return hex(pubkey) + ":" + hex(signature)

con.create_function("sign_transaction", 1, on_sign_transaction)
```

> **ATTENTION:** The callback function is called by the **worker thread**!!
> Your application must sign the transaction and return as fast as possible!

If a special command that requires admin privilege is executed on a node but not signed by him then the transaction will be rejected.


## Retrieving status

There are 2 ways to retrieve status:

1. Locally via PRAGMA commands
2. Remotely sending status requests via UDP packets

The status are divided in 2 parts:

### Blockchain status

This has information about the local blockchain and database.

It can be queried locally using the command:

```
PRAGMA blockchain_status
```

It will return a result in JSON format like the following:

```
{
"use_blockchain": true,
"blockchain": {
  "last_block": 125,
  "state_hash": "..."
},
"local_nonce": 3,
"local_changes": {
  "num_transactions": 3
}
}
```

This status information does not depend on the selected consensus protocol. It has always the above format.


### Network and consensus protocol status

It can be queried using the command:

```
PRAGMA protocol_status
```

The information returned depends on the selected consensus protocol.

For the `mini-raft` consensus protocol the result is in this format:

```
{
"use_blockchain": true,
"node_id": 692281563,
"num_peers": 3,
"mempool": {
  "num_transactions": 0
},
"sync_down_state": "in sync",
"sync_up_state": "in sync"
}
```

We can also return extended information using the command:

```
PRAGMA protocol_status(1)
```

In this case the returned data will contain the list of connected nodes:

```
{
"use_blockchain": true,
"node_id": 1506405147,
"peers": [{
  "node_id": 692281563,
  "conn_type": "outgoing",
  "address": "192.168.1.45:4329"
},{
  "node_id": 1617834522,
  "conn_type": "outgoing",
  "address": "192.168.1.42:4329"
},{
  "node_id": 1772633815,
  "conn_type": "incoming",
  "address": "192.168.1.47:38024"
}],
"mempool": {
  "num_transactions": 0
},
"sync_down_state": "unknown",
"sync_up_state": "in sync"
}
```


### Application defined node information

Your application can set node specific information using this command:

```
PRAGMA node_info=<text>
```

The text value can be a single node identifier or it can contain many information serialized in any text format. Only your applications will use it.

This information is kept on memory locally and also sent to the connected peers. It is not saved on the database and it is dynamic: the next time this command is executed with a different value it will replace the previous one.

The last set value can be retrieved locally using the `PRAGMA node_info` command.

It will also appear in the result of the `PRAGMA protocol_status(1)` command on its peer nodes.


### Last nonce

Each generated transaction on a specific node has an unique incremental nonce.

It is possible to retrieve the current node's last nonce with the command:

```
PRAGMA last_nonce
```

If the returned number is zero it means that this node has not generated any transaction yet.


### Transaction Status

To retrieve the status of a local transaction:

```
PRAGMA transaction_status(<nonce>)
```

Where `<nonce>` should be replaced by the transaction's nonce. eg: `PRAGMA transaction_status(3)`

It will return

On full nodes: (not yet implemented)

* `unprocessed`: the transaction was not yet processed by the network
* `included`: a consensus was reached and the transaction was included on a block
* `rejected`: a consensus was reached and the transaction was rejected

On pruned nodes:

* `unprocessed`: the transaction was not yet processed by the network
* `processed`: the transaction was processed by the network and a consensus was reached on its result

Pruned nodes do not keep information about specific transactions.


### Transaction Notification

To be informed whether a specific transaction was included on a block or rejected the application must use a callback function. It is set up as an `user defined function`:

Example in Python:

```python
def on_processed_transaction(nonce, status):
  print "transaction " + str(nonce) + ": " + status
  return None

con.create_function("transaction_notification", 2, on_processed_transaction)
```

Example in C:

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

> **ATTENTION:** The callback function is called by the **worker thread**!!
> Your application should not use the db connection there and it must return as fast as possible!
> It can send the notification to the main thread before returning


### Update Notification

Your application can be informed whenever an update occurred on the local db due to receiving a new block on the blockchain.

The notification is made using a callback function that is set using an `user defined function`:

Example in Python:

```python
def on_db_update(arg):
  print "update received"
  return None

con.create_function("update_notification", 1, on_db_update)
```

Example in C:

```C
static void on_db_update(sqlite3_context *context, int argc, sqlite3_value **argv){
  puts("update received");
  sqlite3_result_null(context);
}

sqlite3_create_function(db, "update_notification", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
  NULL, &on_db_update, NULL, NULL);
```

> **ATTENTION:** The callback function is called by the **worker thread**!!
> Your application should not use the db connection there and it must return as fast as possible!
> It can send the notification to the main thread before returning


## Block interval

Blocks are created by randomly selected nodes on each round when using the `no-leader` consensus protocol (default).

AergoLite does not produce empty blocks. If there is no transaction to be processed, then no block is created.

A timer for creating a new block is activated when a transaction arrives on the nodes (and the timer is not yet active).

This timeout interval can be configured via URI using the `block_interval` parameter.

The value is interpreted as milliseconds.

```
"file:test.db?blockchain=on&block_interval=1000"
```

If the block interval is not specified then the library will use a default value of 3 seconds.


## Limitations

This first version uses a fully connected network for communication between the nodes.

It works with up to 100 nodes on the automated tests.

Future versions will also contain a gossip based protocol to support thousands of nodes.

Each node can create 2^32 rows on each rowid table.
