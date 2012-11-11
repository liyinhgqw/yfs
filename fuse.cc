/*
 * receive request from fuse and call methods of yfs_client
 *
 * started life as low-level example in the fuse distribution.  we
 * have to use low-level interface in order to get i-numbers.  the
 * high-level interface only gives us complete paths.
 */

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <sstream>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "yfs_client.h"


int myid;
yfs_client *yfs;

int id() { 
  return myid;
}

//
// A file/directory's attributes are a set of information
// including owner, permissions, size, &c. The information is
// much the same as that returned by the stat() system call.
// The kernel needs attributes in many situations, and some
// fuse functions (such as lookup) need to return attributes
// as well as other information, so getattr() gets called a lot.
//
// YFS fakes most of the attributes. It does provide more or
// less correct values for the access/modify/change times
// (atime, mtime, and ctime), and correct values for file sizes.
//
yfs_client::status
getattr(yfs_client::inum inum, struct stat &st)
{
  yfs_client::status ret;

  bzero(&st, sizeof(st));

  st.st_ino = inum;
  printf("getattr %016llx %d\n", inum, yfs->isfile(inum));
  if(yfs->isfile(inum)){
     yfs_client::fileinfo info;
     ret = yfs->getfile(inum, info);
     if(ret != yfs_client::OK)
       return ret;
     st.st_mode = S_IFREG | 0666;
     st.st_nlink = 1;
     st.st_atime = info.atime;
     st.st_mtime = info.mtime;
     st.st_ctime = info.ctime;
     st.st_size = info.size;
     printf("   getattr -> %llu\n", info.size);
   } else {
     yfs_client::dirinfo info;
     ret = yfs->getdir(inum, info);
     if(ret != yfs_client::OK)
       return ret;
     st.st_mode = S_IFDIR | 0777;
     st.st_nlink = 2;
     st.st_atime = info.atime;
     st.st_mtime = info.mtime;
     st.st_ctime = info.ctime;
     printf("   getattr -> %lu %lu %lu\n", info.atime, info.mtime, info.ctime);
   }
   return yfs_client::OK;
}

//
// This is a typical fuse operation handler; you'll be writing
// a bunch of handlers like it.
//
// A handler takes some arguments
// and supplies either a success or failure response. It provides
// an error response by calling either fuse_reply_err(req, errno), and
// a normal response by calling ruse_reply_xxx(req, ...). The req
// argument serves to link up this response with the original
// request; just pass the same @req that was passed into the handler.
// 
// The @ino argument indicates the file or directory FUSE wants
// you to operate on. It's a 32-bit FUSE identifier; just assign
// it to a yfs_client::inum to get a 64-bit YFS inum.
//
void
fuseserver_getattr(fuse_req_t req, fuse_ino_t ino,
                   struct fuse_file_info *fi)
{
    struct stat st;
    yfs_client::inum inum = ino; // req->in.h.nodeid;
    yfs_client::status ret;

    yfs->lock(inum);             // LOCK
    ret = getattr(inum, st);
    yfs->unlock(inum);           // UNLOCK

    if(ret != yfs_client::OK){
      fuse_reply_err(req, ENOENT);
      return;
    }
    fuse_reply_attr(req, &st, 0);
}

//
// Set the attributes of a file. Often used as part of overwriting
// a file, to set the file length to zero.
//
// to_set is a bitmap indicating which attributes to set. You only
// have to implement the FUSE_SET_ATTR_SIZE bit, which indicates
// that the size of the file should be changed. The new size is
// in attr->st_size. If the new size is bigger than the current
// file size, fill the new bytes with '\0'.
//
// On success, call fuse_reply_attr, passing the file's new
// attributes (from a call to getattr()).
//
void
fuseserver_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
                   int to_set, struct fuse_file_info *fi)
{
  printf("fuseserver_setattr 0x%x\n", to_set);
  yfs_client::inum inum = ino;
  yfs->lock(inum);                                // LOCK
  if (FUSE_SET_ATTR_SIZE & to_set) {
    printf("   fuseserver_setattr set size to %zu\n", attr->st_size);
    struct stat st;
    // You fill this in for Lab 2
    // Change the above line to "#if 1", and your code goes here
    // Note: fill st using getattr before fuse_reply_attr
    yfs_client::status ret;
    std::string content_buf;

    ret = yfs->getcontent(inum, content_buf);
    if(ret != yfs_client::OK){
      fuse_reply_err(req, ENOENT);
      goto release;
    }

    size_t new_size, cur_size;
    new_size = attr->st_size;
    cur_size = content_buf.size();
    if (new_size >= cur_size) {
      content_buf.append(new_size - cur_size, '\0');
    } else {
      content_buf = content_buf.substr(0, new_size);
    }
    yfs->putcontent(inum, content_buf);
    getattr(inum, st);
    fuse_reply_attr(req, &st, 0);
  } else {
    fuse_reply_err(req, ENOSYS);
  }

release:
  yfs->unlock(inum);                           // UNLOCK
  return;
}

//
// Read up to @size bytes starting at byte offset @off in file @ino.
//
// Pass the number of bytes actually read to fuse_reply_buf.
// If there are fewer than @size bytes to read between @off and the
// end of the file, read just that many bytes. If @off is greater
// than or equal to the size of the file, read zero bytes.
//
// Ignore @fi. 
// @req identifies this request, and is used only to send a 
// response back to fuse with fuse_reply_buf or fuse_reply_err.
//
void
fuseserver_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                off_t off, struct fuse_file_info *fi)
{
  // You fill this in for Lab 2
  std::string buf;
  // Change the above "#if 0" to "#if 1", and your code goes here
  yfs_client::inum inum = ino;
  yfs_client::status ret;

  yfs->lock(inum);                                      // LOCK
  ret = yfs->getcontent(inum, buf);
  printf("read content_buf: %s\n", buf.c_str());
  if(ret != yfs_client::OK){
    fuse_reply_err(req, ENOENT);
    goto release;
  }

  buf = buf.substr(off, size);
  fuse_reply_buf(req, buf.c_str(), buf.size());

release:
  yfs->unlock(inum);                                   // UNLOCK
  return;
}

//
// Write @size bytes from @buf to file @ino, starting
// at byte offset @off in the file.
//
// If @off + @size is greater than the current size of the
// file, the write should cause the file to grow. If @off is
// beyond the end of the file, fill the gap with null bytes.
//
// Set the file's mtime to the current time.
//
// Ignore @fi.
//
// @req identifies this request, and is used only to send a 
// response back to fuse with fuse_reply_buf or fuse_reply_err.
//
void
fuseserver_write(fuse_req_t req, fuse_ino_t ino,
                 const char *buf, size_t size, off_t off,
                 struct fuse_file_info *fi)
{
  // You fill this in for Lab 2
  // Change the above line to "#if 1", and your code goes here
  yfs_client::inum inum = ino;
  yfs_client::status ret;
  std::string content_buf;
  std::string write_buf = std::string(buf);

  yfs->lock(inum);                                         // LOCK
  ret = yfs->getcontent(inum, content_buf);
  if(ret != yfs_client::OK){
    fuse_reply_err(req, ENOENT);
    goto release;
  }

  if (write_buf.size() >= size) {
    write_buf = write_buf.substr(0, size);
  } else {
    write_buf.append(size - write_buf.size(), '\0');
  }

  printf("# %d , %ld , %d \n", content_buf.size(), off, size);
  if (content_buf.size() <= off) {
    content_buf.append(off - content_buf.size(), '\0');
    content_buf.append(write_buf);
  } else if (content_buf.size() <= off + size) {
    printf("!!!!");
    content_buf = content_buf.substr(0, off);
    content_buf.append(write_buf);
  } else {
    content_buf.replace(off, size, write_buf);
  }
  yfs->putcontent(inum, content_buf);

  fuse_reply_write(req, size);

release:
  yfs->unlock(inum);                                      // UNLOCK
  return;
}

//
// Create file @name in directory @parent. 
//
// - @mode specifies the create mode of the file. Ignore it - you do not
//   have to implement file mode.
// - If a file named @name already exists in @parent, return EXIST.
// - Pick an ino (with type of yfs_client::inum) for file @name. 
//   Make sure ino indicates a file, not a directory!
// - Create an empty extent for ino.
// - Add a <name, ino> entry into @parent.
// - Change the parent's mtime and ctime to the current time/date
//   (this may fall naturally out of your extent server code).
// - On success, store the inum of newly created file into @e->ino, 
//   and the new file's attribute into @e->attr. Get the file's
//   attributes with getattr().
//
// @return yfs_client::OK on success, and EXIST if @name already exists.
//
yfs_client::status
fuseserver_createhelper(fuse_ino_t parent, const char *name,
                        mode_t mode, struct fuse_entry_param *e)
{
  // In yfs, timeouts are always set to 0.0, and generations are always set to 0
  e->attr_timeout = 0.0;
  e->entry_timeout = 0.0;
  e->generation = 0;
  // You fill this in for Lab 2
  yfs_client::inum parent_inum = parent;
  yfs_client::status ret;

  yfs->lock(parent_inum);                            // LOCK parent

  if (yfs->isdir(parent_inum)) {
    std::string buf;
    ret = yfs->getcontent(parent, buf);

    if (ret == yfs_client::OK) {
      std::string name_str = std::string(name);
      if (buf.find(name_str + "=") != std::string::npos) {
        printf("file %s exists. \n", name);
        ret = yfs_client::EXIST;
        goto release;
      }

      yfs_client::inum inum = yfs->get_inum(yfs_client::FILE);
      buf.append(name_str);
      buf.append("=");
      buf.append(yfs_client::filename(inum));
      buf.append(";");

      yfs->lock(inum);                          // LOCK
      yfs->putcontent(inum, "");
      yfs->unlock(inum);  // UNLOCK
      yfs->putcontent(parent_inum, buf);

      e->ino = inum;
      getattr(inum, e->attr);
    }
  }

release:
  yfs->unlock(parent_inum);                          // UNLOCK parent
  return ret;
}

void
fuseserver_create(fuse_req_t req, fuse_ino_t parent, const char *name,
                  mode_t mode, struct fuse_file_info *fi)
{
  struct fuse_entry_param e;
  yfs_client::status ret;
  if( (ret = fuseserver_createhelper( parent, name, mode, &e )) == yfs_client::OK ) {
    fuse_reply_create(req, &e, fi);
  } else {
		if (ret == yfs_client::EXIST) {
			fuse_reply_err(req, EEXIST);
		}else{
			fuse_reply_err(req, ENOENT);
		}
  }
}

void fuseserver_mknod( fuse_req_t req, fuse_ino_t parent, 
    const char *name, mode_t mode, dev_t rdev ) {
  struct fuse_entry_param e;
  yfs_client::status ret;
  if( (ret = fuseserver_createhelper( parent, name, mode, &e )) == yfs_client::OK ) {
    fuse_reply_entry(req, &e);
  } else {
		if (ret == yfs_client::EXIST) {
			fuse_reply_err(req, EEXIST);
		}else{
			fuse_reply_err(req, ENOENT);
		}
  }
}

//
// Look up file or directory @name in the directory @parent. If @name is
// found, set e.attr (using getattr) and e.ino to the attribute and inum of
// the file.
//
void
fuseserver_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
  struct fuse_entry_param e;
  // In yfs, timeouts are always set to 0.0, and generations are always set to 0
  e.attr_timeout = 0.0;
  e.entry_timeout = 0.0;
  e.generation = 0;
  bool found = false;
  yfs_client::status ret;

  std::cout << "... lookup ... [parent:]" << parent << ", [name:]" << name << std::endl;
  // You fill this in for Lab 2
  yfs_client::inum parent_inum = parent;
  yfs->lock(parent_inum);             // LOCK parent
  if (yfs->isdir(parent_inum)) {
    std::string buf;
    ret = yfs->getcontent(parent_inum, buf);

    if (ret == yfs_client::OK) {
      std::string name_str = std::string(name);
      size_t pos = buf.find(name_str + "=");
      if (pos != std::string::npos) {
        found = true;
        std::string temp = buf.substr(pos);
        std::cout << " temp: " << temp << std::endl;
        size_t start_pos = temp.find_first_of('=');
        size_t end_pos = temp.find_first_of(";");
        std::string inum_str = temp.substr(start_pos + 1, end_pos - start_pos - 1);
        std::cout << "inum_str" << inum_str << std::endl;
        yfs_client::inum inum = yfs_client::n2i(inum_str);
        e.ino = inum;
        std::cout << "--- lookup ... [inum:]" << inum << std::endl;

        yfs->lock(inum);                // LOCK
        getattr(inum, e.attr);
        yfs->unlock(inum);
      }
    }
  }
  yfs->unlock(parent_inum);

  if (found)
    fuse_reply_entry(req, &e);
  else
    fuse_reply_err(req, ENOENT);
}


struct dirbuf {
    char *p;
    size_t size;
};

void dirbuf_add(struct dirbuf *b, const char *name, fuse_ino_t ino)
{
    struct stat stbuf;
    size_t oldsize = b->size;
    b->size += fuse_dirent_size(strlen(name));
    b->p = (char *) realloc(b->p, b->size);
    memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_ino = ino;
    fuse_add_dirent(b->p + oldsize, name, &stbuf, b->size);
}

#define min(x, y) ((x) < (y) ? (x) : (y))

int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
          off_t off, size_t maxsize)
{
  if ((size_t)off < bufsize)
    return fuse_reply_buf(req, buf + off, min(bufsize - off, maxsize));
  else
    return fuse_reply_buf(req, NULL, 0);
}

//
// Retrieve all the file names / i-numbers pairs
// in directory @ino. Send the reply using reply_buf_limited.
//
// You can ignore @size and @off (except that you must pass
// them to reply_buf_limited).
//
// Call dirbuf_add(&b, name, inum) for each entry in the directory.
//
void
fuseserver_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                   off_t off, struct fuse_file_info *fi)
{
  yfs_client::inum inum = ino; // req->in.h.nodeid;
  struct dirbuf b;

  printf("fuseserver_readdir\n");

  if(!yfs->isdir(inum)){
    fuse_reply_err(req, ENOTDIR);
    return;
  }

  memset(&b, 0, sizeof(b));


  // You fill this in for Lab 2
  yfs->lock(inum);            // LOCK
  std::string buf;
  yfs->getcontent(inum, buf);
  std::stringstream ss(buf);
  std::vector<std::string> inode_list;

  std::string inode_buf;
  while (std::getline(ss, inode_buf, ';')) {
    inode_list.push_back(inode_buf);
  }
  size_t pos;
  std::string inode_name;
  yfs_client::inum inode_inum;
  for (int i = 0; i < inode_list.size(); i++) {
    pos = inode_list[i].find("=");
    inode_name = inode_list[i].substr(0, pos);
    inode_inum = yfs_client::n2i(inode_list[i].substr(pos + 1));
    dirbuf_add(&b, inode_name.c_str(), inode_inum);
  }
  yfs->unlock(inum);         // LOCK

  reply_buf_limited(req, b.p, b.size, off, size);
  free(b.p);
}


void
fuseserver_open(fuse_req_t req, fuse_ino_t ino,
     struct fuse_file_info *fi)
{
  fuse_reply_open(req, fi);
}

//
// Create a new directory with name @name in parent directory @parent.
// Leave new directory's inum in e.ino and attributes in e.attr.
//
// The new directory should be empty (no . or ..).
// 
// If a file/directory named @name already exists, indicate error EEXIST.
//
// Ignore mode.
//
void
fuseserver_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
     mode_t mode)
{
  struct fuse_entry_param e;
  // In yfs, timeouts are always set to 0.0, and generations are always set to 0
  e.attr_timeout = 0.0;
  e.entry_timeout = 0.0;
  e.generation = 0;
  // Suppress compiler warning of unused e.
  (void) e;

  yfs_client::status ret;

  // You fill this in for Lab 3
  yfs_client::inum parent_inum = parent;
  yfs->lock(parent_inum);       // LOCK parent

  if (yfs->isdir(parent_inum)) {
    std::string buf;
    ret = yfs->getcontent(parent_inum, buf);


    if (ret == yfs_client::OK) {
      std::string name_str = std::string(name);
      if (buf.find(name_str + "=") != std::string::npos) {
        printf("dir %s exists. \n", name);
        fuse_reply_err(req, EEXIST);
      } else {
        yfs_client::inum inum = yfs->get_inum(yfs_client::DIR);
        buf.append(name_str);
        buf.append("=");
        buf.append(yfs_client::filename(inum));
        buf.append(";");

        yfs->lock(inum);              // LOCK
        yfs->putcontent(inum, "");
        yfs->putcontent(parent_inum, buf);

        e.ino = inum;
        getattr(inum, e.attr);

        yfs->unlock(inum);

        fuse_reply_entry(req, &e);
      }
    } else {
      fuse_reply_err(req, ENOSYS);
    }
  }

  yfs->unlock(parent_inum);
}

//
// Remove the file named @name from directory @parent.
// Free the file's extent.
// If the file doesn't exist, indicate error ENOENT.
//
// Do *not* allow unlinking of a directory.
//
void
fuseserver_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{

  // You fill this in for Lab 3
  // Success:	fuse_reply_err(req, 0);
  // Not found:	fuse_reply_err(req, ENOENT);
  yfs_client::status ret;
  yfs_client::inum parent_inum = parent;
  if (yfs->isdir(parent_inum)) {
    std::string buf;
    yfs->lock(parent_inum);                     // LOCK parent
    ret = yfs->getcontent(parent_inum, buf);

    if (ret == yfs_client::OK) {
      std::string name_str = std::string(name);
      size_t pos = buf.find(name_str + "=");
      size_t epos = buf.find_first_of(";", pos);

      if (pos == std::string::npos) {
        printf("file %s exists. \n", name);
        fuse_reply_err(req, ENOENT);
      } else {
        printf("great");
        std::string buf_inum = buf.substr(pos);
        size_t begin_pos = buf_inum.find_first_of("=") + 1;
        size_t end_pos = buf_inum.find_first_not_of(";");
        buf_inum = buf_inum.substr(begin_pos, end_pos - begin_pos + 1);

        yfs_client::inum inum = yfs->n2i(buf_inum);
        yfs->lock(inum);                          // LOCK inum
        if (yfs->isdir(inum)) {
          fuse_reply_err(req, ENOENT);
        } else {
          yfs->remove(inum);
          buf = buf.replace(pos, epos - pos + 1, "");
          yfs->putcontent(parent, buf);
        }
        yfs->unlock(inum);

        fuse_reply_err(req, 0);
      }
    } else {
      fuse_reply_err(req, ENOSYS);
    }
    yfs->unlock(parent_inum);
  }
}

void
fuseserver_statfs(fuse_req_t req)
{
  struct statvfs buf;

  printf("statfs\n");

  memset(&buf, 0, sizeof(buf));

  buf.f_namemax = 255;
  buf.f_bsize = 512;

  fuse_reply_statfs(req, &buf);
}

struct fuse_lowlevel_ops fuseserver_oper;

int
main(int argc, char *argv[])
{
  char *mountpoint = 0;
  int err = -1;
  int fd;

  setvbuf(stdout, NULL, _IONBF, 0);

  if(argc != 4){
    fprintf(stderr, "Usage: yfs_client <mountpoint> <port-extent-server> <port-lock-server>\n");
    exit(1);
  }
  mountpoint = argv[1];

  srandom(getpid());

  myid = random();

  yfs = new yfs_client(argv[2], argv[3]);

  fuseserver_oper.getattr    = fuseserver_getattr;
  fuseserver_oper.statfs     = fuseserver_statfs;
  fuseserver_oper.readdir    = fuseserver_readdir;
  fuseserver_oper.lookup     = fuseserver_lookup;
  fuseserver_oper.create     = fuseserver_create;
  fuseserver_oper.mknod      = fuseserver_mknod;
  fuseserver_oper.open       = fuseserver_open;
  fuseserver_oper.read       = fuseserver_read;
  fuseserver_oper.write      = fuseserver_write;
  fuseserver_oper.setattr    = fuseserver_setattr;
  fuseserver_oper.unlink     = fuseserver_unlink;
  fuseserver_oper.mkdir      = fuseserver_mkdir;

  const char *fuse_argv[20];
  int fuse_argc = 0;
  fuse_argv[fuse_argc++] = argv[0];
#ifdef __APPLE__
  fuse_argv[fuse_argc++] = "-o";
  fuse_argv[fuse_argc++] = "nolocalcaches"; // no dir entry caching
  fuse_argv[fuse_argc++] = "-o";
  fuse_argv[fuse_argc++] = "daemon_timeout=86400";
#endif

  // everyone can play, why not?
  //fuse_argv[fuse_argc++] = "-o";
  //fuse_argv[fuse_argc++] = "allow_other";

  fuse_argv[fuse_argc++] = mountpoint;
  fuse_argv[fuse_argc++] = "-d";

  fuse_args args = FUSE_ARGS_INIT( fuse_argc, (char **) fuse_argv );
  int foreground;
  int res = fuse_parse_cmdline( &args, &mountpoint, 0 /*multithreaded*/,
        &foreground );
  if( res == -1 ) {
    fprintf(stderr, "fuse_parse_cmdline failed\n");
    return 0;
  }

  args.allocated = 0;

  fd = fuse_mount(mountpoint, &args);
  if(fd == -1){
    fprintf(stderr, "fuse_mount failed\n");
    exit(1);
  }

  struct fuse_session *se;

  se = fuse_lowlevel_new(&args, &fuseserver_oper, sizeof(fuseserver_oper),
       NULL);
  if(se == 0){
    fprintf(stderr, "fuse_lowlevel_new failed\n");
    exit(1);
  }

  struct fuse_chan *ch = fuse_kern_chan_new(fd);
  if (ch == NULL) {
    fprintf(stderr, "fuse_kern_chan_new failed\n");
    exit(1);
  }

  fuse_session_add_chan(se, ch);
  // err = fuse_session_loop_mt(se);   // FK: wheelfs does this; why?
  err = fuse_session_loop(se);

  fuse_session_destroy(se);
  close(fd);
  fuse_unmount(mountpoint);

  return err ? 1 : 0;
}
