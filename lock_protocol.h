// lock protocol

#ifndef lock_protocol_h
#define lock_protocol_h

#include "rpc.h"

class lock_protocol {
 public:
  enum xxstatus { OK, RETRY, RPCERR, NOENT, IOERR };
  enum cache_status { NONE, FREE, LOCKED, ACQURING, RELEASING,  REVOKING, OWNED, ASSIGNING };
  typedef int status;
  typedef int ccstatus;
  typedef unsigned long long lockid_t;
  typedef unsigned long long xid_t;
  enum rpc_numbers {
    acquire = 0x7001,
    release,
    stat,
    retry,
    revoke
  };
};

class rlock_protocol {
 public:
  enum xxstatus { OK, RPCERR };
  typedef int status;
  enum rpc_numbers {
    revoke = 0x8001,
    retry = 0x8002
  };
};
#endif 
