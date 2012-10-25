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

  pthread_mutex_init(&m_, NULL);
  for (int i = 0; i < 256; i++) {
    pthread_cond_init(&c_[i], NULL);
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

  lock_protocol::status ret;
  int r;
  bool locked = false;

  while (1) {
    pthread_mutex_lock(&m_);    // LOCK status
    
    printf("[client] acquire %016llx %s \n", lid, get_hostandport().c_str());
    if (lstatus.find(lid) == lstatus.end())
      lstatus[lid] = lock_protocol::NONE;

    lock_protocol::ccstatus cstatus = lstatus[lid];
    printf("<status> %d \n", cstatus);

    if (cstatus == lock_protocol::NONE) {
      lstatus[lid] = lock_protocol::ACQURING;
      pthread_mutex_unlock(&m_);

      printf("[client] RPC acquire start ... \n");
      ret = cl->call(lock_protocol::acquire, lid, get_hostandport(), r);
      printf("[client] RPC acquire done, ret = %d ! \n", ret);

      pthread_mutex_lock(&m_);
      if (ret == lock_protocol::OK) {
        // wait for the 'retry' signal
        while (lstatus[lid] != lock_protocol::LOCKED)
          pthread_cond_wait(&c_[lid & 0xff], &m_);
        locked = true;
        break;
      } else {
        lstatus[lid] = lock_protocol::NONE;
      }
    } else if (cstatus == lock_protocol::FREE) {
      lstatus[lid] = lock_protocol::LOCKED;
      ret = lock_protocol::OK;
      locked = true;
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

    pthread_mutex_unlock(&m_);
    sleep(1);
  }

  if (locked) pthread_mutex_unlock(&m_);
  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid) {
  printf("[client] release %016llx %s \n", lid, get_hostandport().c_str());
  lock_protocol::status ret;
  int r;

  pthread_mutex_lock(&m_);    // LOCK status

  if (lstatus.find(lid) == lstatus.end())
    lstatus[lid] = lock_protocol::NONE;

  lock_protocol::ccstatus cstatus = lstatus[lid];
  if (cstatus == lock_protocol::LOCKED) {
    lstatus[lid] = lock_protocol::FREE;
    if (revoke_.find(lid) != revoke_.end()
        && revoke_[lid] == true) {
      lstatus[lid] = lock_protocol::RELEASING;
      pthread_mutex_unlock(&m_);

      ret = cl->call(lock_protocol::release, lid, get_hostandport(), r);
      printf("[client] RPC release done ! \n");
      
      pthread_mutex_lock(&m_);
      lstatus[lid] = lock_protocol::NONE;
      revoke_[lid] = false;
    } else {
      ret = lock_protocol::OK;
    }
  } else {
    ret = lock_protocol::IOERR;
    printf("[Release] not locked !\n");
  }

  pthread_mutex_unlock(&m_);

  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid,
                                        int &) {
  printf("[client] revoke %016llx %s \n", lid, get_hostandport().c_str());
  lock_protocol::status ret;
  int r;


  pthread_mutex_lock(&m_);
  revoke_[lid] = true;
  if (lstatus[lid] == lock_protocol::FREE) {
    pthread_mutex_unlock(&m_);
    ret = cl->call(lock_protocol::release, lid, get_hostandport(), r);
    printf("[client] RPC release done ! \n");
    pthread_mutex_lock(&m_);
    revoke_[lid] = false;
    lstatus[lid] = lock_protocol::NONE;
  }
  pthread_mutex_unlock(&m_);
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid,
                                       int &) {
  printf("[client] retry %016llx %s \n", lid, get_hostandport().c_str());

  pthread_mutex_lock(&m_);    // LOCK status
  lstatus[lid] = lock_protocol::LOCKED;
  pthread_cond_signal(&c_[lid & 0xff]);
  pthread_mutex_unlock(&m_);

  return lock_protocol::OK;
}
