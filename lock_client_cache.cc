/*
 * lock_client_cache.cc
 *
 *  Created on: Oct 15, 2012
 *      Author: liyinhgqw
 */

#include "lock_client_cache.h"
#include "rpc.h"
#include <arpa/inet.h>

#include <sstream>
#include <iostream>
#include <stdio.h>
#include <string>

std::string
lock_client_cache::get_hostandport() {
  std::ostringstream itoa;
  itoa << rlock_port;
  itoa.flush();
  return hostname + itoa.str();
}

lock_client_cache::lock_client_cache(std::string xdst, lock_release_user *l) {
  sockaddr_in dstsock;
  make_sockaddr(xdst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() < 0) {
    printf("lock_client %s: call bind\n", xdst.c_str());
  }

  pthread_mutex_init(&m_, NULL);
  for (int i = 0; i < 256; i++) {
    pthread_cond_init(&cond_locked_[i], NULL);
    pthread_cond_init(&cond_idle_[i], NULL);
  }

  hostname = "127.0.0.1:";
  rlock_port = 50000 + rand() % 5000;

  int count = 0;
  char *count_env = getenv("RPC_COUNT");
  if(count_env != NULL){
    count = atoi(count_env);
  }
  rpcs *client = new rpcs(rlock_port, count);
  client->reg(lock_protocol::retry, this, &lock_client_cache::retry_handler);
  client->reg(lock_protocol::revoke, this, &lock_client_cache::revoke_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid) {
  printf("\n@ [client] acquire %016llx %s \n", lid, get_hostandport().c_str());
  lock_protocol::status ret;
  int r;

  pthread_mutex_lock(&m_);    // LOCK status

  if (lstatus.find(lid) == lstatus.end())
    lstatus[lid] = lock_protocol::NONE;

  printf("  %s  ", get_hostandport().c_str());

  if (lstatus[lid] == lock_protocol::NONE) printf("<NONE>\n");
  if (lstatus[lid] == lock_protocol::FREE) printf("<FREE>\n");
  if (lstatus[lid] == lock_protocol::LOCKED) printf("<LOCKED>\n");
  if (lstatus[lid] == lock_protocol::ACQUIRING) printf("<ACQUIRING>\n");
  if (lstatus[lid] == lock_protocol::RELEASING) printf("<RELEASING>\n");

  while (1) {
    if (lstatus[lid] == lock_protocol::NONE) {    // NONE
      lstatus[lid] = lock_protocol::ACQUIRING;
      pthread_mutex_unlock(&m_);

      printf("  [client] RPC acquire start %s ... \n", get_hostandport().c_str());
      ret = cl->call(lock_protocol::acquire, lid, get_hostandport(), r);
      printf("  [client] RPC acquire done, ret = %d ! %s \n", ret, get_hostandport().c_str());

      pthread_mutex_lock(&m_);
      while (lstatus[lid] != lock_protocol::LOCKED) {
        pthread_cond_wait(&cond_locked_[lid & 0xff], &m_);
      }
      ret = lock_protocol::OK;
      break;
    } else if (lstatus[lid] == lock_protocol::FREE) {  // FREE
      printf("  granted lock !\n");
      lstatus[lid] = lock_protocol::LOCKED;
      ret = lock_protocol::OK;
      break;
    } else { // LOCKED, ACQUIRING, RELEASING
      while (lstatus[lid] != lock_protocol::NONE 
             || lstatus[lid] != lock_protocol::FREE) {
        pthread_cond_wait(&cond_idle_[lid & 0xff], &m_);
      }
    }
  }

  pthread_mutex_unlock(&m_);

  printf("@@ [client] acquire %016llx %s \n", lid, get_hostandport().c_str());

  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid) {
  printf("\n@ [client] release %016llx %s \n", lid, get_hostandport().c_str());
  lock_protocol::status ret = lock_protocol::OK;
  int r;

  pthread_mutex_lock(&m_);    // LOCK status

  if (lstatus.find(lid) == lstatus.end())
    lstatus[lid] = lock_protocol::NONE;

  if (lstatus[lid] == lock_protocol::LOCKED) {  // LOCKED
    lstatus[lid] = lock_protocol::FREE;
    if (revoke_.find(lid) != revoke_.end()
        && revoke_[lid] == true) {
      lstatus[lid] = lock_protocol::RELEASING;
      pthread_mutex_unlock(&m_);

      printf("  [client] RPC release start %s ... \n", get_hostandport().c_str());
      ret = cl->call(lock_protocol::release, lid, get_hostandport(), r);
      printf("  [client] RPC release done ! \n");

      pthread_mutex_lock(&m_);
      lstatus[lid] = lock_protocol::NONE;
      revoke_[lid] = false;
    }
    pthread_cond_signal(&cond_locked_[lid & 0xff]);
  } else {
    ret = lock_protocol::IOERR;
    printf("  [Release] not locked !\n");
  }

  pthread_mutex_unlock(&m_);

  printf("@@ [client] release %016llx %s \n", lid, get_hostandport().c_str());

  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid,
                                  int &) {
  printf("__ @ [client] revoke %016llx %s \n", lid, get_hostandport().c_str());
  lock_protocol::status ret;
  int r;

  pthread_mutex_lock(&m_);
  revoke_[lid] = true;

  if (lstatus[lid] == lock_protocol::FREE) {
    lstatus[lid] = lock_protocol::RELEASING;
    pthread_mutex_unlock(&m_);

    printf("    [client] RPC release start %s ... \n", get_hostandport().c_str());
    ret = cl->call(lock_protocol::release, lid, get_hostandport(), r);
    printf("    [client] RPC release done ! \n");

    pthread_mutex_lock(&m_);
    revoke_[lid] = false;
    lstatus[lid] = lock_protocol::NONE;
  }
  pthread_mutex_unlock(&m_);

  printf("__ @@ [client] revoke %016llx %s \n", lid, get_hostandport().c_str());
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid,
                                 int &) {
  printf("__ @ [client] retry %016llx %s \n", lid, get_hostandport().c_str());

  pthread_mutex_lock(&m_);    // LOCK status
  lstatus[lid] = lock_protocol::LOCKED;
  pthread_cond_signal(&cond_locked_[lid & 0xff]);
  pthread_mutex_unlock(&m_);

  printf("__ @@ [client] retry %016llx %s \n", lid, get_hostandport().c_str());
  return lock_protocol::OK;
}
