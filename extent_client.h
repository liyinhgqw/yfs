// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "rpc.h"

class extent_client {
public:
  struct inode {
    std::string content;
    extent_protocol::attr att;
    bool is_dirty;
    bool is_removed;
    inode(): is_dirty(false), is_removed(false) {}
  };

 private:
  rpcc *cl;
  std::map<extent_protocol::extentid_t, inode> cache_;
  pthread_mutex_t m_;

  bool is_inode_exist(extent_protocol::extentid_t id);

 public:
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status flush(extent_protocol::extentid_t eid);
};

#endif 

