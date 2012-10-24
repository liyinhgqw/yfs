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
  return hostname + itoa.str();
}

lock_client_cache::lock_client_cache(std::string xdst, lock_release_user *l) {
  sockaddr_in dstsock;
  make_sockaddr(xdst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() < 0) {
    printf("lock_client %s: call bind\n", xdst.c_str());
  }

  for (int i = 0; i < 256; i++) {
    pthread_mutex_init(&m_[i], NULL);
    pthread_cond_init(&c_[i], NULL);
  }

  hostname = "127.0.0.1:";
  rlock_port = 40000 + rand() % 5000;

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
  printf("[client] acquire %016llx %s \n", lid, get_hostandport().c_str());

  lock_protocol::status ret;
  int r;

  pthread_mutex_lock(&m_[lid & 0xff]);    // LOCK status

  while (1) {
    if (lstatus.find(lid) == lstatus.end())
      lstatus[lid] = lock_protocol::NONE;

    lock_protocol::ccstatus cstatus = lstatus[lid];
    if (cstatus == lock_protocol::NONE) {
      lstatus[lid] = lock_protocol::ACQURING;
      pthread_mutex_unlock(&m_[lid & 0xff]);

      ret = cl->call(lock_protocol::acquire, lid, get_hostandport(), r);

      pthread_mutex_lock(&m_[lid & 0xff]);
      if (ret == lock_protocol::OK) {
        // wait for the 'retry' signal
        while (lstatus[lid] != lock_protocol::LOCKED)
          pthread_cond_wait(&c_[lid & 0xff], &m_[lid & 0xff]);

        break;
      } else {
        lstatus[lid] = lock_protocol::NONE;
      }
    } else if (cstatus == lock_protocol::FREE) {
      lstatus[lid] = lock_protocol::LOCKED;
      ret = lock_protocol::OK;
      break;
    } else if (cstatus == lock_protocol::LOCKED) {
      ret = lock_protocol::RETRY;
    } else if (cstatus == lock_protocol::ACQURING) {
      ret = lock_protocol::RETRY;
    } else if (cstatus == lock_protocol::RELEASING) {
      ret = lock_protocol::RETRY;
    } else {
      ret = lock_protocol::IOERR;
      printf("Unknown status !\n");
    }

    sleep(0.2);
  }

  pthread_mutex_unlock(&m_[lid & 0xff]);

  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid) {
  printf("[client] release %016llx %s \n", lid, get_hostandport().c_str());
  lock_protocol::status ret;
  int r;

  pthread_mutex_lock(&m_[lid & 0xff]);    // LOCK status

  if (lstatus.find(lid) == lstatus.end())
    lstatus[lid] = lock_protocol::NONE;

  lock_protocol::ccstatus cstatus = lstatus[lid];
  if (cstatus == lock_protocol::LOCKED) {
    lstatus[lid] = lock_protocol::FREE;
    if (revoke_.find(lid) != revoke_.end()
        && revoke_[lid] == true) {
      lstatus[lid] = lock_protocol::RELEASING;
      pthread_mutex_unlock(&m_[lid & 0xff]);

      ret = cl->call(lock_protocol::release, lid, get_hostandport(), r);

      pthread_mutex_lock(&m_[lid & 0xff]);
      revoke_[lid] = false;
    } else {
      ret = lock_protocol::OK;
    }
  } else {
    ret = lock_protocol::IOERR;
    printf("[Release] not locked !\n");
  }

  pthread_mutex_unlock(&m_[lid & 0xff]);

  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid,
                                        int &) {
  printf("[client] revoke %016llx %s \n", lid, get_hostandport().c_str());

  revoke_[lid] = true;
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid,
                                       int &) {
  printf("[client] retry %016llx %s \n", lid, get_hostandport().c_str());

  pthread_mutex_lock(&m_[lid & 0xff]);    // LOCK status
  lstatus[lid] = lock_protocol::LOCKED;
  pthread_cond_signal(&c_[lid & 0xff]);
  pthread_mutex_unlock(&m_[lid & 0xff]);

  return lock_protocol::OK;
}
