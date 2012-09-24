// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"

class extent_server {

 public:
  struct inode {
    std::string content;
    extent_protocol::attr att;
  };

  extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
  bool is_inode_exist(extent_protocol::extentid_t id);

// private:
  std::map<extent_protocol::extentid_t, inode> inode_dir_;
  pthread_mutex_t m_;
};

#endif 








