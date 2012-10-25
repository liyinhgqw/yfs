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

  lock_protocol::status ret = lock_protocol::OK;
  r = nacquire;

  pthread_mutex_lock(&m_);

  if (lstatus_.find(lid) == lstatus_.end()
      || lstatus_[lid] == lock_protocol::FREE) {  // Free: acquire_list_ is empty
    lock_pos_[lid] = id;
    ret = lock_protocol::OK;
    lstatus_[lid] = lock_protocol::ASSIGNING;
    pthread_mutex_unlock(&m_);
    // bind and send retry
    handle h(id);
    if (h.safebind()) {
      printf("  [server] call retry, %s \n", id.c_str());
      ret = h.safebind()->call(lock_protocol::retry, lid, r);
      printf("  [server] RPC retry done, %s ! \n", id.c_str());
    }
    if (!h.safebind() || ret != lock_protocol::OK) {
      // handle failure
      printf("safebind error!");
      abort();
    }
    pthread_mutex_lock(&m_);
    lstatus_[lid] = lock_protocol::OWNED;
  } else {  // Not Free
    acquire_list_[lid].push_back(id);
    if (acquire_list_[lid].size() <= 1) {
      pthread_mutex_unlock(&m_);
      // bind and send revoke
      handle h(lock_pos_[lid]);
      if (h.safebind()) {
        printf("  [server] call revoke, %s \n", lock_pos_[lid].c_str());
        ret = h.safebind()->call(lock_protocol::revoke, lid, r);
        printf("  [server] RPC revoke done ! %s\n", lock_pos_[lid].c_str());
      }
      if (!h.safebind() || ret != lock_protocol::OK) {
        // handle failure
        printf("safebind error!");
        abort();
      }
      pthread_mutex_lock(&m_);
    }
  }

  printf("@@ [server] acquire %016llx %s \n", lid, id.c_str());

  pthread_mutex_unlock(&m_);

  return ret;
}

lock_protocol::status
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r)
{
  printf("\n@ [server] release %016llx %s \n", lid, id.c_str());

  lock_protocol::status ret;
  r = nacquire;

  pthread_mutex_lock(&m_);
  if (lock_pos_[lid] == id)
    lock_pos_.erase(lid);
  else
    printf("  %s is not the owner, cannot release ! %s is. \n", id.c_str(), lock_pos_[lid].c_str());

  if (!acquire_list_[lid].empty()) {
    std::string aid = acquire_list_[lid].front();
    acquire_list_[lid].pop_front();
    lock_pos_[lid] = aid;
    ret = lock_protocol::OK;
    lstatus_[lid] = lock_protocol::ASSIGNING;
    pthread_mutex_unlock(&m_);
    // bind and send retry
    handle h(aid);
    if (h.safebind()) {
      printf("  [server] call retry, %s \n", aid.c_str());
      ret = h.safebind()->call(lock_protocol::retry, lid, r);
      printf("  [server] RPC retry done, %s ! \n", aid.c_str());
    }
    if (!h.safebind() || ret != lock_protocol::OK) {
      // handle failure
      printf("safebind error!");
      abort();
    }

    pthread_mutex_lock(&m_);
    lstatus_[lid] = lock_protocol::OWNED;

    if (!acquire_list_[lid].empty()) {
      pthread_mutex_unlock(&m_);
      // bind and send revoke
      handle h(aid);
      if (h.safebind()) {
        printf("  [server] call revoke %s \n", aid.c_str());
        ret = h.safebind()->call(lock_protocol::revoke, lid, r);
        printf("  [server] RPC revoke done ! %s \n", aid.c_str());
      }
      if (!h.safebind() || ret != lock_protocol::OK) {
        // handle failure
        printf("safebind error!");
        abort();
      }
      pthread_mutex_lock(&m_);
    }
  } else {
    lstatus_[lid] = lock_protocol::FREE;
  }
  pthread_mutex_unlock(&m_);

  printf("@@ [server] release %016llx %s \n", lid, id.c_str());

  ret = lock_protocol::OK;

  return ret;
}


