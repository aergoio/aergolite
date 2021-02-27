<p align="right"><a href="README.md">English</a>&nbsp;&nbsp;&nbsp;<a href="README-zh.md">中文</a></p>
<p align="center"><img width="65%" src="https://user-images.githubusercontent.com/7624275/92685476-2390b800-f30e-11ea-9edc-980b0e66c0ad.png" alt="AergoLite"></p>

<h1 align="center">SQLite com Blockchain</h1>

<blockquote align="center"><p><em>A maneira mais fácil de implantar um blockchain para armazenamento de dados relacionais em seu aplicativo ou dispositivo</em></p></blockquote>

O AergoLite nos permite ter um banco de dados SQLite replicado protegido por um blockchain privado e leve.

Cada aplicativo possui uma réplica local do banco de dados.

Novas transações do banco de dados são distribuídas para todos os pares e uma vez que eles alcançam um consenso sobre a ordem de execução, todos os nós executam as transações. Como a ordem de execução dessas transações é a mesma, todos os nós têm o mesmo conteúdo do banco de dados resultante.

Os aplicativos também podem gravar no banco de dados local quando estão off-line. As transações do banco de dados são armazenadas em uma fila local e enviadas para a rede assim que a conectividade for restabelecida. O aplicativo fará a leitura do novo estado do banco de dados após as modificações off-line, podendo verificar se as transações off-line foram processadas pelo consenso global. Se rejeitado, o banco de dados retornará ao estado anterior.

AergoLite usa **tecnologia blockchain especial** focada em **dispositivos com recursos limitados**.

O protocolo de consenso usa uma **função aleatória verificável (VRF)** para determinar qual nó produzirá o próximo bloco, e os nós não podem descobrir qual nó foi selecionado com antecedência. Tornando-o seguro contra ataques de negação de serviço (DoS).

AergoLite usa **finalidade absoluta**. Uma vez que os nós chegam a um consenso sobre um novo bloco e as transações são confirmadas, não há caminho de volta. Também não há necessidade de criar novos blocos se não houver transações a serem processadas (ao contrário da finalidade probabilística).

Apenas o último bloco é necessário para verificar o blockchain e a integridade do estado do banco de dados, portanto, os nós não precisam manter e verificar todo o histórico de blocos e transações.
Também é possível configurar alguns nós para manter todo o histórico por motivos de auditoria.

Ele também usa um **hash do estado do banco de dados**. Isso permite que os nós verifiquem se eles têm exatamente o mesmo conteúdo no banco de dados, protege contra modificações intencionais no arquivo do banco de dados e também funciona como uma verificação de integridade para detectar falhas na mídia de armazenamento.

Este hash final é atualizado usando apenas as páginas modificadas em cada novo bloco. Não é necessário carregar todo o banco de dados para calcular o novo estado. A verificação de integridade também só é feita quando uma nova página do banco de dados é carregada. Isso aumenta drasticamente o desempenho do banco de dados.

A solução resultante não requer grande armazenamento em disco, usa pouco tempo de processador e pouca memória.

O tráfego de rede também é leve para reduzir o consumo de energia. Novos pacotes são transferidos apenas quando há novas transações de banco de dados.

Essa tecnologia nos permite ter um blockchain privado em IoT e dispositivos móveis.

O AergoLite também é fácil de usar. Você não precisa saber como funciona um blockchain para usá-lo.

Sistemas Operacionais suportados:

* Mac
* Linux
* Windows
* Android
* iOS
* OpenWrt

Linguagens de programação suportadas:

* C
* C++
* Java
* Javascript (Node.js)
* Python
* .Net (C# e VB)
* Ruby
* Swift
* Lua
* Go

E provavelmente qualquer outra linguagem que tenha suporte para SQLite.

A maioria dessas linguagens é suportada por meio de wrappers existentes.


## Binários pré-compilados

Verifique em [releases](https://github.com/aergoio/aergolite/releases)


## Imagens Docker

* [Base](https://hub.docker.com/r/aergo/aergolite) (com bibliotecas e o shell SQLite, também pode ser usado para adicionar aplicativos C / C++)
* [Python](https://hub.docker.com/r/aergo/aergolite-python)
* [Node.js](https://hub.docker.com/r/aergo/aergolite-nodejs)

Para construir as imagens localmente:

```
make docker
```


## Compilando e instalando

### No Linux e Mac

Primeiro instale as ferramentas necessárias com este comando:

```
sudo apt-get install git gcc make automake libtool libreadline-dev -y
```

Em seguida, copie e cole em um terminal:

```
# Instalar libuv

git clone --depth=1 https://github.com/libuv/libuv
cd libuv
./autogen.sh
./configure
make
sudo make install
sudo ldconfig
cd ..

# Instalar binn

git clone --depth=1 https://github.com/liteserver/binn
cd binn
make
sudo make install
cd ..

# Instalar libsecp256k1-vrf

git clone --depth=1 https://github.com/aergoio/secp256k1-vrf
cd secp256k1-vrf
./autogen.sh
./configure --disable-benchmark
make
sudo make install
cd ..

# Instalar AergoLite

git clone --depth=1 https://github.com/aergoio/aergolite
cd aergolite
make
sudo make install
cd -
```

### No Windows usando MinGW

Copie e cole em um terminal MSYS2 MinGW:

```
# Compilar libuv

git clone --depth=1 https://github.com/libuv/libuv
cd libuv
./autogen.sh
./configure
make
make install
cd ..

# Compilar binn

git clone --depth=1 https://github.com/liteserver/binn
cd binn
make
make install
cd ..

# Compilar libsecp256k1-vrf

git clone --depth=1 https://github.com/aergoio/secp256k1-vrf
cd secp256k1-vrf
./autogen.sh
./configure --with-bignum=no --disable-benchmark
make
make install
cd ..

# Compilar AergoLite

git clone --depth=1 https://github.com/aergoio/aergolite
cd aergolite
make
make install
```

### Para Android

Use o [SQLite Android Bindings](https://github.com/aergoio/aergolite-tools/tree/master/wrappers/SQLite_Android_Bindings)
para gerar um arquivo `aar` e incluí-lo no projeto Android Studio.
Um exemplo de projeto está disponível [aqui](https://github.com/aergoio/aergolite-tools/tree/master/projects/AndroidStudio-NativeInterface)


### Para iOS

Gere bibliotecas estáticas e dinâmicas com o comando:

```
./makeios
```

Eles podem ser incluídos como um módulo em projetos iOS.
Você também pode copiá-los para a subpasta `AergoLite` do
Wrapper [AergoLite.swift](https://github.com/aergoio/AergoLite.swift)


## Testes automatizados

Esses testes simulam até 100 nós em seu computador.

Antes de executar os testes, você precisará aumentar o limite de arquivos abertos em seu terminal:

```
ulimit -Sn 16000
```

Então você pode executar os testes automatizados com:

```
make test
```

Para imprimir mensagens de depuração em um arquivo de log, você deve recompilar a biblioteca no modo de depuração antes de executar os testes:

```
make clean
make debug
```

A execução de testes com Valgrind também está disponível:

```
make valgrind
```


## Usando

A biblioteca compilada tem suporte para arquivos de banco de dados SQLite nativos e para bancos de dados SQLite com suporte a blockchain, de modo que o aplicativo pode abrir bancos de dados SQLite nativos e aqueles com blockchain ao mesmo tempo.

A biblioteca funciona exatamente da mesma maneira para um banco de dados SQLite normal.

Para abrir um banco de dados com suporte a blockchain, usamos um parâmetro URI: `blockchain`

Exemplo:

```
"file:test.db?blockchain=on"
```


## Administrador do Blockchain

AergoLite implementa um blockchain privado. Isso significa que você ou sua organização podem ter
seu(s) próprio(s) blockchain(s) privado(s) nos quais você tem controle do que pode acontecer.

A entidade que tem controle sobre o blockchain é chamada de administrador do blockchain. Isto
é um usuário que possui seu próprio par de chaves pública e privada.

O administrador do blockchain pode:

* Adicionar nós à rede blockchain
* Executar comandos SQL reservados

Em versões futuras, também será capaz de:

* Remover nós da rede blockchain
* Adicionar usuários à rede
* Criar contratos inteligentes para permitir que os nós executem comandos SQL bloqueados

Você precisará informar a chave pública do administrador do blockchain em cada nó participante.

Isso garante que:

1. Os nós podem verificar se os comandos recebidos vêm do administrador do blockchain
2. Um nó não se conectará a uma rede que não seja sua

Isso é feito através do parâmetro `admin`, onde a chave pública pode estar no formato nativo ou hexadecimal.

Exemplo:

```
"file:test.db?blockchain=on&admin=95F9AB75CA1..."
```


## Imutabilidade

Soluções baseadas em confiança permitem que usuários ou nós específicos executem qualquer comando SQL no banco de dados.
Eles não são seguros porque um invasor adquirindo o controle de um único nó pode excluir e/ou
sobrescrever dados em toda a rede.

Um blockchain real sem confiança em um nó específico e imutável deve controlar o que os nós podem fazer.

O AergoLite, por padrão, permite que os nós apenas adicionem dados no banco de dados. Eles não podem atualizer ou apagar dados. Eles só podem executar comandos SQL do tipo `INSERT`.
Apenas o administrador pode executar todos os comandos SQL.

Versões futuras podem permitir que os nós executem contratos inteligentes criados pelo administrador que podem incluir qualquer comando SQL.

Toda essa proteção impõe a necessidade à um invasor de controlar a maioria dos nós da rede para poder atacá-la. Quanto mais nós na rede, mais difícil é o ataque.


## Proteção de chave privada

Cada nó gera um par distinto de chave pública e privada. Eles são identificados e autorizados por meio de sua chave pública.

Por enquanto, a chave privada de cada nó é armazenada criptografada no dispositivo. Versões futuras podem oferecer suporte à proteção de chave privada baseada em hardware.

Precisamos informar a senha usada para descriptografar a chave privada usando o parâmetro URI `password`.

Exemplo:

```
"file:test.db?blockchain=on&admin=95F9AB75CA1...&password=testing"
```

A senha pode ser diferente em cada nó.

Opcionalmente, seu aplicativo pode ser responsável por gerar e armazenar a chave privada para cada nó. Neste caso pode informar a chave privada em formato hexadecimal para a biblioteca via URI usando o parâmetro `privkey`:

```
"file:test.db?blockchain=on&admin=95F9AB75CA1...&privkey=AABBCCDD..."
```

Isso pode ser útil ao usar AergoLite em contêiners, onde cada instância deve ter uma chave privada diferente.

O administrador do blockchain é responsável por armazenar sua chave privada de forma segura. Recomendamos não armazená-la em um dos nós do blockchain e também não em formato simples. Deve ser criptografada e armazenada em um dispositivo externo ou mídia. Uma carteira de papel também é uma boa ideia. A melhor opção é usar uma carteira de hardware.


## Carteira de hardware

![ledger-app-aergolite-sql](https://user-images.githubusercontent.com/7624275/75842624-98a79180-5daf-11ea-8427-f0c3e7788f41.jpg)

Para o maior nível de segurança, o administrador do blockchain pode proteger sua chave privada usando um Ledger Nano S.

Nesse caso, ele usa o dispositivo para assinar suas transações.

Para mais detalhes, verifique as [instruções](https://github.com/aergoio/aergolite/wiki/Using-a-Hardware-Wallet)


## Descoberta de nós

Um nó precisa descobrir seus pares na rede blockchain.

Especificamos o método de descoberta de nó usando o parâmetro URI `discovery`.

Existem 2 opções de descoberta de nós:

### 1. Transmissão UDP local

Este método envia um pacote de transmissão UDP na rede local para a porta especificada.

Todos os nós da mesma rede local devem usar o mesmo número de porta.

Exemplo:

```
"file:test.db?blockchain=on&discovery=local:4329"
```

### 2. Nós conhecidos

Neste método, alguns nós têm um endereço IP fixo e os outros nós se conectam a eles.

Os nós com endereço conhecido também devem se ligar a uma porta TCP definida. Isso é informado usando o parâmetro `bind`.

Exemplo para um nó conhecido:

```
"file:test.db?blockchain=on&bind=5501"
```

Os outros nós devem ter um parâmetro `discovery` explícito contendo o endereço dos nós conhecidos.

Exemplo para os outros nós:

```
"file:test.db?blockchain=on&discovery=<endereço-IP>:<porta>"
```

Também podemos especificar os endereços de vários nós conhecidos:

```
"file:test.db?blockchain=on&discovery=<endereço-IP>:<porta>,<endereço-IP>:<porta>"
```

Depois que uma conexão é estabelecida e o nó é aceito, eles trocam uma lista de endereços de nós ativos.

### 3. Misturando os dois métodos

Também podemos usar os 2 métodos acima ao mesmo tempo. Isso pode ser útil quando temos alguns nós na LAN e outros externos.

Podemos fixar o endereço de um ou mais nós para que eles possam ser encontrados por nós de fora da rede local.

Os nós na LAN descobrirão os nós locais via transmissão UDP e podem se conectar a nós conhecidos fora da LAN ou receber conexões deles.

Os nós conhecidos podem se ligar a uma porta, encontrar nós locais por meio de transmissão e também se conectar a nós externos conhecidos. Exemplo:

```
"file:test.db?blockchain=on&bind=1234&discovery=local:1234,<outside_ip1>:<port1>,<outside_ip2>:<port2>"
```

Os nós sem endereço fixo usarão a descoberta local e a conexão com nós conhecidos externos:

```
"file:test.db?blockchain=on&discovery=local:1234,<outside_ip1>:<port1>,<outside_ip2>:<port2>"
```

Se os nós nesta LAN estão apenas recebendo conexões de fora, o parâmetro `discovery` deve conter apenas o método de descoberta local.


## Listando nós conectados

Você pode listar os nós em sua rede blockchain privada usando o comando:

```
PRAGMA nodes
```

Ele listará todos os nós autorizados, conectados ou não, e também os nós conectados que ainda não foram autorizados.


## Adicionando nós à rede

Depois de listar os nós conectados com o comando acima, o administrador do blockchain pode autorizar os nós usando o comando:

```
PRAGMA add_node="<chave pública>"
```

Apenas o administrador do blockchain pode adicionar nós à rede.

O primeiro nó a ser autorizado deve ser aquele no qual o comando está sendo executado.

As autorizações para outros nós devem ser executadas em nós já autorizados.

O comando acima será enviado para o dispositivo Ledger para ser assinado se o dispositivo estiver conectado, caso contrário, ele irá disparar o callback de assinatura da transação onde a transação deve ser assinada usando a chave privada do administrador do blockchain.


## Especificando o tipo de nó

Por padrão, um nó é autorizado como um nó **leve** (não mantém o histórico de blocos). Para autorizá-lo como um nó **completo**, adicione `full:` antes da chave pública do nó:

```
PRAGMA add_node="full:<chave pública>"
```

Para modificar o tipo de um nó depois que ele já foi autorizado, use o comando `node_type`, com este formato:

```
PRAGMA node_type="<tipo>:<nós>"
```

O tipo pode ser `full` ou` light`. E "nós" é uma lista separada por vírgulas de identificadores de nós (chave pública ou id de nó) ou `*` para todos os nós autorizados.

Aqui estão alguns exemplos:

```
PRAGMA node_type="full:Am12..abc1"
PRAGMA node_type="full:Am12..abc1,Am12..abc2,Am12..abc3"
PRAGMA node_type="full:1287649477,3817592406,2373041549"
PRAGMA node_type="full:*"
PRAGMA node_type="light:1287649477"
```


## Assinando transações

No AergoLite, as transações blockchain são construídas usando os comandos SQL das transações do banco de dados.

Cada transação do banco de dados gera uma transação blockchain.

Essas transações precisam ser assinadas para serem aceitas pela rede e incluídas no blockchain.

Duas entidades podem assinar transações:

* O administrador
* Cada nó autorizado

Se a transação exigir direitos especiais, a biblioteca AergoLite a enviará para ser assinada pelo administrador. Caso contrário, ele o assinará automaticamente usando a chave privada do nó.

Se nenhum dispositivo Ledger for usado em sua rede, então pelo menos um nó precisa registrar uma função que será usada para assinar transações do administrador.

Exemplo em Python:

```python
def on_sign_transaction(data):
  print "transação a ser assinada: " + data
  signature = sign(data, privkey)
  return hex(pubkey) + ":" + hex(signature)

con.create_function("sign_transaction", 1, on_sign_transaction)
```

> **ATENÇÃO:** A função callback é chamada pelo **thread de trabalho** !!
> Seu aplicativo deve assinar a transação e retornar da função o mais rápido possível!

Se um comando especial que requer privilégio de administrador for executado em um nó, mas não for assinado por ele, a transação será rejeitada.


### Estado do banco de dados

Seu aplicativo **deve** verificar se o banco de dados local está pronto para leitura e gravação antes que qualquer comando SQL seja executado.

Essa verificação é feita com o comando:

```
PRAGMA db_is_ready
```

Ele retorna `1` quando o aplicativo pode ler e gravar no banco de dados, caso contrário, retorna` 0`.


### Estado do Blockchain

Ele contém informações sobre o blockchain local, o banco de dados local e a rede.

Ele pode ser consultado localmente usando o comando:

```
PRAGMA blockchain_status
```

Ele retornará um resultado no formato JSON como o seguinte:

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

### Estado da lista de espera (mempool)

Ele retorna as transações pendentes no mempool local.

```
PRAGMA mempool
```

Ele retornará um resultado no formato JSON como o seguinte:

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

### Informações de nó definidas pelo aplicativo

Seu aplicativo pode definir informações específicas para cada nó usando este comando:

```
PRAGMA node_info=<texto>
```

O valor de texto pode ser um identificador de nó único ou pode conter muitas informações serializadas em qualquer formato de texto. Apenas seus aplicativos irão usá-lo.

Essas informações são mantidas na memória localmente e também enviadas aos pontos conectados. Não é salvo no banco de dados e é dinâmico: na próxima vez que este comando for executado com um valor diferente, ele substituirá o anterior.

O último valor definido para este nó pode ser recuperado localmente usando o comando `PRAGMA node_info`.

É possível visualizar os valores dos nós conectados no resultado do comando `PRAGMA nodes` no campo `extra`.


### Último "nonce"

Cada transação gerada em um nó específico possui um número sequencial único chamado nonce.

É possível recuperar o último nonce do nó atual com o comando:

```
PRAGMA last_nonce
```

Se o número retornado for zero, significa que este nó ainda não gerou nenhuma transação.


### Status da transação

Para recuperar o status de uma transação local:

```
PRAGMA transaction_status(<nonce>)
```

Onde `<nonce>` deve ser substituído pelo nonce da transação. Por exemplo: `PRAGMA transaction_status (3)`

Este comando retorna:

Em nós completos:

* `unprocessed`: a transação ainda não foi processada pela rede
* `included`: um consenso foi alcançado e a transação foi incluída em um bloco
* `rejected`: um consenso foi alcançado e a transação foi rejeitada

Em nós leves:

* `unprocessed`: a transação ainda não foi processada pela rede
* `processed`: a transação foi processada pela rede e um consenso foi alcançado sobre o seu resultado

Os nós leves não mantêm informações sobre transações específicas.


### Notificação de transação processada

Para ser informado se determinada transação foi incluída em um bloco ou rejeitada, o aplicativo deve utilizar uma função de callback. É configurado como uma `função definida pelo usuário`:

Exemplo em Python:

```python
def on_processed_transaction(nonce, status):
  print "transação " + str(nonce) + ": " + status
  return None

con.create_function("transaction_notification", 2, on_processed_transaction)
```

Exemplo em C:

```C
static void on_processed_transaction(sqlite3_context *context, int argc, sqlite3_value **argv){
  long long int nonce = sqlite3_value_int64(argv[0]);
  char *status = sqlite3_value_text(argv[1]);

  printf("transação %lld: %s\n", nonce, status);

  sqlite3_result_null(context);
}

sqlite3_create_function(db, "transaction_notification", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
  NULL, &on_processed_transaction, NULL, NULL);
```

> **ATENÇÃO:** A função callback é chamada pelo **thread de trabalho** !!
> Seu aplicativo não deve usar a conexão ao banco de dados nela e
> deve assinar a transação e retornar da função o mais rápido possível!


### Notificação de atualização

Seu aplicativo pode ser informado sempre que ocorrer uma atualização no banco de dados local devido ao recebimento de um novo bloco no blockchain.

A notificação é feita usando uma função de retorno de chamada definida usando uma `função definida pelo usuário`:

Exemplo em Python:

```python
def on_db_update(arg):
  print "Atualização recebida"
  return None

con.create_function("update_notification", 1, on_db_update)
```

Exemplo em C:

```C
static void on_db_update(sqlite3_context *context, int argc, sqlite3_value **argv){
  puts("atualização recebida");
  sqlite3_result_null(context);
}

sqlite3_create_function(db, "update_notification", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
  NULL, &on_db_update, NULL, NULL);
```

> **ATENÇÃO:** A função callback é chamada pelo **thread de trabalho** !!
> Seu aplicativo não deve usar a conexão ao banco de dados nela e
> deve assinar a transação e retornar da função o mais rápido possível!


## Intervalo entre blocos

Os blocos são criados por nós selecionados aleatoriamente em cada rodada.

AergoLite não produz blocos vazios. Se não houver nenhuma transação a ser processada, nenhum bloco será criado.

Um cronômetro para a criação de um novo bloco é ativado quando uma transação chega aos nós (caso o cronômetro ainda não esteja ativo).

Este intervalo de tempo limite pode ser configurado via URI usando o parâmetro `block_interval`.

O valor é interpretado como milissegundos.

```
"file:test.db?blockchain=on&block_interval=1000"
```

Se o intervalo de bloco não for especificado, a biblioteca usará um valor padrão de 3 segundos.


## Limitações

Esta primeira versão usa uma rede totalmente conectada para comunicação entre os nós. Ele funciona com até 200 nós nos testes automatizados. Em breve, também conterá um protocolo blockchain com suporte a milhões de nós.

Apenas 1 conexão para cada arquivo de banco de dados. Se houver muitos aplicativos que precisam acessar o arquivo db, cada aplicativo deve ter sua própria cópia do banco de dados, configurada como um nó separado.

A numeração de linhas em tabelas rowid (aquelas que usam um número inteiro como chave primária) é diferente do SQLite. Os primeiros 32 bits são o id do nó e os 32 bits restantes são sequenciais por nó. Isso também significa que cada nó pode criar até 2^32 linhas em cada tabela rowid.

Como em qualquer sistema de replicação multimestre, podem ocorrer conflitos. A transação inteira pode ser abortada em alguns casos, então leve isso em consideração. Veja acima como o aplicativo pode verificar o estado da transação.


## Licenciamento

O AergoLite é lançado em uma das duas opções abaixo:

1. AGPLv3

Isso significa que seu aplicativo deve estar em conformidade com esta licença, incluindo o lançamento de seu código-fonte e a publicação sob uma GPL compatível.

2. LICENÇA COMERCIAL

Se as condições acima não atendem às suas necessidades, ou se você deseja um melhor suporte e serviços, entre em contato conosco para adquirir uma licença comercial.


## Sobre nós

AergoLite foi desenvolvido por Bernardo Ramos em:

[![aergo logo](https://user-images.githubusercontent.com/7624275/100549737-8e89c500-3253-11eb-96b3-585916ed0883.png)](https://aergo.io/)

Aergo tem um [blockchain público](https://aergoscan.io/) com suporte para [contratos inteligentes](https://ide.aergo.io/) em Lua.

Em breve, ele suportará armazenamento de dados relacionais e SQL em contratos inteligentes, atualmente disponíveis em nossa testnet e em redes blockchain privadas.


## Suporte

O suporte de baixa prioridade está disponível em nosso [Fórum](https://aergolite-forum.aergo.io/)

Suporte empresarial também está disponível. Contate-nos por e-mail: aergolite *AT* aergo *DOT* io
