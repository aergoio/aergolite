#include "db_functions.c"

#define majority(X)  X / 2 + 1

const int wait_time = 150000;

/****************************************************************************/

void delete_files(int n){
  int i;
  for(i=1; i<=n; i++){
    unlinkf("db%d.db", i);
    unlinkf("db%d.db-loc", i);
    unlinkf("db%d.db-con", i);
    unlinkf("db%d.db-shm", i);
    unlinkf("db%d.db-state", i);
    unlinkf("db%d.db-state-wal", i);
    unlinkf("db%d.db-state-shm", i);
  }
}

/****************************************************************************/

void print_nodes(char *title, int list[]){
  printf("%s=%d  { ", title, len_array_list(list));
  for(int i=0; list[i]; i++){
    if( i>0 ) printf(", ");
    printf("%d", list[i]);
  }
  puts(" }");
}

/****************************************************************************/
/****************************************************************************/

void test_5_nodes(int bind_to_random_ports){
  sqlite3 *db1, *db2, *db3, *db4, *db5;
  int rc, count, done;

  printf("test_5_nodes(random_ports=%d)...", bind_to_random_ports); fflush(stdout);

  /* delete the db files if they exist */

  delete_files(5);

  /* open the connections to the databases */

  assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db1)==SQLITE_OK );
  assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db2)==SQLITE_OK );

  if( bind_to_random_ports ){
  assert( sqlite3_open("file:db3.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302", &db3)==SQLITE_OK );
  assert( sqlite3_open("file:db4.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302", &db4)==SQLITE_OK );
  assert( sqlite3_open("file:db5.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302", &db5)==SQLITE_OK );
  }else{
  assert( sqlite3_open("file:db3.db?blockchain=on&bind=4303&discovery=127.0.0.1:4301,127.0.0.1:4302", &db3)==SQLITE_OK );
  assert( sqlite3_open("file:db4.db?blockchain=on&bind=4304&discovery=127.0.0.1:4301,127.0.0.1:4302", &db4)==SQLITE_OK );
  assert( sqlite3_open("file:db5.db?blockchain=on&bind=4305&discovery=127.0.0.1:4301,127.0.0.1:4302", &db5)==SQLITE_OK );
  }


  /* execute 3 db transactions on one of the databases */

  db_check_int(db1, "PRAGMA last_nonce", 1);

  db_execute(db1, "create table t1 (name)");
  db_execute(db1, "insert into t1 values ('aa1')");
  db_execute(db1, "insert into t1 values ('aa2')");

  db_check_int(db1, "PRAGMA last_nonce", 4);
  db_check_int(db2, "PRAGMA last_nonce", 1);
  db_check_int(db3, "PRAGMA last_nonce", 1);


  /* wait until the transactions are processed in a new block */

  done = 0;
  for(count=0; !done && count<100; count++){
    char *result;
    usleep(wait_time); // 100 ms
    rc = db_query_str(&result, db1, "PRAGMA transaction_status(4)");
    assert(rc==SQLITE_OK);
    done = (strcmp(result,"processed")==0);
    sqlite3_free(result);
    printf("."); fflush(stdout);
  }
  assert(done);

  printf("1"); fflush(stdout);


  /* db2 */

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db2, "select count(*) from sqlite_master where name='t1'");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>0);
  }
  assert(done);

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db2, "select count(*) from t1");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>1);
  }
  assert(done);

  db_check_int(db2, "select count(*) from t1", 2);
  db_check_int(db2, "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db2, "select count(*) from t1 where name='aa2'", 1);

  printf("2"); fflush(stdout);


  /* db3 */

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db3, "select count(*) from sqlite_master where name='t1'");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>0);
  }
  assert(done);

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db3, "select count(*) from t1");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>1);
  }
  assert(done);

  db_check_int(db3, "select count(*) from t1", 2);
  db_check_int(db3, "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db3, "select count(*) from t1 where name='aa2'", 1);

  printf("3"); fflush(stdout);


  /* db4 */

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db4, "select count(*) from sqlite_master where name='t1'");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>0);
  }
  assert(done);

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db4, "select count(*) from t1");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>1);
  }
  assert(done);

  db_check_int(db4, "select count(*) from t1", 2);
  db_check_int(db4, "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db4, "select count(*) from t1 where name='aa2'", 1);

  printf("4"); fflush(stdout);


  /* db5 */

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db5, "select count(*) from sqlite_master where name='t1'");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>0);
  }
  assert(done);

  done = 0;
  for(count=0; !done && count<100; count++){
    int result;
    if( count>0 ) usleep(wait_time);
    rc = db_query_int32(&result, db5, "select count(*) from t1");
    assert(rc==SQLITE_OK);
    printf("."); fflush(stdout);
    done = (result>1);
  }
  assert(done);

  db_check_int(db5, "select count(*) from t1", 2);
  db_check_int(db5, "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db5, "select count(*) from t1 where name='aa2'", 1);

  printf("5"); fflush(stdout);


  /* db1 */

  db_check_int(db1, "select count(*) from t1", 2);
  db_check_int(db1, "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db1, "select count(*) from t1 where name='aa2'", 1);


  sqlite3_close(db1);
  sqlite3_close(db2);
  sqlite3_close(db3);
  sqlite3_close(db4);
  sqlite3_close(db5);

  puts(" done");

}

/****************************************************************************/

void test_n_nodes(int n, bool bind_to_random_ports){
  sqlite3 *db[512];
  char uri[256];
  int rc, i, count, done;

  printf("test_n_nodes(nodes=%d, random_ports=%d)...", n, bind_to_random_ports); fflush(stdout);

  assert(n>2 && n<512);

  /* delete the db files if they exist */

  delete_files(n);

  /* open the connections to the databases */

#if 0
  assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db[1])==SQLITE_OK );
  assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db[2])==SQLITE_OK );
#endif

  sprintf(uri, "file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302&num_nodes=%d", n);
  assert( sqlite3_open(uri, &db[1])==SQLITE_OK );

  sprintf(uri, "file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301&num_nodes=%d", n);
  assert( sqlite3_open(uri, &db[2])==SQLITE_OK );

  for(i=3; i<=n; i++){
    if( bind_to_random_ports ){
      sprintf(uri, "file:db%d.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", i, n);
    }else{
      sprintf(uri, "file:db%d.db?blockchain=on&bind=%d&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", i, 4300 + i, n);
    }
    //puts(uri);
    assert( sqlite3_open(uri, &db[i])==SQLITE_OK );
  }


  /* execute 3 db transactions on one of the databases */

  db_check_int(db[1], "PRAGMA last_nonce", 1);

  db_execute(db[1], "create table t1 (name)");
  db_execute(db[1], "insert into t1 values ('aa1')");
  db_execute(db[1], "insert into t1 values ('aa2')");

  db_check_int(db[1], "PRAGMA last_nonce", 4);
  db_check_int(db[2], "PRAGMA last_nonce", 1);
  db_check_int(db[3], "PRAGMA last_nonce", 1);


  /* wait until the transactions are processed in a new block */

  done = 0;
  for(count=0; !done && count<100; count++){
    char *result;
    usleep(wait_time);
    rc = db_query_str(&result, db[1], "PRAGMA transaction_status(4)");
    assert(rc==SQLITE_OK);
    done = (strcmp(result,"processed")==0);
    sqlite3_free(result);
    printf("."); fflush(stdout);
  }
  assert(done);

  puts("");


  /* check if the data was replicated to the other nodes */

  for(i=2; i<=n; i++){

    printf("checking node %d\n", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from sqlite_master where name='t1'");
      assert(rc==SQLITE_OK);
      done = (result>0);
    }
    assert(done);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result>1);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1", 2);
    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);

  }


  /* db1 */

  db_check_int(db[1], "select count(*) from t1", 2);
  db_check_int(db[1], "select count(*) from t1 where name='aa1'", 1);
  db_check_int(db[1], "select count(*) from t1 where name='aa2'", 1);



  /* execute more transactions on separate databases */

  puts("inserting more data...");

  db_execute(db[4], "insert into t1 values ('aa3')");
  db_execute(db[3], "insert into t1 values ('aa4')");
  db_execute(db[5], "insert into t1 values ('aa5')");
  db_execute(db[2], "create table t2 (name)");

  db_check_int(db[4], "PRAGMA last_nonce", 2);
  db_check_int(db[3], "PRAGMA last_nonce", 2);
  db_check_int(db[5], "PRAGMA last_nonce", 2);
  db_check_int(db[2], "PRAGMA last_nonce", 2);


  /* wait until the transactions are processed in a new block */

  for(i=2; i<=5; i++){

    printf("waiting new block on node %d", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      char *result;
      usleep(wait_time);
      rc = db_query_str(&result, db[i], "PRAGMA transaction_status(2)");
      assert(rc==SQLITE_OK);
      done = (strcmp(result,"processed")==0);
      sqlite3_free(result);
      printf("."); fflush(stdout);
    }
    assert(done);

    puts("");

  }


  /* check if the data was replicated on all the nodes */

  for(i=1; i<=n; i++){

    printf("checking node %d\n", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from sqlite_master where name='t2'");
      assert(rc==SQLITE_OK);
      done = (result>0);
    }
    assert(done);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result>2);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1", 5);
    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa3'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa4'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa5'", 1);

  }


  for(i=1; i<=n; i++){
    sqlite3_close(db[i]);
  }

  puts("done");

}

/****************************************************************************/

void test_reconnection(
  int n, bool bind_to_random_ports,
  /* nodes that should be disconnected */
  int disconnect_nodes[],
  /* transactions executed on disconnected nodes while in split */
  int num_txns_on_offline_nodes,
  int active_offline_nodes[],
  /* transactions executed on connected nodes while in split */
  int num_txns_on_online_nodes,
  int active_online_nodes[],
  /* transactions executed after the nodes were reconnected and in sync */
  int num_txns_on_reconnect,
  int active_nodes_on_reconnect[]
){
  sqlite3 *db[512];
  char uri[256];
  int rc, i, j, count, done;
  int last_nonce[512];

  printf("----------------------------------------------------------\n"
         "test_reconnection(\n"
         "  nodes=%d\n", n);
  printf("  random_ports=%s\n", bind_to_random_ports ? "yes" : "no");
  print_nodes("  disconnect_nodes", disconnect_nodes);
  print_nodes("  active_offline_nodes", active_offline_nodes);
  print_nodes("  active_online_nodes", active_online_nodes);
  print_nodes("  active_nodes_on_reconnect", active_nodes_on_reconnect);
  printf("  num_txns_on_offline_nodes=%d\n", num_txns_on_offline_nodes);
  printf("  num_txns_on_online_nodes=%d\n", num_txns_on_online_nodes);
  printf("  num_txns_on_reconnect=%d\n", num_txns_on_reconnect);
  puts(")");
  fflush(stdout);

  assert(n>=5 && n<512);

  /* delete the db files if they exist */

  delete_files(n);

  /* open the connections to the databases */

#if 0
  assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db[1])==SQLITE_OK );
  assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db[2])==SQLITE_OK );
#endif

  sprintf(uri, "file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302&num_nodes=%d", n);
  assert( sqlite3_open(uri, &db[1])==SQLITE_OK );

  sprintf(uri, "file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301&num_nodes=%d", n);
  assert( sqlite3_open(uri, &db[2])==SQLITE_OK );

  for(i=3; i<=n; i++){
    if( bind_to_random_ports ){
      sprintf(uri, "file:db%d.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", i, n);
    }else{
      sprintf(uri, "file:db%d.db?blockchain=on&bind=%d&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", i, 4300 + i, n);
    }
    //puts(uri);
    assert( sqlite3_open(uri, &db[i])==SQLITE_OK );
  }


  /* set the initial nonce value for each node */

  for(i=1; i<=n; i++){
    last_nonce[i] = 1;
  }

  for(i=1; i<=n; i++){
    db_check_int(db[i], "PRAGMA last_nonce", last_nonce[i]);
  }


  /* execute 3 db transactions on one of the databases */

// (later or in other fn) configurable: if it does this now or after leader election, how many txns, by which node

  db_execute(db[1], "create table t1 (name)");
  db_execute(db[1], "insert into t1 values ('aa1')");
  db_execute(db[1], "insert into t1 values ('aa2')");

  last_nonce[1] = 4;

  db_check_int(db[1], "PRAGMA last_nonce", 4);
  for(i=1; i<=n; i++){
    db_check_int(db[i], "PRAGMA last_nonce", last_nonce[i]);
  }


  /* wait until the transactions are processed in a new block */

  done = 0;
  for(count=0; !done && count<100; count++){
    char *result;
    usleep(wait_time);
    rc = db_query_str(&result, db[1], "PRAGMA transaction_status(4)");
    assert(rc==SQLITE_OK);
    done = (strcmp(result,"processed")==0);
    sqlite3_free(result);
    printf("."); fflush(stdout);
  }
  assert(done);

  puts("");


  /* check if the data was replicated to the other nodes */

  for(i=1; i<=n; i++){

    printf("checking node %d\n", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from sqlite_master where name='t1'");
      assert(rc==SQLITE_OK);
      done = (result>0);
    }
    assert(done);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result>1);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1", 2);
    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);

    char sql[128];
    sprintf(sql, "PRAGMA transaction_status(%d)", last_nonce[i]);
    db_check_str(db[i], sql, "processed");

  }


  /* disconnect some nodes */

  for(i=0; disconnect_nodes[i]; i++){
    int node = disconnect_nodes[i];
    printf("disconnecting node %d\n", node);
    sqlite3_close(db[node]);
    db[node] = 0;
  }


  /* execute transactions on online nodes */

  if( num_txns_on_online_nodes>0 ){
    assert(len_array_list(active_online_nodes)>0);

    puts("executing new transactions on online nodes...");

    for(j=0, i=0; j<num_txns_on_online_nodes; j++, i++){
      int node = active_online_nodes[i];
      if( node==0 ){
        i = 0;
        node = active_online_nodes[i];
      }
      printf("executing on node %d\n", node);

      db_execute(db[node], "insert into t1 values ('online')");

      last_nonce[node]++;
      db_check_int(db[node], "PRAGMA last_nonce", last_nonce[node]);
    }

    /* wait until the transactions are processed in a new block */

    printf("waiting for new block"); fflush(stdout);

    for(i=0; active_online_nodes[i]; i++){
      int node = active_online_nodes[i];

      done = 0;
      for(count=0; !done && count<200; count++){
        char *result, sql[128];
        if( count>0 ) usleep(150000);
        sprintf(sql, "PRAGMA transaction_status(%d)", last_nonce[node]);
        rc = db_query_str(&result, db[node], sql);
        assert(rc==SQLITE_OK);
        done = (strcmp(result,"processed")==0);
        sqlite3_free(result);
        printf("."); fflush(stdout);
      }
      assert(done);

    }

    puts("");

    /* check if the data was replicated to the other nodes */

    for(i=1; i<=n; i++){

      if( in_array_list(i,disconnect_nodes) ) continue;

      printf("checking node %d\n", i); fflush(stdout);

      done = 0;
      for(count=0; !done && count<100; count++){
        int result;
        if( count>0 ) usleep(wait_time);
        rc = db_query_int32(&result, db[i], "select count(*) from sqlite_master where name='t1'");
        assert(rc==SQLITE_OK);
        done = (result>0);
      }
      assert(done);

      done = 0;
      for(count=0; !done && count<100; count++){
        int result;
        if( count>0 ) usleep(wait_time);
        rc = db_query_int32(&result, db[i], "select count(*) from t1");
        assert(rc==SQLITE_OK);
        done = (result>2);
      }
      assert(done);

      db_check_int(db[i], "select count(*) from t1", 2 + num_txns_on_online_nodes);
      db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='online'", num_txns_on_online_nodes);

    }

  }


  /* execute transactions on offline nodes */

  if( num_txns_on_offline_nodes>0 ){
    assert(len_array_list(active_offline_nodes)>0);

    /* reopen the nodes in off-line mode */

    for(i=0; active_offline_nodes[i]; i++){
      int node = active_offline_nodes[i];
      printf("reopening node %d in offline mode\n", node);
      sprintf(uri, "file:db%d.db?blockchain=on&num_nodes=%d", node, n);
      assert( sqlite3_open(uri, &db[node])==SQLITE_OK );
    }

    puts("executing new transactions on offline nodes...");

    for(j=0, i=0; j<num_txns_on_offline_nodes; j++, i++){
      int node = active_offline_nodes[i];
      if( node==0 ){
        i = 0;
        node = active_offline_nodes[i];
      }
      printf("executing on node %d\n", node);

      db_execute(db[node], "insert into t1 values ('offline')");

      last_nonce[node]++;
      db_check_int(db[node], "PRAGMA last_nonce", last_nonce[node]);

      //db_check_int(db[node], "select count(*) from t1", 2 + num_txns_on_offline_nodes);
      //db_check_int(db[node], "select count(*) from t1 where name='aa1'", 1);
      //db_check_int(db[node], "select count(*) from t1 where name='aa2'", 1);
      //db_check_int(db[node], "select count(*) from t1 where name='offline'", num_txns_on_offline_nodes);
    }

    /* close the off-line nodes */

    for(i=0; active_offline_nodes[i]; i++){
      int node = active_offline_nodes[i];
      sqlite3_close(db[node]);
      db[node] = 0;
    }

  }


  /* reconnect the nodes */

  for(i=0; disconnect_nodes[i]; i++){
    int node = disconnect_nodes[i];
    printf("reconnecting node %d\n", node);
    if( node==1 ){
      //assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db[1])==SQLITE_OK );
      sprintf(uri, "file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302&num_nodes=%d", n);
      assert( sqlite3_open(uri, &db[1])==SQLITE_OK );
    }else if( node==2 ){
      //assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db[2])==SQLITE_OK );
      sprintf(uri, "file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301&num_nodes=%d", n);
      assert( sqlite3_open(uri, &db[2])==SQLITE_OK );
    }else{
      if( bind_to_random_ports ){
        sprintf(uri, "file:db%d.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", node, n);
      }else{
        sprintf(uri, "file:db%d.db?blockchain=on&bind=%d&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", node, 4300 + node, n);
      }
      assert( sqlite3_open(uri, &db[node])==SQLITE_OK );
    }
  }


  /* check if they are up-to-date */

  for(i=0; disconnect_nodes[i]; i++){
    int node = disconnect_nodes[i];

    printf("checking node %d\n", node); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      char *result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_str(&result, db[node], "pragma protocol_status");
      assert(rc==SQLITE_OK);
      done = strstr(result,"\"is_leader\": true")>0 || strstr(result,"\"leader\": null")==0;
      sqlite3_free(result);
    }
    assert(done);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[node], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result >= 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes);
    }
    assert(done);

    db_check_int(db[node], "select count(*) from t1", 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes);
    db_check_int(db[node], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[node], "select count(*) from t1 where name='aa2'", 1);
    db_check_int(db[node], "select count(*) from t1 where name='offline'", num_txns_on_offline_nodes);
    db_check_int(db[node], "select count(*) from t1 where name='online'", num_txns_on_online_nodes);

  }


  /* check if the data was replicated to the other nodes */

  for(i=1; i<=n; i++){

    if( in_array_list(i,disconnect_nodes) ) continue;

    printf("checking node %d\n", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result >= 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1", 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes);
    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='offline'", num_txns_on_offline_nodes);
    db_check_int(db[i], "select count(*) from t1 where name='online'", num_txns_on_online_nodes);

  }


  /* execute new transactions after reconnection */

  if( num_txns_on_reconnect>0 ){
    assert(len_array_list(active_nodes_on_reconnect)>0);

    puts("executing new transactions after reconnection...");

    for(j=0, i=0; j<num_txns_on_reconnect; j++, i++){
      int node = active_nodes_on_reconnect[i];
      if( node==0 ){
        i = 0;
        node = active_nodes_on_reconnect[i];
      }
      printf("executing on node %d\n", node);

      db_execute(db[node], "insert into t1 values ('reconnect')");

      last_nonce[node]++;
      db_check_int(db[node], "PRAGMA last_nonce", last_nonce[node]);
    }

    /* wait until the transactions are processed in a new block */

    printf("waiting for new block"); fflush(stdout);

    for(i=0; active_nodes_on_reconnect[i]; i++){
      int node = active_nodes_on_reconnect[i];

      done = 0;
      for(count=0; !done && count<200; count++){
        char *result, sql[128];
        if( count>0 ) usleep(150000);
        sprintf(sql, "PRAGMA transaction_status(%d)", last_nonce[node]);
        rc = db_query_str(&result, db[node], sql);
        assert(rc==SQLITE_OK);
        done = (strcmp(result,"processed")==0);
        sqlite3_free(result);
        printf("."); fflush(stdout);
      }
      assert(done);

    }

    puts("");

    /* check if the data was replicated to the other nodes */

    for(i=1; i<=n; i++){

      printf("checking node %d\n", i); fflush(stdout);

      done = 0;
      for(count=0; !done && count<100; count++){
        int result;
        if( count>0 ) usleep(wait_time);
        rc = db_query_int32(&result, db[i], "select count(*) from t1");
        assert(rc==SQLITE_OK);
        done = (result >= 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes + num_txns_on_reconnect);
      }
      assert(done);

      db_check_int(db[i], "select count(*) from t1", 2 + num_txns_on_online_nodes + num_txns_on_offline_nodes + num_txns_on_reconnect);
      db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='offline'", num_txns_on_offline_nodes);
      db_check_int(db[i], "select count(*) from t1 where name='online'", num_txns_on_online_nodes);
      db_check_int(db[i], "select count(*) from t1 where name='reconnect'", num_txns_on_reconnect);

    }

  }

  /* close the db connections */

  for(i=1; i<=n; i++){
    sqlite3_close(db[i]);
  }

  puts("done");

}

/****************************************************************************/

void test_new_nodes(
  int n_before, bool bind_to_random_ports,
  /* which node will make the first transactions */
  int starting_node,
  /* new nodes added to the network after it is running */
  int n_new_no_content,
  int n_new_with_content,
  /* nodes that are disconnected and reconnected */
  int n_old_no_content,
  int n_old_with_content,
  /* blocks generated while some nodes were offline */
  int new_blocks_on_net,
  int num_offline_txns,

  /* number of nodes remaining online later, less than the majority of nodes */
  int n_remaining_online,
  /* new nodes added while the network has less than half nodes */
  int n_new_no_content2,
  int n_new_with_content2
){
  sqlite3 *db[512] = {0};
  char uri[256];
  int last_nonce[512];
  int disconnect_nodes[512] = {0};
  int active_online_nodes[512] = {0};
  int connecting_nodes[512] = {0};
  int rc, i, j, n, count, done;

  printf("----------------------------------------------------------\n"
         "test_new_nodes(\n"
         "  initial_nodes=%d\n", n_before);
  printf("  random_ports=%s\n", bind_to_random_ports ? "yes" : "no");
  printf("  n_new_no_content=%d\n", n_new_no_content);
  printf("  n_new_with_content=%d\n", n_new_with_content);
  printf("  n_old_no_content=%d\n", n_old_no_content);
  printf("  n_old_with_content=%d\n", n_old_with_content);
  printf("  num_offline_txns=%d\n", num_offline_txns);
  printf("  new_blocks_on_net=%d\n", new_blocks_on_net);

  printf("  n_remaining_online=%d\n", n_remaining_online);
  printf("  n_new_no_content2=%d\n", n_new_no_content2);
  printf("  n_new_with_content2=%d\n", n_new_with_content2);
  puts(")");
  fflush(stdout);

  n = n_before + n_new_no_content + n_new_with_content + n_new_no_content2 + n_new_with_content2;

  assert(n>=5 && n<512);

  /* delete the db files if they exist */

  delete_files(n);

  /* open the connections to the databases */

#if 0
  assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db[1])==SQLITE_OK );
  assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db[2])==SQLITE_OK );
#endif

  sprintf(uri, "file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302&num_nodes=%d", n);
  assert( sqlite3_open(uri, &db[1])==SQLITE_OK );

  sprintf(uri, "file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301&num_nodes=%d", n);
  assert( sqlite3_open(uri, &db[2])==SQLITE_OK );

  for(i=3; i<=n_before; i++){
    if( bind_to_random_ports ){
      sprintf(uri, "file:db%d.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", i, n);
    }else{
      sprintf(uri, "file:db%d.db?blockchain=on&bind=%d&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", i, 4300 + i, n);
    }
    //puts(uri);
    assert( sqlite3_open(uri, &db[i])==SQLITE_OK );
  }


  /* set the initial nonce value for each node */

  for(i=1; i<=n; i++){
    last_nonce[i] = 1;
  }

  for(i=1; i<=n_before; i++){
    db_check_int(db[i], "PRAGMA last_nonce", last_nonce[i]);
  }


  /* execute 3 db transactions on one of the databases */

  db_execute(db[starting_node], "create table t1 (name)");
  db_execute(db[starting_node], "insert into t1 values ('aa1')");
  db_execute(db[starting_node], "insert into t1 values ('aa2')");

  last_nonce[starting_node] = 4;

  db_check_int(db[starting_node], "PRAGMA last_nonce", 4);
  for(i=1; i<=n_before; i++){
    db_check_int(db[i], "PRAGMA last_nonce", last_nonce[i]);
  }


  /* wait until the transactions are processed in a new block */

  done = 0;
  for(count=0; !done && count<100; count++){
    char *result;
    usleep(wait_time);
    rc = db_query_str(&result, db[starting_node], "PRAGMA transaction_status(4)");
    assert(rc==SQLITE_OK);
    done = (strcmp(result,"processed")==0);
    sqlite3_free(result);
    printf("."); fflush(stdout);
  }
  assert(done);

  puts("");


  /* check if the data was replicated to the other nodes */

  for(i=1; i<=n_before; i++){

    printf("checking node %d\n", i); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from sqlite_master where name='t1'");
      assert(rc==SQLITE_OK);
      done = (result>0);
    }
    assert(done);

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result>1);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1", 2);
    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);

    char sql[128];
    sprintf(sql, "PRAGMA transaction_status(%d)", last_nonce[i]);
    db_check_str(db[i], sql, "processed");

  }


  /* disconnect some nodes */

  int n_first_disconnect = n_old_no_content + n_old_with_content;

  for(i=0; i<n_first_disconnect; i++){
    int node;
    do{
      node = random_number(1, n_before);
    }while( in_array_list(node,disconnect_nodes) || node==2 ); /* does not disconnect node 2 */
    add_to_array_list(disconnect_nodes, node);
    printf("disconnecting node %d\n", node);
    sqlite3_close(db[node]);
    db[node] = 0;
  }

  for(i=1; i<=n_before; i++){
    if( !in_array_list(i,disconnect_nodes) ){
      add_to_array_list(active_online_nodes, i);
    }
  }


  /* execute transactions on online nodes */

  int num_txns_per_block = num_offline_txns;

  for(int num_blocks=1; num_blocks<=new_blocks_on_net; num_blocks++){
    assert(len_array_list(active_online_nodes)>0);

    puts("executing new transactions on online nodes...");

    for(j=0, i=0; j<num_txns_per_block; j++, i++){
      int node = active_online_nodes[i];
      if( node==0 ){
        i = 0;
        node = active_online_nodes[i];
      }
      printf("executing on node %d\n", node);

      db_execute(db[node], "insert into t1 values ('online')");

      last_nonce[node]++;
      db_check_int(db[node], "PRAGMA last_nonce", last_nonce[node]);
    }

    /* wait until the transactions are processed in a new block */

    printf("waiting for new block"); fflush(stdout);

    for(i=0; active_online_nodes[i]; i++){
      int node = active_online_nodes[i];

      done = 0;
      for(count=0; !done && count<200; count++){
        char *result, sql[128];
        if( count>0 ) usleep(150000);
        sprintf(sql, "PRAGMA transaction_status(%d)", last_nonce[node]);
        rc = db_query_str(&result, db[node], sql);
        assert(rc==SQLITE_OK);
        done = (strcmp(result,"processed")==0);
        sqlite3_free(result);
        printf("."); fflush(stdout);
      }
      assert(done);

    }

    puts("");

    /* check if the data was replicated to the other nodes */

    for(i=1; i<=n_before; i++){

      if( in_array_list(i,disconnect_nodes) ) continue;

      printf("checking node %d\n", i); fflush(stdout);

      done = 0;
      for(count=0; !done && count<100; count++){
        int result;
        if( count>0 ) usleep(wait_time);
        rc = db_query_int32(&result, db[i], "select count(*) from t1");
        assert(rc==SQLITE_OK);
        done = (result >= 2 + num_blocks * num_txns_per_block);
      }
      assert(done);

      db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='online'", num_blocks * num_txns_per_block);
      db_check_int(db[i], "select count(*) from t1", 2 + num_blocks * num_txns_per_block);

    }

  }


  /* execute transactions on disconnected nodes */

  if( n_old_with_content>0 ){

    /* reopen the nodes in off-line mode */

    for(i=0; i<n_old_with_content; i++){
      int node = disconnect_nodes[i];
      printf("reopening node %d in offline mode\n", node);
      sprintf(uri, "file:db%d.db?blockchain=on&num_nodes=%d", node, n);
      assert( sqlite3_open(uri, &db[node])==SQLITE_OK );
    }

    puts("executing new transactions on offline nodes...");

    for(j=0, i=0; j<n_old_with_content*num_offline_txns; j++, i++){
      int node = disconnect_nodes[i];
      if( node==0 || db[node]==0 ){
        i = 0;
        node = disconnect_nodes[i];
      }
      printf("executing on node %d\n", node);

      db_execute(db[node], "insert into t1 values ('disconnected')");

      last_nonce[node]++;
      db_check_int(db[node], "PRAGMA last_nonce", last_nonce[node]);
    }

    /* close the off-line nodes */

    for(i=0; i<n_old_with_content; i++){
      int node = disconnect_nodes[i];
      sqlite3_close(db[node]);
      db[node] = 0;
    }

  }


  /* execute transactions on new, not yet connected nodes */

  if( n_new_with_content>0 ){

    /* reopen the nodes in off-line mode */

    for(i=n_before+1; i<=n_before+n_new_with_content; i++){
      int node = i;
      printf("opening new node %d in offline mode\n", node);
      sprintf(uri, "file:db%d.db?blockchain=on&num_nodes=%d", node, n);
      assert( sqlite3_open(uri, &db[node])==SQLITE_OK );
    }

    puts("executing offline transactions on new nodes...");

    for(i=n_before+1; i<=n_before+n_new_with_content; i++){
      int node = i;
      printf("executing on node %d\n", node);
      for(j=0; j<num_offline_txns; j++){
        char sql[256];
        sprintf(sql, "create table t%d%d (name)", i, j);
        db_execute(db[node], sql);
        //db_execute(db[node], "insert into t1 values ('new')");
        last_nonce[node]++;
        db_check_int(db[node], "PRAGMA last_nonce", last_nonce[node]);
      }
    }

    /* close the new nodes */

    for(i=n_before+1; i<=n_before+n_new_with_content; i++){
      int node = i;
      sqlite3_close(db[node]);
      db[node] = 0;
    }

  }


  /* get the list of connecting nodes */

  for(i=0; disconnect_nodes[i]; i++){
    int node = disconnect_nodes[i];
    add_to_array_list(connecting_nodes, node);
  }

  for(i=n_before+1; i<=n_before+n_new_with_content+n_new_no_content; i++){
    add_to_array_list(connecting_nodes, i);
  }


  /* reconnect the nodes */

  for(i=0; connecting_nodes[i]; i++){
    int node = connecting_nodes[i];
    if( node>n_before ){
      printf("connecting new node %d\n", node);
    }else{
      printf("reconnecting node %d\n", node);
    }
    if( node==1 ){
      //assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db[1])==SQLITE_OK );
      sprintf(uri, "file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302&num_nodes=%d", n);
      assert( sqlite3_open(uri, &db[1])==SQLITE_OK );
    }else if( node==2 ){
      //assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db[2])==SQLITE_OK );
      sprintf(uri, "file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301&num_nodes=%d", n);
      assert( sqlite3_open(uri, &db[2])==SQLITE_OK );
    }else{
      if( bind_to_random_ports ){
        sprintf(uri, "file:db%d.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", node, n);
      }else{
        sprintf(uri, "file:db%d.db?blockchain=on&bind=%d&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", node, 4300 + node, n);
      }
      assert( sqlite3_open(uri, &db[node])==SQLITE_OK );
    }
  }


  /* check if they are up-to-date */

  for(i=0; connecting_nodes[i]; i++){
    int node = connecting_nodes[i];

    printf("checking node %d\n", node); fflush(stdout);

    done = 0;
    for(count=0; !done && count<100; count++){
      char *result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_str(&result, db[node], "pragma protocol_status");
      assert(rc==SQLITE_OK);
      done = strstr(result,"\"is_leader\": true")>0 || strstr(result,"\"leader\": null")==0;
      sqlite3_free(result);
    }
    assert(done);

    int total_rows = 2 + new_blocks_on_net * num_txns_per_block + n_old_with_content * num_offline_txns;

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[node], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result >= total_rows);
    }
    assert(done);

    db_check_int(db[node], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[node], "select count(*) from t1 where name='aa2'", 1);
    db_check_int(db[node], "select count(*) from t1 where name='online'", new_blocks_on_net * num_txns_per_block);
    db_check_int(db[node], "select count(*) from t1 where name='disconnected'", n_old_with_content * num_offline_txns);
    db_check_int(db[node], "select count(*) from t1", total_rows);
    db_check_int(db[node], "select count(*) from sqlite_master where type='table'", 1 + 1 + n_new_with_content * num_offline_txns);

  }


  /* check if the data was replicated to the other nodes */

  for(i=1; i<=n_before; i++){

    if( in_array_list(i,connecting_nodes) ) continue;

    printf("checking node %d\n", i); fflush(stdout);

    int total_rows = 2 + new_blocks_on_net * num_txns_per_block + n_old_with_content * num_offline_txns;

    done = 0;
    for(count=0; !done && count<100; count++){
      int result;
      if( count>0 ) usleep(wait_time);
      rc = db_query_int32(&result, db[i], "select count(*) from t1");
      assert(rc==SQLITE_OK);
      done = (result >= total_rows);
    }
    assert(done);

    db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
    db_check_int(db[i], "select count(*) from t1 where name='online'", new_blocks_on_net * num_txns_per_block);
    db_check_int(db[i], "select count(*) from t1 where name='disconnected'", n_old_with_content * num_offline_txns);
    db_check_int(db[i], "select count(*) from t1", total_rows);
    db_check_int(db[i], "select count(*) from sqlite_master where type='table'", 1 + 1 + n_new_with_content * num_offline_txns);

  }




  // join while the network was having less than majority
  // join while the network has less than majority, and keep having no majority



  int n_after = n_before + n_new_with_content + n_new_no_content;

  int n_disconnect = n_after - n_remaining_online;

  if( n_disconnect==0 ) goto loc_exit;


  /* disconnect many nodes until the network has no majority online */

  clear_array_list(disconnect_nodes);

  for(i=0; i<n_disconnect; i++){
    int node;
    do{
      node = random_number(1, n_after);
    }while( in_array_list(node,disconnect_nodes) || node==2 ); /* does not disconnect node 2 */
    add_to_array_list(disconnect_nodes, node);
    printf("disconnecting node %d\n", node);
    sqlite3_close(db[node]);
    db[node] = 0;
  }

  clear_array_list(active_online_nodes);

  for(i=1; i<=n_after; i++){
    if( !in_array_list(i,disconnect_nodes) ){
      add_to_array_list(active_online_nodes, i);
    }
  }



  /* execute transactions on the remaining online nodes */

  puts("executing transactions on remaining online nodes...");

  for(j=0, i=0; j<num_offline_txns; j++, i++){
    int node = active_online_nodes[i];
    if( node==0 ){
      i = 0;
      node = active_online_nodes[i];
    }
    printf("executing on node %d\n", node);

    db_execute(db[node], "insert into t1 values ('no_quorum')");

    last_nonce[node]++;
    db_check_int(db[node], "PRAGMA last_nonce", last_nonce[node]);
  }

  /* wait until the transactions are processed in a new block */

  printf("checking that no new block is generated"); fflush(stdout);

  done = 0;
  for(count=0, i=0; !done && count<40; count++){
    int node = active_online_nodes[i];
    if( node==0 ){
      i = 0;
      node = active_online_nodes[i];
    }
    char *result, sql[128];
    usleep(wait_time);
    sprintf(sql, "PRAGMA transaction_status(%d)", last_nonce[node]);
    rc = db_query_str(&result, db[node], sql);
    assert(rc==SQLITE_OK);
    done = (strcmp(result,"processed")==0);
    sqlite3_free(result);
    printf("."); fflush(stdout);
  }
  assert(done==0);

  puts("");


  /* execute transactions on new, not yet connected nodes */

  if( n_new_with_content2>0 ){

    /* reopen the nodes in off-line mode */

    for(i=n_after+1; i<=n_after+n_new_with_content2; i++){
      int node = i;
      printf("opening new node %d in offline mode\n", node);
      sprintf(uri, "file:db%d.db?blockchain=on&num_nodes=%d", node, n);
      assert( sqlite3_open(uri, &db[node])==SQLITE_OK );
    }

    puts("executing offline transactions on new nodes...");

    for(i=n_after+1; i<=n_after+n_new_with_content2; i++){
      int node = i;
      printf("executing on node %d\n", node);
      for(j=0; j<num_offline_txns; j++){
        char sql[256];
        sprintf(sql, "create table t%d%d (name)", i, j);
        db_execute(db[node], sql);
        //db_execute(db[node], "insert into t1 values ('new')");
        last_nonce[node]++;
        db_check_int(db[node], "PRAGMA last_nonce", last_nonce[node]);
      }
    }

    /* close the new nodes */

    for(i=n_after+1; i<=n_after+n_new_with_content2; i++){
      int node = i;
      sqlite3_close(db[node]);
      db[node] = 0;
    }

  }



  /* get the list of connecting nodes */

  clear_array_list(connecting_nodes);

  //for(i=0; disconnect_nodes[i]; i++){
  //  int node = disconnect_nodes[i];
  //  add_to_array_list(connecting_nodes, node);
  //}

  int n_after2 = n_after + n_new_with_content2 + n_new_no_content2;

  for(i=n_after+1; i<=n_after2; i++){
    add_to_array_list(connecting_nodes, i);
    add_to_array_list(active_online_nodes, i);
  }


  /* reconnect the nodes */

  for(i=0; connecting_nodes[i]; i++){
    int node = connecting_nodes[i];
    if( node>n_before ){
      printf("connecting new node %d\n", node);
    }else{
      printf("reconnecting node %d\n", node);
    }
    if( node==1 ){
      //assert( sqlite3_open("file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302", &db[1])==SQLITE_OK );
      sprintf(uri, "file:db1.db?blockchain=on&bind=4301&discovery=127.0.0.1:4302&num_nodes=%d", n);
      assert( sqlite3_open(uri, &db[1])==SQLITE_OK );
    }else if( node==2 ){
      //assert( sqlite3_open("file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301", &db[2])==SQLITE_OK );
      sprintf(uri, "file:db2.db?blockchain=on&bind=4302&discovery=127.0.0.1:4301&num_nodes=%d", n);
      assert( sqlite3_open(uri, &db[2])==SQLITE_OK );
    }else{
      if( bind_to_random_ports ){
        sprintf(uri, "file:db%d.db?blockchain=on&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", node, n);
      }else{
        sprintf(uri, "file:db%d.db?blockchain=on&bind=%d&discovery=127.0.0.1:4301,127.0.0.1:4302&num_nodes=%d", node, 4300 + node, n);
      }
      assert( sqlite3_open(uri, &db[node])==SQLITE_OK );
    }
  }



  /* check if the transactions are processed in a new block */

  if( n_after2 >= majority(n) ){

    printf("waiting for new block"); fflush(stdout);

    for(i=0; active_online_nodes[i]; i++){
      int node = active_online_nodes[i];
      done = 0;
      for(count=0; !done && count<200; count++){
        char *result, sql[128];
        if( count>0 ) usleep(wait_time);
        sprintf(sql, "PRAGMA transaction_status(%d)", last_nonce[node]);
        rc = db_query_str(&result, db[node], sql);
        assert(rc==SQLITE_OK);
        done = (strcmp(result,"processed")==0);
        sqlite3_free(result);
        printf("."); fflush(stdout);
      }
      assert(done);
    }
    puts("");

    /* check if the data was replicated to the other nodes */

    int num_records = 2 + new_blocks_on_net * num_txns_per_block
                        + n_old_with_content * num_offline_txns
                        + num_offline_txns;

    int num_tables = 2 + n_new_with_content * num_offline_txns
                       + n_new_with_content2 * num_offline_txns;

    for(i=1; i<=n_after2; i++){

      if( in_array_list(i,disconnect_nodes) ) continue;

      printf("checking node %d\n", i); fflush(stdout);

      done = 0;
      for(count=0; !done && count<100; count++){
        int result;
        if( count>0 ) usleep(wait_time);
        rc = db_query_int32(&result, db[i], "select count(*) from t1");
        assert(rc==SQLITE_OK);
        done = (result >= num_records);
      }
      assert(done);

      db_check_int(db[i], "select count(*) from t1 where name='aa1'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='aa2'", 1);
      db_check_int(db[i], "select count(*) from t1 where name='online'", new_blocks_on_net * num_txns_per_block);
      db_check_int(db[i], "select count(*) from t1 where name='disconnected'", n_old_with_content * num_offline_txns);
      db_check_int(db[i], "select count(*) from t1 where name='no_quorum'", num_offline_txns);
      db_check_int(db[i], "select count(*) from sqlite_master where type='table'", num_tables);
      db_check_int(db[i], "select count(*) from t1", num_records);
    }

  }else{  /* no majority of nodes on the network */

    printf("checking that no new block is generated"); fflush(stdout);

    done = 0;
    for(count=0, i=0; !done && count<100; count++){
      int node = active_online_nodes[i];
      if( node==0 ){
        i = 0;
        node = active_online_nodes[i];
      }
      char *result, sql[128];
      usleep(wait_time);
      sprintf(sql, "PRAGMA transaction_status(%d)", last_nonce[node]);
      rc = db_query_str(&result, db[node], sql);
      assert(rc==SQLITE_OK);
      done = (strcmp(result,"processed")==0);
      sqlite3_free(result);
      printf("."); fflush(stdout);
    }
    puts("");
    assert(done==0);

  }


loc_exit:

  /* close the db connections */

  for(i=1; i<=n; i++){
    sqlite3_close(db[i]);
  }

  puts("done");

}

/****************************************************************************/
/****************************************************************************/

int main(){

//  test_5_nodes(0);
//  test_5_nodes(1);

//  test_n_nodes(10, true);
//  test_n_nodes(25, false);
//  test_n_nodes(50, true);
//  test_n_nodes(100, true);

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 0,
    /* active_offline_nodes[],     */ (int[]){0},
    /* num_txns_on_online_nodes,   */ 0,
    /* active_online_nodes[],      */ (int[]){0},
    /* num_txns_on_reconnect,      */ 0,
    /* active_nodes_on_reconnect[] */ (int[]){0}
  );

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 0,
    /* active_offline_nodes[],     */ (int[]){0},
    /* num_txns_on_online_nodes,   */ 0,
    /* active_online_nodes[],      */ (int[]){0},
    /* num_txns_on_reconnect,      */ 3,
    /* active_nodes_on_reconnect[] */ (int[]){3,6,9,0}
  );

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 0,
    /* active_offline_nodes[],     */ (int[]){0},
    /* num_txns_on_online_nodes,   */ 3,
    /* active_online_nodes[],      */ (int[]){3,8,0},
    /* num_txns_on_reconnect,      */ 0,
    /* active_nodes_on_reconnect[] */ (int[]){0}
  );

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 0,
    /* active_offline_nodes[],     */ (int[]){0},
    /* num_txns_on_online_nodes,   */ 3,
    /* active_online_nodes[],      */ (int[]){3,8,0},
    /* num_txns_on_reconnect,      */ 5,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,0}
  );

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 3,
    /* active_offline_nodes[],     */ (int[]){4,10,0},
    /* num_txns_on_online_nodes,   */ 0,
    /* active_online_nodes[],      */ (int[]){0},
    /* num_txns_on_reconnect,      */ 0,
    /* active_nodes_on_reconnect[] */ (int[]){0}
  );

  test_reconnection(10, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,0},
    /* num_txns_on_offline_nodes,  */ 3,
    /* active_offline_nodes[],     */ (int[]){4,10,0},
    /* num_txns_on_online_nodes,   */ 3,
    /* active_online_nodes[],      */ (int[]){3,8,0},
    /* num_txns_on_reconnect,      */ 5,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,0}
  );

  test_reconnection(25, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,15,20,23,0},
    /* num_txns_on_offline_nodes,  */ 6,
    /* active_offline_nodes[],     */ (int[]){2,7,15,23,0},
    /* num_txns_on_online_nodes,   */ 6,
    /* active_online_nodes[],      */ (int[]){3,8,11,17,0},
    /* num_txns_on_reconnect,      */ 9,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,20,21,0}
  );

  test_reconnection(50, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,15,20,23,33,37,38,44,49,0},
    /* num_txns_on_offline_nodes,  */ 9,
    /* active_offline_nodes[],     */ (int[]){4,10,20,33,38,49,0},
    /* num_txns_on_online_nodes,   */ 9,
    /* active_online_nodes[],      */ (int[]){3,8,11,25,35,45,0},
    /* num_txns_on_reconnect,      */ 12,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,20,25,44,45,0}
  );

  test_reconnection(100, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,15,20,23,33,37,38,44,49,55,66,77,88,95,0},
    /* num_txns_on_offline_nodes,  */ 9,
    /* active_offline_nodes[],     */ (int[]){4,10,20,33,38,49,0},
    /* num_txns_on_online_nodes,   */ 9,
    /* active_online_nodes[],      */ (int[]){3,8,11,25,35,45,0},
    /* num_txns_on_reconnect,      */ 12,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,20,25,44,45,0}
  );

#if 0
  test_reconnection(150, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,15,20,23,33,37,38,44,49,55,66,77,88,95,0},
    /* num_txns_on_offline_nodes,  */ 9,
    /* active_offline_nodes[],     */ (int[]){4,10,20,33,38,49,0},
    /* num_txns_on_online_nodes,   */ 9,
    /* active_online_nodes[],      */ (int[]){3,8,11,25,35,45,0},
    /* num_txns_on_reconnect,      */ 12,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,20,25,44,45,0}
  );

  test_reconnection(200, false,
    /* disconnect_nodes[]          */ (int[]){2,4,7,10,15,20,23,33,37,38,44,49,55,66,77,88,95,0},
    /* num_txns_on_offline_nodes,  */ 9,
    /* active_offline_nodes[],     */ (int[]){4,10,20,33,38,49,0},
    /* num_txns_on_online_nodes,   */ 9,
    /* active_online_nodes[],      */ (int[]){3,8,11,25,35,45,0},
    /* num_txns_on_reconnect,      */ 12,
    /* active_nodes_on_reconnect[] */ (int[]){2,3,6,7,20,25,44,45,0}
  );
#endif


//  test_new_nodes  -- some nodes join only after the majority already 'worked' for a while
    // join while other nodes are joining:
      // ones without content, others with offline content but first-time connection,
      // and others that were already part of the network but are now offline (without and with new offline content)
      // in 2 cases: the net has new blocks, the net does not have new block
    // join while the network was having less than majority
    // join while the network has less than majority, and keep having no majority

#if 0
  test_new_nodes(
    /* n_before,              */ 10,
    /* bind_to_random_ports,  */ true,
    /* starting_node,         */ 2,
    /* n_new_no_content,      */ 1,
    /* n_new_with_content,    */ 1,
    /* n_old_no_content,      */ 1,
    /* n_old_with_content,    */ 2,
    /* new_blocks_on_net,     */ 1,
    /* num_offline_txns,      */ 3,

    /* n_remaining_online,    */ 12,
    /* n_new_no_content2,     */ 0,
    /* n_new_with_content2    */ 0
  );
#endif

  test_new_nodes( /* total_nodes=14 */
    /* n_before,              */ 10,
    /* bind_to_random_ports,  */ true,
    /* starting_node,         */ 2,
    /* n_new_no_content,      */ 1,
    /* n_new_with_content,    */ 1,
    /* n_old_no_content,      */ 1,
    /* n_old_with_content,    */ 1,
    /* new_blocks_on_net,     */ 1,
    /* num_offline_txns,      */ 3,

    /* n_remaining_online,    */ 7,
    /* n_new_no_content2,     */ 1,
    /* n_new_with_content2    */ 1
  );

  test_new_nodes( /* total_nodes=14 */
    /* n_before,              */ 10,
    /* bind_to_random_ports,  */ true,
    /* starting_node,         */ 2,
    /* n_new_no_content,      */ 1,
    /* n_new_with_content,    */ 1,
    /* n_old_no_content,      */ 1,
    /* n_old_with_content,    */ 1,
    /* new_blocks_on_net,     */ 1,
    /* num_offline_txns,      */ 3,

    /* n_remaining_online,    */ 7,
    /* n_new_no_content2,     */ 2,
    /* n_new_with_content2    */ 0
  );


  /* delete the test db files on success */
  delete_files(200);

  puts("OK. All tests pass!"); return 0;
}
