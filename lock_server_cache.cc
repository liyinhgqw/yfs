/*
 * lock_server_cache.cc
 *
 *  Created on: Oct 15, 2012
 *      Author: liyinhgqw
 */

#include "lock_server.h"
#include "rpc/slock.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server_cache::lock_server_cache()
  :nacquire (0)
{
  for (int i = 0; i < 256; i++) {
    pthread_mutex_init(&m_[i], NULL);
  }
}


lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret;
  r = nacquire;

  return ret;
}

lock_protocol::status
lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, int &r)
{
  lock_protocol::status ret;
  r = nacquire;

  pthread_mutex_lock(&m_[lid & 0xff]);

  add_client(id);
  rpcc *pos = find_lock(lid);
  if (lstatus.find(lid) == lstatus.end()
      || lstatus[lid] == lock_protocol::FREE) {
    lock_pos_[lid] = id;
    ret = lock_protocol::OK;
    lstatus[lid] = lock_protocol::ASSIGNING;
    pthread_mutex_unlock(&m_[lid & 0xff]);
    rpcc_map_[id]->call(lock_protocol::retry, lid, r);
    pthread_mutex_lock(&m_[lid & 0xff]);
    lstatus[lid] = lock_protocol::OWNED;
  } else if (lstatus[lid] == lock_protocol::OWNED){
    if (lock_pos_[lid] == id) {
      printf("you are the lock owner!\n");

    } else {
      lstatus[lid] = lock_protocol::REVOKING;
      pthread_mutex_unlock(&m_[lid & 0xff]);
      pos->call(lock_protocol::revoke, lid, r);
      pthread_mutex_lock(&m_[lid & 0xff]);
      ret = lock_protocol::RETRY;
    }
  } else { // assigning or revoking
    // do nothing because the lock is being revoked
    ret = lock_protocol::RETRY;
  }

  pthread_mutex_unlock(&m_[lid & 0xff]);

  return ret;
}

lock_protocol::status
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r)
{

  lock_protocol::status ret;
  r = nacquire;

  pthread_mutex_lock(&m_[lid & 0xff]);
  add_client(id);
  lock_pos_[lid] = NULL;
  lstatus[lid] = lock_protocol::FREE;
  pthread_mutex_unlock(&m_[lid & 0xff]);

  ret = lock_protocol::OK;

  return ret;
}


