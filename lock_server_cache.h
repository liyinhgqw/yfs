#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  int nacquire;
  std::map<std::string, *rpcc> rpcc_map_;
  std::map<lock_protocol::lockid_t, std::string> lock_pos_;
  pthread_mutex_t m_[256];
  std::map<lock_protocol::lockid_t, lock_protocol::ccstatus> lstatus;

  rpcc* find_lock(lock_protocol::lockid_t lid) {
    if (lock_pos_.find(lid) != lock_pos_.end())
      return rpcc_map_[lock_pos_[lid]];
    else
      return NULL;
  }

  bool add_client(std::string clt_host) {
    if (rpcc_map_.find(clt_host) == rpcc_map_.end()) {
      sockaddr_in dstsock;
      make_sockaddr(clt_host.c_str(), &dstsock);
      rpcc *cl = new rpcc(dstsock);
      if (cl->bind() < 0) {
        printf("lock_client %s: call bind\n", clt_host.c_str());
      }
      rpcc_map_[clt_host] = cl;
      return true;
    }

    return false;
  }

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
