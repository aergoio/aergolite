
/****************************************************************************/

SQLITE_PRIVATE int random_number(int lower, int upper){
  unsigned int n;
  sqlite3_randomness(sizeof(n), &n);
  return (n % (upper - lower + 1)) + lower;
}

/****************************************************************************/

/*
** Calculate the wait time interval (to generate a block) from the
** pseudorandom output + total authorized nodes + configured block interval.
**
** The total_authorized_nodes and block_interval may not be the same on
** all the nodes. Some nodes may not had received new node authorizations.
**
** To avoid manipulation on the time interval, the total_authorized_nodes
** number does not affect the order of nodes. It only changes the time
** interval between them.
**
** The nodes do not need to use the same value for total_authorized_nodes
** when selecting the winner block. Each node can use its own value and it
** will give the same order.
*/
SQLITE_PRIVATE unsigned int calculate_wait_interval(
  unsigned char *vrf_output, int total_authorized_nodes, int block_interval
){
  unsigned int lower, upper, n, random_block_interval;
  double val;

  SYNCTRACE("calculate_wait_interval total_authorized_nodes=%d block_interval=%d\n",
            total_authorized_nodes, block_interval);

  assert(total_authorized_nodes > 0);
  assert(block_interval > 0);

  /* calculate the lower and upper bounds */
  lower = block_interval / 2;
  upper = block_interval / 2 * total_authorized_nodes;

  /* calculate the wait interval */
  n = *(int*) &vrf_output[10];
  val = ((double)n) / 0xFFFFFFFF;
  random_block_interval = val * (upper - lower) + lower;

  assert( random_block_interval>=lower && random_block_interval<=upper );

  return random_block_interval;
}

/****************************************************************************/

/*
** Calculate this node's wait time interval to generate the block at the
** given height.
**
** random_no, proof = vrf_generate(plugin, seed);
** interval = calc_wait_interval(random_no, total_authorized_nodes, block_interval);
*/
SQLITE_PRIVATE unsigned int calculate_node_wait_interval(plugin *plugin, int64 block_height){
  unsigned char proof[81];
  unsigned char output[32];
  char msg[64];
  size_t msglen;
  unsigned int interval;

  SYNCTRACE("calculate_node_wait_interval block_height=%" INT64_FORMAT "\n", block_height);

  /* generate the seed for this block height */
  sprintf(msg, "block%" INT64_FORMAT "production", block_height);
  msglen = strlen(msg);

  /* generate the vrf proof */
  if( !secp256k1_vrf_prove(proof, plugin->privkey, plugin->pubkey_obj, msg, msglen) ){
    return 0x7FFFFFFF;  /* in the case of failure returns a huge amount of time */
  }

  /* retrieve the verifiable pseudorandom output from the proof */
  if( !secp256k1_vrf_proof_to_hash(output, proof) ){
    return 0x7FFFFFFF;  /* in the case of failure returns a huge amount of time */
  }

  /* get the wait time interval from the random output */
  interval = calculate_wait_interval(output, plugin->total_authorized_nodes, plugin->block_interval);

  /* save the values for use on block generation */
  memcpy(plugin->block_vrf_proof, proof, sizeof proof);
  memcpy(plugin->block_vrf_output, output, sizeof output);

  return interval;
}

/****************************************************************************/

/*
** Returns the amount of time to wait for a block
*/
SQLITE_PRIVATE int get_block_wait_interval(plugin *plugin){
  return plugin->block_interval / 2;
}

/****************************************************************************/

/*
** Verify the VRF proof
*/
SQLITE_PRIVATE int verify_proof(
  plugin *plugin,
  int64 block_height,
  int node_id,
  unsigned char *proof,
  int prooflen,
  unsigned char *output
){
  char msg[64];
  size_t msglen;
  char pubkey[36];
  int rc, pklen;
  bool success;

  SYNCTRACE("verify_proof block_height=%" INT64_FORMAT " node_id=%d\n", block_height, node_id);

  /* generate the seed for this block height */
  sprintf(msg, "block%" INT64_FORMAT "production", block_height);
  msglen = strlen(msg);

  /* get the node's public key */
  rc = aergolite_get_authorization(plugin->this_node, node_id, pubkey, &pklen, NULL, NULL);
  if( rc ) return SQLITE_ERROR;

  /* verify the proof */
  success = secp256k1_vrf_verify(output, proof, (uchar*) pubkey, msg, msglen);

  if( success ) return SQLITE_OK;
  return SQLITE_ERROR;
}
