// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <map>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

class lock_server {

 public:
  enum lock_st { FREE = 0, LOCKED };

  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);

 protected:
  int nacquire;
  pthread_mutex_t m_;  // only use a single mutex
  pthread_cond_t lock_c_[256]; // lid & 0xff
  std::map<lock_protocol::lockid_t, lock_st> lock_dir_;
};

#endif 







