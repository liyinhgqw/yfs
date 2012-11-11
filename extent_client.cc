// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }

  pthread_mutex_init(&m_, NULL);
}

bool extent_client::is_inode_exist(extent_protocol::extentid_t id) {
  if (cache_.find(id) != cache_.end())
    return true;
  return false;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;

  ScopedLock m1(&m_);

  if (is_inode_exist(eid)) {
    if (!cache_[eid].is_removed) {
      buf = cache_[eid].content;
      cache_[eid].att.atime = time(NULL);
    } else {
      ret = extent_protocol::NOENT;
    }
  } else {
    ret = cl->call(extent_protocol::get, eid, buf);
    if (ret == extent_protocol::OK) {
      cache_[eid].content = buf;
      ret = cl->call(extent_protocol::getattr, eid, cache_[eid].att);
    }
  }
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;

  ScopedLock m1(&m_);

  if (is_inode_exist(eid)) {
    if (!cache_[eid].is_removed) {
      extent_protocol::attr _attr = cache_[eid].att;
      attr.size = _attr.size;
      attr.atime = _attr.atime;
      attr.mtime = _attr.mtime;
      attr.ctime = _attr.ctime;
    } else {
      ret = extent_protocol::NOENT;
    }
  } else {
    ret = cl->call(extent_protocol::getattr, eid, attr);
    cache_[eid].att = attr;
    ret = cl->call(extent_protocol::get, eid, cache_[eid].content);
  }
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;

  ScopedLock m1(&m_);
  cache_[eid].content = buf;
  time_t now = time(NULL);
  extent_protocol::attr &_attr = cache_[eid].att;
  _attr.atime = now;
  _attr.ctime = now;
  _attr.mtime = now;
  _attr.size = buf.size();
  cache_[eid].is_dirty = true;
  cache_[eid].is_removed = false;


//  ret = cl->call(extent_protocol::put, eid, buf, r);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;

  ScopedLock m1(&m_);

  cache_[eid].is_removed = true;
  return ret;
}

extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  printf("flush ... \n");
  ScopedLock m1(&m_);
  if (!is_inode_exist(eid)) return ret;
  inode cached = cache_[eid];
  if (cached.is_dirty) {
    if (cached.is_removed) {
      ret = cl->call(extent_protocol::remove, eid, r);
    } else {
      printf("flush put: %s\n", cached.content.c_str());
      ret = cl->call(extent_protocol::put, eid, cached.content, r);
      assert(ret == extent_protocol::OK);
      ret = cl->call(extent_protocol::setattr, eid, cached.att, r);
    }
  }
  cache_.erase(eid);

  return ret;
}


