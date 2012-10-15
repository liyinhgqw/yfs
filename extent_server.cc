// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {
  pthread_mutex_init(&m_, NULL);
  int r;
  int ret = put(0x00000001, "", r);
  printf("** put root !\n");
  assert (ret == extent_protocol::OK);
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &) {
  // You fill this in for Lab 2.
  ScopedLock m1(&m_);
  inode_dir_[id].content = buf;
  time_t now = time(NULL);
  extent_protocol::attr &_attr = inode_dir_[id].att;
  _attr.atime = now;
  _attr.ctime = now;
  _attr.mtime = now;
  _attr.size = buf.size();
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf) {
  // You fill this in for Lab 2.
  ScopedLock m1(&m_);

  if (is_inode_exist(id)) {
    buf = inode_dir_[id].content;
    extent_protocol::attr _attr = inode_dir_[id].att;
    _attr.atime = time(NULL);
    return extent_protocol::OK;
  }
  return extent_protocol::NOENT;
}

int extent_server::getattr(extent_protocol::extentid_t id,
                           extent_protocol::attr &a) {
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  ScopedLock m1(&m_);

  if (is_inode_exist(id)) {
    extent_protocol::attr _attr = inode_dir_[id].att;
    a.size = _attr.size;
    a.atime = _attr.atime;
    a.mtime = _attr.mtime;
    a.ctime = _attr.ctime;
    return extent_protocol::OK;
  }
  return extent_protocol::NOENT;
}

int extent_server::remove(extent_protocol::extentid_t id, int &) {
  // You fill this in for Lab 2.
  ScopedLock m1(&m_);

  if (is_inode_exist(id)) {
    inode_dir_.erase(id);
    return extent_protocol::OK;
  }
  return extent_protocol::NOENT;
}

bool extent_server::is_inode_exist(extent_protocol::extentid_t id) {

  if (inode_dir_.find(id) != inode_dir_.end())
    return true;
  return false;
}

