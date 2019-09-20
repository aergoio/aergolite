# AergoLite: SQLite with blockchain!

> *The easiest way to deploy a blockchain for data storage on your app*

AergoLite allow us to have a replicated SQLite database enforced by a private and lightweight blockchain.

Each app has a local replica of the database.

New database transactions are distributed to all the peers and once they reach a consensus on the order of execution all the nodes can execute the transactions.

As the order of execution of these transactions are the same, all the nodes have the same resulting database content.

Apps can also write to the local database when they are off-line. The database transactions are stored on a local queue and sent to the network once the connectivity is reestablished.

As in any SQLite database, only the last state of the database is stored on the file. There is no stored snapshot for each previous states. This is important to reduce storage size on small devices.

Only the blockchain itself has the full history, and it stores the SQL commands to generate the database.

The resulting increase in size of the database is 10-15% compared to the same SQLite database without the blockchain.

The network traffic is lightweight to reduce energy consumption. New packets are transferred only when there are new database transactions.

You can easily choose which consensus protocol to use between 2 available: 1 leader based and 1 gossip based.


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

Most of these languages are supported via a wrapper.

## Compiling and installing

### On Linux and Mac

1. Install libuv

```
sudo apt-get install automake libtool libreadline-dev -y
git clone https://github.com/libuv/libuv --depth=1
cd libuv
./autogen.sh
./configure
make
sudo make install
sudo ldconfig
cd ..
```

2. Install binn

```
git clone https://github.com/liteserver/binn
cd binn
make
sudo make install
cd ..
```

3. Install AergoLite

```
git clone https://github.com/aergoio/aergolite
cd aergolite
make
sudo make install
cd -
```

### On Windows using MinGW

1. Compile libuv

```
git clone https://github.com/libuv/libuv --depth=1
cd libuv
sh autogen.sh
./configure
make
cd ..
```

2. Compile binn

```
git clone https://github.com/liteserver/binn
cd binn
make
cd ..
```

3. Compile AergoLite

```
git clone https://github.com/aergoio/aergolite
cd aergolite
make
cd -
```

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


## Manual Testing

You can test it using the SQLite shell.

Open an empty local database on each device:

```
sqlite3
.log stdout
.open "file:test.db?blockchain=on&discovery=local:4329"
```


## Using

The compiled library has support for both native SQLite database files and for SQLite databases with blockchain support, so the application can open native SQLite databases and ones with blockchain at the same time.

The library works exacly the same way for a normal SQLite database.

For opening a database with blockchain support we inform the library using a URI parameter: `blockchain=on`

We also need to inform the node discovery method. This is done via the `discovery` parameter.

Here is an example using discovery via UDP on the local network. You can choose any port:

```
"file:test.db?blockchain=on&discovery=local:4329"
```

All nodes from the same network must use the same node discovery method and the same UDP port.


## Retrieving status

There are 2 ways to retrieve status:

1. Locally via PRAGMA commands
2. Remotely via status requests

The status are divided in 2 parts:

### Blockchain status

This has information about the local blockchain and database.

It can be queried using the command:

```
PRAGMA blockchain_status
```

That will return a result in JSON format like the following:

```
{
"use_blockchain": true,
"blockchain": {
  "num_transactions": 125,
  "last_transaction": {
    "id": 3476202423059134961,
    "hash": "..."
  }
},
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

For the `raft-like` consensus protocol the result is in this format:

```
{
"use_blockchain": true,
"node_id": 692281563,
"is_leader": false,
"leader": 1772633815,
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
"is_leader": false,
"peers": [{
  "node_id": 692281563,
  "is_leader": false,
  "conn_type": "outgoing",
  "address": "192.168.1.45:4329"
},{
  "node_id": 1617834522,
  "is_leader": true,
  "conn_type": "outgoing",
  "address": "192.168.1.42:4329"
},{
  "node_id": 1772633815,
  "is_leader": false,
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
