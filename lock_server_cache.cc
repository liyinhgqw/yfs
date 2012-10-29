/*
 * lock_server_cache.cc
 *
 *  Created on: Oct 15, 2012
 *      Author: liyinhgqw
 */

#include "lock_server_cache.h"
#include "rpc/slock.h"
#include "handle.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server_cache::lock_server_cache()
  :nacquire (0)
{
  pthread_mutex_init(&m_, NULL);
  pthread_mutex_init(&mm_, NULL);
}


lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret;
  r = nacquire;
  ret = lock_protocol::OK;
  return ret;
}

lock_protocol::status
lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, int &r)
{
  printf("\n@ [server] acquire %016llx %s \n", lid, id.c_str());
  pthread_mutex_lock(&m_);

  handle h(id);
  if (lock_pos_.find(lid) == lock_pos_.end()) {
    lock_pos_[lid] = id;
    pthread_mutex_unlock(&m_);
    return lock_protocol::OK;
  } else {
    acquire_map_[lid].push_back(make_pair(id, false));
  }

  pthread_mutex_unlock(&m_);
  check_revoke(lid);
  return lock_protocol::RETRY;
}

lock_protocol::status
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r)
{
  printf("\n@ [server] release %016llx %s \n", lid, id.c_str());
  lock_protocol::status ret;
  r = nacquire;
  pthread_mutex_lock(&m_);

  if (lock_pos_.find(lid) != lock_pos_.end() 
      && lock_pos_[lid] == id) {
    lock_pos_.erase(lid);
  } else {
    printf("wrong owner !\n");
    abort();
  }

  pthread_mutex_unlock(&m_);
  check_retry(lid);
  return lock_protocol::OK;
}

void
lock_server_cache::check_retry(lock_protocol::lockid_t lid)
{
  int r;
  pthread_mutex_lock(&m_);

  // retry
  printf("retry ...");
  if (acquire_map_.find(lid) != acquire_map_.end()
      && !acquire_map_[lid].empty()) {
    std::pair<std::string, bool> first_waiter = acquire_map_[lid].front();
    std::string id = first_waiter.first;
    if (lock_pos_.find(lid) == lock_pos_.end()) {
      // send retry
      lock_pos_[lid] = id;
      acquire_map_[lid].pop_front();
      handle h(id);
      pthread_mutex_unlock(&m_);
      h.safebind()->call(lock_protocol::retry, lid, r);
      pthread_mutex_lock(&m_);
    }
  }

  pthread_mutex_unlock(&m_);
  check_revoke(lid);
}

void
lock_server_cache::check_revoke(lock_protocol::lockid_t lid)
{
  int r;
  pthread_mutex_lock(&m_);

  // revoke
  printf("revoke ... %016llx  ", lid);
  if (acquire_map_.find(lid) != acquire_map_.end() 
      && !acquire_map_[lid].empty()) {
    std::pair<std::string, bool> first_waiter = acquire_map_[lid].front();
    std::string id = first_waiter.first;
    bool sent = first_waiter.second;
    if (!sent && lock_pos_.find(lid) != lock_pos_.end()) {
      acquire_map_[lid].pop_front();
      acquire_map_[lid].push_front(make_pair(id, true));
      // send revoke
      std::string revoke_id = lock_pos_[lid];
      handle h(revoke_id);
      pthread_mutex_unlock(&m_);
      h.safebind()->call(lock_protocol::revoke, lid, r);
      pthread_mutex_lock(&m_);
    }
  }

  pthread_mutex_unlock(&m_);
}

