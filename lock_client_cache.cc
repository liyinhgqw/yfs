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

void lock_client_cache::set_hostandport() {
  std::ostringstream itoa;
  itoa << rlock_port;
  host_and_port = hostname + itoa.str();
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
  }

  hostname = "127.0.0.1:";
  rlock_port = 50000 + rand() % 5000;
  set_hostandport();

  int count = 0;
  char *count_env = getenv("RPC_COUNT");
  if (count_env != NULL) {
    count = atoi(count_env);
  }
  rpcs *client = new rpcs(rlock_port, count);
  client->reg(lock_protocol::retry, this, &lock_client_cache::retry_handler);
  client->reg(lock_protocol::revoke, this, &lock_client_cache::revoke_handler);
}

lock_protocol::status lock_client_cache::acquire(lock_protocol::lockid_t lid) {
  printf("@ [client] acquire %016llx %s \n", lid, host_and_port.c_str());
  lock_protocol::status ret;
  int r;

  pthread_mutex_lock(&m_);

  if (lstatus_.find(lid) == lstatus_.end())
    lstatus_[lid] = lock_protocol::NONE;

  lock_protocol::status status = lstatus_[lid];
  switch (status) {
    case lock_protocol::NONE:
      lstatus_[lid] = lock_protocol::ACQUIRING;
      pthread_mutex_unlock(&m_);
      ret = cl->call(lock_protocol::acquire, lid, host_and_port, r);
      pthread_mutex_lock(&m_);
      if (ret == lock_protocol::OK) {
        lstatus_[lid] = lock_protocol::LOCKED;
      } else if (ret == lock_protocol::RETRY) {
        while (lstatus_[lid] != lock_protocol::LOCKED) {
          pthread_cond_wait(&cond_locked_[lid & 0xff], &m_);
        }
      } else {
        printf("bad ret ! \n");
        abort();
      }
      break;
    case lock_protocol::FREE:
      lstatus_[lid] = lock_protocol::LOCKED;
      break;
    default:
      printf("bad other ret ! \n");
      break;
  }
  pthread_mutex_unlock(&m_);

  return lock_protocol::OK;
}

lock_protocol::status lock_client_cache::release(lock_protocol::lockid_t lid) {
  printf("@ [client] release %016llx %s \n", lid, host_and_port.c_str());
  lock_protocol::status ret;
  int r;

  pthread_mutex_lock(&m_);

  if (lstatus_.find(lid) == lstatus_.end())
    lstatus_[lid] = lock_protocol::NONE;

  lock_protocol::status status = lstatus_[lid];
  if (status == lock_protocol::LOCKED) {
    lstatus_[lid] = lock_protocol::FREE;
    if (revoke_set_.find(lid) != revoke_set_.end()) {
      // release the lock to server
      lstatus_[lid] = lock_protocol::RELEASING;
      pthread_mutex_unlock(&m_);
      ret = cl->call(lock_protocol::release, lid, host_and_port, r);
      pthread_mutex_lock(&m_);
      lstatus_[lid] = lock_protocol::NONE;
      revoke_set_.erase(lid);
    }
  } else {
    printf("Release not locked !\n");
    abort();
  }

  pthread_mutex_unlock(&m_);
  return lock_protocol::OK;
}

rlock_protocol::status lock_client_cache::revoke_handler(
    lock_protocol::lockid_t lid, int &) {
  printf("@ [client] revoke %016llx %s \n", lid, host_and_port.c_str());
  lock_protocol::status ret;
  int r;

  pthread_mutex_lock(&m_);

  revoke_set_.insert(lid);
  if (lstatus_[lid] == lock_protocol::FREE) {
    // release the lock to server
    lstatus_[lid] = lock_protocol::RELEASING;
    pthread_mutex_unlock(&m_);
    ret = cl->call(lock_protocol::release, lid, host_and_port, r);
    pthread_mutex_lock(&m_);
    lstatus_[lid] = lock_protocol::NONE;
    revoke_set_.erase(lid);
  }

  pthread_mutex_unlock(&m_);
  return lock_protocol::OK;
}

rlock_protocol::status lock_client_cache::retry_handler(
    lock_protocol::lockid_t lid, int &) {
  printf("[client] retry %016llx %s \n", lid, host_and_port.c_str());

  pthread_mutex_lock(&m_);    // LOCK status
  lstatus_[lid] = lock_protocol::LOCKED;
  pthread_cond_signal(&cond_locked_[lid & 0xff]);
  pthread_mutex_unlock(&m_);

  return lock_protocol::OK;
}
