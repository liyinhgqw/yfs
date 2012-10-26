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
  std::map<lock_protocol::lockid_t, std::string> lock_pos_;
  pthread_mutex_t m_, mm_;
  std::map<lock_protocol::lockid_t, std::list< std::pair<std::string, bool> > > acquire_map_;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
  void check_retry_and_revoke(lock_protocol::lockid_t);
};

#endif
