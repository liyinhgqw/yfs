// the lock server implementation

#include "lock_server.h"
#include "rpc/slock.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>


lock_server::lock_server():
  nacquire (0)
{
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  ScopedLock m1(&m_);

  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  ScopedLock m1(&m_);

  lock_protocol::status ret = lock_protocol::OK;
  printf("acquire lock %llu request from clt %d\n", lid, clt);

  while (lock_dir_[lid] == lock_server::LOCKED)
    pthread_cond_wait(&lock_c_[lid & 0xff], &m_);

  lock_dir_[lid] = lock_server::LOCKED;

  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  ScopedLock m1(&m_);

  lock_protocol::status ret = lock_protocol::OK;
  printf("release lock %llu request from clt %d\n", lid, clt);

  pthread_cond_signal(&lock_c_[lid & 0xff]);
  lock_dir_[lid] = lock_server::FREE;


  r = nacquire;
  return ret;
}


