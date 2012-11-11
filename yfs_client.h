#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
// #include "lock_client.h"
#include "lock_client_cache.h"

class yfs_client: public lock_release_user {
  extent_client *ec;
  lock_client *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  enum inode_type { FILE, DIR };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };


  static std::string filename(inum);
  static inum n2i(std::string);


  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int getcontent(inum, std::string &);
  int putcontent(inum, std::string);

  int remove(inum);

  inum get_inum(inode_type);

  void lock(inum);
  void unlock(inum);

  void dorelease(lock_protocol::lockid_t);
};

#endif 
