// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
}

yfs_client::inum
yfs_client::get_inum(yfs_client::inode_type type) {
  yfs_client::inum inum;
  inum = rand();
  if (type == yfs_client::FILE) {
    inum |= 0x80000000;
  } else {
    inum &= 0x7FFFFFFF;
  }
  return inum;
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock


  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:
  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}

int
yfs_client::getcontent(inum inum, std::string &content)
{
  int r = OK;

  printf("getcontent %016llx\n", inum);
  if (ec->get(inum, content) != extent_protocol::OK) {
    r = IOERR;
  }
  std::cout << " >> " << content << std::endl;

  return r;
}

int
yfs_client::putcontent(inum inum, std::string content)
{
  int r = OK;

  printf("putcontent %016llx\n", inum);
  if (ec->put(inum, content) != extent_protocol::OK) {
    r = IOERR;
  }

  return r;
}

int
yfs_client::remove(inum inum)
{
  int r = OK;

  printf("remove %016llx\n", inum);
  if (ec->remove(inum) != extent_protocol::OK) {
    r = IOERR;
  }

  return r;
}

void yfs_client::lock(inum inum)
{
  lc->acquire(inum);
}

void yfs_client::unlock(inum inum)
{
  lc->release(inum);
}



