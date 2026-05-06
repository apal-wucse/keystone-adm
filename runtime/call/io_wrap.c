#ifdef USE_IO_SYSCALL
#include "call/io_wrap.h"

#include <alloca.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/epoll.h>

#include "adm.h"
#include "call/syscall.h"
#include "edge_syscall.h"
#include "uaccess.h"
#include "util/string.h"

/* Syscalls iozone uses in -i0 mode
*** Fake these
 *   uname
*** odd
    rt_sigaction
    rt_sigprocmask
*** hard
    brk
    mmap
*/

#define MAX_STRACE_PRINT 20

#ifndef USE_EDGE_PROTECTION
uintptr_t
io_syscall_sync() {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();

  edge_syscall->syscall_num = SYS_sync;

  size_t totalsize = (sizeof(struct edge_syscall));

  uintptr_t ret = dispatch_edgecall_syscall(edge_syscall, totalsize);
  print_strace("[runtime] proxied sync\r\n");
  return ret;
}

uintptr_t
io_syscall_ftruncate(int fd, off_t offset) {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_ftruncate* args = (sargs_SYS_ftruncate*)edge_syscall->data;
  edge_syscall->syscall_num = SYS_ftruncate;

  args->fd     = fd;
  args->offset = offset;

  size_t totalsize =
      (sizeof(struct edge_syscall) + sizeof(sargs_SYS_ftruncate));

  uintptr_t ret = dispatch_edgecall_syscall(edge_syscall, totalsize);
  print_strace("[runtime] proxied ftruncate (%i) = %li\r\n", fd, ret);
  return ret;
}

uintptr_t
io_syscall_fsync(int fd) {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_fsync* args     = (sargs_SYS_fsync*)edge_syscall->data;
  edge_syscall->syscall_num = SYS_fsync;

  args->fd = fd;

  size_t totalsize = (sizeof(struct edge_syscall) + sizeof(sargs_SYS_fsync));

  uintptr_t ret = dispatch_edgecall_syscall(edge_syscall, totalsize);
  print_strace("[runtime] proxied fsync (%i) = %li\r\n", fd, ret);
  return ret;
}

uintptr_t
io_syscall_lseek(int fd, off_t offset, int whence) {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_lseek* args     = (sargs_SYS_lseek*)edge_syscall->data;
  edge_syscall->syscall_num = SYS_lseek;

  args->fd     = fd;
  args->offset = offset;
  args->whence = whence;

  size_t totalsize = (sizeof(struct edge_syscall) + sizeof(sargs_SYS_lseek));

  uintptr_t ret = dispatch_edgecall_syscall(edge_syscall, totalsize);
  print_strace(
      "[runtime] proxied lseek (on fd:%i to %li from %i) = %li\r\n", fd, offset,
      whence, ret);
  return ret;
}

uintptr_t
io_syscall_close(int fd) {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_close* args     = (sargs_SYS_close*)edge_syscall->data;
  edge_syscall->syscall_num = SYS_close;

  args->fd = fd;

  size_t totalsize = (sizeof(struct edge_syscall) + sizeof(sargs_SYS_close));

  uintptr_t ret = dispatch_edgecall_syscall(edge_syscall, totalsize);
  print_strace("[runtime] proxied close (%i) = %li\r\n", fd, ret);
  return ret;
}

uintptr_t
io_syscall_read(int fd, void* buf, size_t len) {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_read* args      = (sargs_SYS_read*)edge_syscall->data;
  uintptr_t ret             = -1;
  edge_syscall->syscall_num = SYS_read;
  args->fd                  = fd;
  args->len                 = len;

  // Sanity check that the read buffer will fit in the shared memory
  if (edge_call_check_ptr_valid((uintptr_t)args->buf, len) != 0) {
    goto done;
  }

  size_t totalsize =
      (sizeof(struct edge_syscall) + sizeof(sargs_SYS_read) + len);

  ret = dispatch_edgecall_syscall(edge_syscall, totalsize);

  if ((int)ret < 0) {
    goto done;
  }

  // Previously checked that this is staying in untrusted buffer range
  copy_to_user(buf, args->buf, ret > len ? len : ret);

done:
  print_strace(
      "[runtime] proxied read from %i (size: %lu) = %li\r\n", fd, len, ret);
  return ret;
}

uintptr_t
io_syscall_write(int fd, void* buf, size_t len) {
  /* print_strace("[write] len :%lu\r\n", len); */
  /* if(len > 0){ */
  /*   size_t stracelen = len > MAX_STRACE_PRINT? MAX_STRACE_PRINT:len; */
  /*   char* lbuf[MAX_STRACE_PRINT+1]; */
  /*   memset(lbuf, 0, sizeof(lbuf)); */
  /*   copy_from_user(lbuf, (void*)buf, stracelen); */
  /*   print_strace("[write] \"%s\"\r\n", (char*)lbuf); */
  /* } */

  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_write* args = (sargs_SYS_write*)edge_syscall->data;
  uintptr_t ret         = -1;

  edge_syscall->syscall_num = SYS_write;
  args->fd                  = fd;
  args->len                 = len;

  // Sanity check that the write buffer will fit in the shared memory
  if (edge_call_check_ptr_valid((uintptr_t)args->buf, len) != 0) {
    goto done;
  }

  copy_from_user(args->buf, buf, len);

  size_t totalsize =
      (sizeof(struct edge_syscall) + sizeof(sargs_SYS_write) + len);

  ret = dispatch_edgecall_syscall(edge_syscall, totalsize);

done:
  print_strace(
      "[runtime] proxied write to %i (size: %lu) = %li\r\n", fd, len, ret);
  return ret;
}

uintptr_t
io_syscall_openat(int dirfd, char* path, int flags, mode_t mode) {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_openat* args = (sargs_SYS_openat*)edge_syscall->data;

  edge_syscall->syscall_num = SYS_openat;
  args->dirfd               = dirfd;
  args->flags               = flags;
  args->mode                = mode;
  uintptr_t ret             = -1;

  size_t pathlen;
  ALLOW_USER_ACCESS(pathlen = _strlen(path) + 1);
  // Sanity check that the buffer will fit in the shared memory
  if (edge_call_check_ptr_valid((uintptr_t)args->path, pathlen) != 0) {
    goto done;
  }
  copy_from_user(args->path, path, pathlen);

  size_t totalsize =
      (sizeof(struct edge_syscall) + sizeof(sargs_SYS_openat) + pathlen);

  ret = dispatch_edgecall_syscall(edge_syscall, totalsize);

done:
  // TODO path print here isn't necessarily correct or even copied!
  print_strace(
      "[runtime] proxied openat(path: %.*s) = %li\r\n",
      pathlen > MAX_STRACE_PRINT ? MAX_STRACE_PRINT : pathlen, args->path, ret);

  return ret;
}

uintptr_t
io_syscall_unlinkat(int dirfd, char* path, int flags) {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_unlinkat* args = (sargs_SYS_unlinkat*)edge_syscall->data;
  uintptr_t ret            = -1;

  edge_syscall->syscall_num = SYS_unlinkat;
  args->dirfd               = dirfd;
  args->flags               = flags;
  size_t pathlen;
  ALLOW_USER_ACCESS(pathlen = _strlen(path) + 1);
  // Sanity check that the buffer will fit in the shared memory
  if (edge_call_check_ptr_valid((uintptr_t)args->path, pathlen) != 0) {
    goto done;
  }
  copy_from_user(args->path, path, pathlen);

  size_t totalsize =
      (sizeof(struct edge_syscall) + sizeof(sargs_SYS_unlinkat) + pathlen);

  ret = dispatch_edgecall_syscall(edge_syscall, totalsize);

done:
  // TODO path print here isn't necessarily correct or even copied!
  print_strace(
      "[runtime] proxied unlinkat(path: %.*s) = %li\r\n",
      pathlen > MAX_STRACE_PRINT ? MAX_STRACE_PRINT : pathlen, args->path, ret);
  return ret;
}

uintptr_t
io_syscall_writev(int fd, const struct iovec* iov, int iovcnt) {
  int i         = 0;
  uintptr_t ret = 0;
  size_t total  = 0;
  print_strace(
      "[runtime] Simulating writev (cnt %i) with write calls\r\n", iovcnt);
  for (i = 0; i < iovcnt && ret >= 0; i++) {
    struct iovec iov_local;
    copy_from_user(&iov_local, &(iov[i]), sizeof(struct iovec));
    ret = io_syscall_write(fd, iov_local.iov_base, iov_local.iov_len);
    total += ret;
  }
  ret = total;
  print_strace("[runtime] Simulated writev = %li\r\n", ret);
  return ret;
}

uintptr_t
io_syscall_readv(int fd, const struct iovec* iov, int iovcnt) {
  int i         = 0;
  uintptr_t ret = 0;
  size_t total  = 0;
  print_strace(
      "[runtime] Simulating readv (cnt %i) with read calls\r\n", iovcnt);
  for (i = 0; i < iovcnt && ret >= 0; i++) {
    struct iovec iov_local;
    copy_from_user(&iov_local, &(iov[i]), sizeof(struct iovec));
    ret = io_syscall_read(fd, iov_local.iov_base, iov_local.iov_len);
    total += ret;
  }

  ret = total;
  print_strace("[runtime] Simulated readv = %li\r\n", ret);
  return ret;
}

uintptr_t
io_syscall_fstatat(int dirfd, char* pathname, struct stat* statbuf, int flags) {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_fstatat* args = (sargs_SYS_fstatat*)edge_syscall->data;
  uintptr_t ret           = -1;

  edge_syscall->syscall_num = SYS_fstatat;
  args->dirfd               = dirfd;
  args->flags               = flags;

  size_t pathlen;
  ALLOW_USER_ACCESS(pathlen = _strlen(pathname) + 1);
  // Sanity check that the buffer will fit in the shared memory
  if (edge_call_check_ptr_valid((uintptr_t)args->pathname, pathlen) != 0) {
    goto done;
  }
  copy_from_user(args->pathname, pathname, pathlen);

  size_t totalsize =
      (sizeof(struct edge_syscall) + sizeof(sargs_SYS_fstatat) + pathlen);

  ret = dispatch_edgecall_syscall(edge_syscall, totalsize);

  if (ret == 0) {
    copy_to_user(statbuf, &args->stats, sizeof(struct stat));
  }

done:
  print_strace(
      "[runtime] proxied fstatat (path %.*s) = %li\r\n",
      pathlen > MAX_STRACE_PRINT ? MAX_STRACE_PRINT : pathlen, args->pathname,
      ret);
  return ret;
}

uintptr_t
io_syscall_pipe(int* fds) {
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  edge_syscall->syscall_num = SYS_pipe2;

  int* args = (int*)edge_syscall->data;

  size_t totalsize = sizeof(struct edge_syscall);

  ret = dispatch_edgecall_syscall(edge_syscall, totalsize);

  if (ret == 0) {
    copy_to_user(fds, args, 2 * sizeof(int));
  }

  print_strace("[runtime] proxied pipe \r\n");
  return ret;
}

uintptr_t
io_syscall_epoll_create(int size) {
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  edge_syscall->syscall_num = SYS_epoll_create1;

  sargs_SYS_epoll_create1* args = (sargs_SYS_epoll_create1*)edge_syscall->data;

  // Since Linux 2.6.8, the size argument is ignored, but must be greater than
  // zero. See https://man7.org/linux/man-pages/man2/epoll_create.2.html
  args->size = 1024;

  size_t totalsize =
      sizeof(struct edge_syscall) + sizeof(sargs_SYS_epoll_create1);
  ret = dispatch_edgecall_syscall(edge_syscall, totalsize);

  print_strace("[runtime] proxied epoll_create: %d \r\n", ret);
  return ret;
}

uintptr_t
io_syscall_epoll_ctl(int epfd, int op, int fd, uintptr_t event) {
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  edge_syscall->syscall_num = SYS_epoll_ctl;

  sargs_SYS_epoll_ctl* args = (sargs_SYS_epoll_ctl*)edge_syscall->data;

  args->epfd = epfd;
  args->op   = op;
  args->fd   = fd;

  copy_from_user(&args->event, (void*)event, sizeof(struct epoll_event));

  size_t totalsize = sizeof(struct edge_syscall) + sizeof(sargs_SYS_epoll_ctl);
  ret              = dispatch_edgecall_syscall(edge_syscall, totalsize);

  print_strace("[runtime] proxied epoll_create: %d \r\n", ret);
  return ret;
}

uintptr_t
io_syscall_fcntl(int fd, int cmd, uintptr_t arg) {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_fcntl* args = (sargs_SYS_fcntl*)edge_syscall->data;
  uintptr_t ret         = -1;

  edge_syscall->syscall_num = SYS_fcntl;
  args->fd                  = fd;
  args->cmd                 = cmd;

  size_t totalsize;
  if (cmd == F_SETLK || cmd == F_SETLKW || cmd == F_GETLK) {
    print_strace("F_SETLK, FSETLKW, or FGETLK");
    if (edge_call_check_ptr_valid((uintptr_t)args->arg, sizeof(struct flock)) !=
        0) {
      print_strace("Ptr not valid");
      goto done;
    }
    copy_from_user(
        (struct flock*)args->arg, (struct flock*)arg, sizeof(struct flock));
    args->has_struct = 1;

    totalsize =
        (sizeof(struct edge_syscall) + sizeof(sargs_SYS_fcntl) +
         sizeof(struct flock));
  } else {
    args->arg[0]     = arg;
    args->has_struct = 0;
    totalsize        = (sizeof(struct edge_syscall) + sizeof(sargs_SYS_fcntl));
  }

  ret = dispatch_edgecall_syscall(edge_syscall, totalsize);

done:
  print_strace("[runtime] proxied fcntl = %li\r\n", ret);
  return ret;
}

uintptr_t
io_syscall_getcwd(char* buf, size_t size) {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_getcwd* args = (sargs_SYS_getcwd*)edge_syscall->data;
  // char* syscall_ret = NULL;

  edge_syscall->syscall_num = SYS_getcwd;

  args->size = size;

  size_t totalsize = (sizeof(struct edge_syscall) + sizeof(sargs_SYS_getcwd));

  dispatch_edgecall_syscall(edge_syscall, totalsize);

  copy_to_user(buf, &args->buf, size);
  print_strace("[runtime] proxied getcwd\r\n");
  return (uintptr_t)buf;
}

uintptr_t
io_syscall_chdir(char* path) {
  path          = "./";
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_chdir* args = (sargs_SYS_chdir*)edge_syscall->data;
  // char* syscall_ret = NULL;

  edge_syscall->syscall_num = SYS_chdir;

  copy_from_user(args->path, path, strlen(path) + 1);

  size_t totalsize = (sizeof(struct edge_syscall)) + strlen(args->path) + 1;
  ret              = dispatch_edgecall_syscall(edge_syscall, totalsize);

  print_strace("[runtime] proxied chdir: %s\r\n", args->path);
  return ret;
}

uintptr_t
io_syscall_epoll_pwait(int epfd, uintptr_t events, int maxevents, int timeout) {
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_epoll_pwait* args = (sargs_SYS_epoll_pwait*)edge_syscall->data;

  edge_syscall->syscall_num = SYS_epoll_pwait;

  args->epfd      = epfd;
  args->maxevents = maxevents;
  args->timeout   = timeout;

  copy_from_user(&args->events, (void*)events, sizeof(struct epoll_event));

  size_t totalsize =
      (sizeof(struct edge_syscall)) + sizeof(sargs_SYS_epoll_pwait);
  ret = dispatch_edgecall_syscall(edge_syscall, totalsize);

  copy_to_user((void*)events, &args->events, sizeof(struct epoll_event));
  print_strace(
      "[runtime] proxied epoll_pwait: epfd: %d, ret: %d\r\n", args->epfd, ret);
  return ret;
}

uintptr_t
io_syscall_renameat2(
    int olddirfd, uintptr_t oldpath, int newdirfd, uintptr_t newpath,
    unsigned int flags) {
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_renameat2* args = (sargs_SYS_renameat2*)edge_syscall->data;

  edge_syscall->syscall_num = SYS_renameat2;

  args->olddirfd = olddirfd;
  args->newdirfd = newdirfd;
  args->flags    = flags;

  copy_from_user(&args->oldpath, (void*)oldpath, 128);
  copy_from_user(&args->newpath, (void*)newpath, 128);

  size_t totalsize =
      (sizeof(struct edge_syscall)) + sizeof(sargs_SYS_renameat2);
  ret = dispatch_edgecall_syscall(edge_syscall, totalsize);

  print_strace(
      "[runtime] proxied renameat2: oldpath: %s, newpath: %s, ret: %d\r\n",
      args->oldpath, args->newpath, ret);
  return ret;
}

uintptr_t
io_syscall_umask(int mask) {
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_umask* args = (sargs_SYS_umask*)edge_syscall->data;

  edge_syscall->syscall_num = SYS_umask;
  args->mask                = mask;

  size_t totalsize = (sizeof(struct edge_syscall)) + sizeof(sargs_SYS_umask);
  ret              = dispatch_edgecall_syscall(edge_syscall, totalsize);

  print_strace(
      "[runtime] proxied umask: mask: %d, ret: %d\r\n", args->mask, ret);
  return ret;
}

uintptr_t
io_syscall_fstat(int fd, struct stat* statbuf) {
  struct edge_syscall* edge_syscall =
      (struct edge_syscall*)edge_call_data_ptr();
  sargs_SYS_fstat* args = (sargs_SYS_fstat*)edge_syscall->data;
  uintptr_t ret         = -1;

  edge_syscall->syscall_num = SYS_fstat;
  args->fd                  = fd;

  size_t totalsize = (sizeof(struct edge_syscall) + sizeof(sargs_SYS_fstat));

  ret = dispatch_edgecall_syscall(edge_syscall, totalsize);

  if (ret == 0) {
    copy_to_user(statbuf, &args->stats, sizeof(struct stat));
  }

  print_strace("[runtime] proxied fstat = %li\r\n", ret);
  return ret;
}

#else  /* USE_EDGE_PROTECTION */

uintptr_t
io_syscall_sync() {
  struct edge_syscall* edge_syscall;
  uintptr_t ret;

  edge_syscall =
      (struct edge_syscall*)setup_adm_syscall_metadata(SYS_sync, 0, 0);
  if (!edge_syscall) return -1;

  ret = dispatch_edgecall_syscall_protected(
      edge_syscall, sizeof(struct edge_syscall), false);
  print_strace("[runtime] proxied sync\r\n");
  return ret;
}

uintptr_t
io_syscall_ftruncate(int fd, off_t offset) {
  struct edge_syscall* edge_syscall;
  sargs_SYS_ftruncate* args;
  size_t data_len;
  uintptr_t ret;

  edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
      SYS_ftruncate, sizeof(sargs_SYS_ftruncate), 0);
  if (!edge_syscall) return -1;

  args         = (sargs_SYS_ftruncate*)edge_syscall->data;
  args->fd     = fd;
  args->offset = offset;

  data_len = sizeof(struct edge_syscall) + sizeof(struct sargs_SYS_ftruncate);
  ret      = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);
  print_strace("[runtime] proxied ftruncate (%i) = %li\r\n", fd, ret);
  return ret;
}

uintptr_t
io_syscall_fsync(int fd) {
  struct edge_syscall* edge_syscall;
  sargs_SYS_fsync* args;
  size_t data_len;
  uintptr_t ret;

  edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
      SYS_fsync, sizeof(sargs_SYS_fsync), 0);
  if (!edge_syscall) return -1;

  args     = (sargs_SYS_fsync*)edge_syscall->data;
  args->fd = fd;

  data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_fsync);
  ret      = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);
  print_strace("[runtime] proxied fsync (%i) = %li\r\n", fd, ret);
  return ret;
}

uintptr_t
io_syscall_lseek(int fd, off_t offset, int whence) {
  struct edge_syscall* edge_syscall;
  sargs_SYS_lseek* args;
  size_t data_len;
  uintptr_t ret;

  edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
      SYS_lseek, sizeof(sargs_SYS_lseek), 0);
  if (!edge_syscall) return -1;

  args         = (sargs_SYS_lseek*)edge_syscall->data;
  args->fd     = fd;
  args->offset = offset;
  args->whence = whence;

  data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_lseek);
  ret      = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);
  print_strace(
      "[runtime] proxied lseek (on fd:%i to %li from %i) = %li\r\n", fd, offset,
      whence, ret);
  return ret;
}

uintptr_t
io_syscall_close(int fd) {
  struct edge_syscall* edge_syscall;
  sargs_SYS_close* args;
  size_t data_len;
  uintptr_t ret;

  edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
      SYS_close, sizeof(sargs_SYS_close), 0);
  if (!edge_syscall) return -1;

  args     = (sargs_SYS_close*)edge_syscall->data;
  args->fd = fd;

  data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_close);
  ret      = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);
  print_strace("[runtime] proxied close (%i) = %li\r\n", fd, ret);
  return ret;
}

uintptr_t
io_syscall_read(int fd, void* buf, size_t len) {
  struct edge_syscall* edge_syscall;
  size_t data_len;
  uintptr_t ret = -1;

  uintptr_t buf_offset = __adm_offset((uintptr_t)buf);
  if (buf_offset) {  // no copy
    sargs_SYS_read_p* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_read | SYSCALL_ADMPTR_BUFFER, sizeof(sargs_SYS_read_p), 0);
    if (!edge_syscall) return -1;

    args      = (sargs_SYS_read_p*)edge_syscall->data;
    args->fd  = fd;
    args->len = len;
    args->buf = (AdmPtr){buf_offset};

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_read_p);
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);
  } else {  // with copy
    sargs_SYS_read* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_read, sizeof(sargs_SYS_read), len);
    if (!edge_syscall) return -1;

    args      = (sargs_SYS_read*)edge_syscall->data;
    args->fd  = fd;
    args->len = len;

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_read) + len;
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);
    if ((int)ret < 0) goto done;
    copy_to_user(buf, args->buf, ret > len ? len : ret);
  }

done:
  print_strace(
      "[runtime] proxied read from %i (size: %lu) = %li\r\n", fd, len, ret);
  return ret;
}

uintptr_t
io_syscall_write(int fd, void* buf, size_t len) {
  struct edge_syscall* edge_syscall;
  size_t data_len;
  uintptr_t ret = -1;

  uintptr_t buf_offset = __adm_offset((uintptr_t)buf);
  if (buf_offset) {
    sargs_SYS_write_p* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_write | SYSCALL_ADMPTR_BUFFER, sizeof(sargs_SYS_write_p), 0);
    if (!edge_syscall) goto done;

    args      = (sargs_SYS_write_p*)edge_syscall->data;
    args->fd  = fd;
    args->len = len;
    args->buf = (AdmPtr){buf_offset};

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_write_p);
  } else {
    sargs_SYS_write* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_write, sizeof(sargs_SYS_write), len);
    if (!edge_syscall) goto done;

    args      = (sargs_SYS_write*)edge_syscall->data;
    args->fd  = fd;
    args->len = len;

    copy_from_user(args->buf, buf, len);

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_write) + len;
  }

  ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);

done:
  print_strace(
      "[runtime] proxied write to %i (size: %lu) = %li\r\n", fd, len, ret);
  return ret;
}

uintptr_t
io_syscall_openat(int dirfd, char* path, int flags, mode_t mode) {
  struct edge_syscall* edge_syscall;
  size_t data_len;
  uintptr_t ret = -1;

  uintptr_t buf_offset = __adm_offset((uintptr_t)path);
  size_t pathlen;
  ALLOW_USER_ACCESS(pathlen = _strlen(path) + 1);
  if (buf_offset) {
    sargs_SYS_openat_p* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_openat | SYSCALL_ADMPTR_BUFFER, sizeof(sargs_SYS_openat_p),
        pathlen);
    if (!edge_syscall) goto done;

    args        = (sargs_SYS_openat_p*)edge_syscall->data;
    args->dirfd = dirfd;
    args->path  = (AdmPtr){buf_offset};
    args->flags = flags;
    args->mode  = mode;

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_openat);
  } else {
    sargs_SYS_openat* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_openat, sizeof(sargs_SYS_openat), pathlen);
    if (!edge_syscall) goto done;

    args        = (sargs_SYS_openat*)edge_syscall->data;
    args->dirfd = dirfd;
    args->flags = flags;
    args->mode  = mode;

    copy_from_user(args->path, path, pathlen);

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_openat) + pathlen;
  }

  ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);

done:
  // TODO path print here isn't necessarily correct or even copied!
  print_strace("[runtime] proxied openat = %li\r\n", ret);

  return ret;
}

uintptr_t
io_syscall_unlinkat(int dirfd, char* path, int flags) {
  struct edge_syscall* edge_syscall;
  size_t data_len;
  uintptr_t ret = -1;

  uintptr_t buf_offset = __adm_offset((uintptr_t)path);
  size_t pathlen;
  ALLOW_USER_ACCESS(pathlen = _strlen(path) + 1);
  if (buf_offset) {
    sargs_SYS_unlinkat_p* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_unlinkat | SYSCALL_ADMPTR_BUFFER, sizeof(sargs_SYS_unlinkat_p), 0);
    if (!edge_syscall) goto done;

    args        = (sargs_SYS_unlinkat_p*)edge_syscall->data;
    args->dirfd = dirfd;
    args->path  = (AdmPtr){buf_offset};
    args->flags = flags;

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_unlinkat_p);
  } else {
    sargs_SYS_unlinkat* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_unlinkat, sizeof(sargs_SYS_unlinkat), pathlen);
    if (!edge_syscall) goto done;

    args        = (sargs_SYS_unlinkat*)edge_syscall->data;
    args->dirfd = dirfd;
    args->flags = flags;

    copy_from_user(args->path, path, pathlen);

    data_len =
        sizeof(struct edge_syscall) + sizeof(sargs_SYS_unlinkat) + pathlen;
  }

  ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);

done:
  // TODO path print here isn't necessarily correct or even copied!
  print_strace("[runtime] proxied unlinkat = %li\r\n", ret);
  return ret;
}

uintptr_t
io_syscall_writev(int fd, const struct iovec* iov, int iovcnt) {
  int i         = 0;
  uintptr_t ret = 0;
  size_t total  = 0;
  print_strace(
      "[runtime] Simulating writev (cnt %i) with write calls\r\n", iovcnt);
  uintptr_t offset = __adm_offset((uintptr_t)iov);
  if (offset) {
    struct iovec* iov_adm = (struct iovec*)adm2ptr(&(AdmPtr){offset});
    for (i = 0; i < iovcnt && ret >= 0; i++) {
      ret = io_syscall_write(fd, iov_adm[i].iov_base, iov_adm[i].iov_len);
      total += ret;
    }
  } else {
    for (i = 0; i < iovcnt && ret >= 0; i++) {
      struct iovec iov_local;
      copy_from_user(&iov_local, &(iov[i]), sizeof(struct iovec));
      ret = io_syscall_write(fd, iov_local.iov_base, iov_local.iov_len);
      total += ret;
    }
  }
  ret = total;
  print_strace("[runtime] Simulated writev = %li\r\n", ret);
  return ret;
}

uintptr_t
io_syscall_readv(int fd, const struct iovec* iov, int iovcnt) {
  int i         = 0;
  uintptr_t ret = 0;
  size_t total  = 0;
  print_strace(
      "[runtime] Simulating readv (cnt %i) with read calls\r\n", iovcnt);
  uintptr_t offset = __adm_offset((uintptr_t)iov);
  if (offset) {
    struct iovec* iov_adm = (struct iovec*)adm2ptr(&(AdmPtr){offset});
    for (i = 0; i < iovcnt && ret >= 0; i++) {
      ret = io_syscall_read(fd, iov_adm[i].iov_base, iov_adm[i].iov_len);
      total += ret;
    }
  } else {
    for (i = 0; i < iovcnt && ret >= 0; i++) {
      struct iovec iov_local;
      copy_from_user(&iov_local, &(iov[i]), sizeof(struct iovec));
      ret = io_syscall_read(fd, iov_local.iov_base, iov_local.iov_len);
      total += ret;
    }
  }
  ret = total;
  print_strace("[runtime] Simulated readv = %li\r\n", ret);
  return ret;
}

uintptr_t
io_syscall_fstatat(int dirfd, char* pathname, struct stat* statbuf, int flags) {
  struct edge_syscall* edge_syscall;
  size_t data_len;
  uintptr_t ret = -1;

  uintptr_t pathbuf = __adm_offset((uintptr_t)pathname);
  size_t pathlen;
  ALLOW_USER_ACCESS(pathlen = _strlen(pathname) + 1);
  if (pathbuf) {
    sargs_SYS_fstatat_p* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_fstatat | SYSCALL_ADMPTR_BUFFER, sizeof(sargs_SYS_fstatat_p), 0);
    if (!edge_syscall) goto done;

    args           = (sargs_SYS_fstatat_p*)edge_syscall->data;
    args->dirfd    = dirfd;
    args->flags    = flags;
    args->pathname = (AdmPtr){pathbuf};

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_fstatat_p);
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);
    if (ret == 0) {
      copy_to_user(statbuf, &args->stats, sizeof(struct stat));
    }
  } else {
    sargs_SYS_fstatat* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_fstatat, sizeof(sargs_SYS_fstatat), pathlen);
    if (!edge_syscall) goto done;

    args        = (sargs_SYS_fstatat*)edge_syscall->data;
    args->dirfd = dirfd;
    args->flags = flags;

    copy_from_user(args->pathname, pathname, pathlen);

    data_len =
        sizeof(struct edge_syscall) + sizeof(sargs_SYS_fstatat) + pathlen;
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);
    if (ret == 0) {
      copy_to_user(statbuf, &args->stats, sizeof(struct stat));
    }
  }

done:
  print_strace("[runtime] proxied fstatat = %li\r\n", ret);
  return ret;
}

uintptr_t
io_syscall_pipe(int* fds) {  // int fds[2]
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall;
  size_t data_len;

  edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
      SYS_pipe2, sizeof(int) * 2, 0);
  if (!edge_syscall) goto done;

  data_len = sizeof(struct edge_syscall) + sizeof(int) * 2;
  ret      = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);
  if (ret == 0) {
    int* adm_fds = (int*)edge_syscall->data;
    copy_to_user(fds, adm_fds, 2 * sizeof(int));
  }

done:
  print_strace("[runtime] proxied pipe \r\n");
  return ret;
}

uintptr_t
io_syscall_epoll_create(int size) {
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall;
  sargs_SYS_epoll_create1* args;
  size_t data_len;

  edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
      SYS_epoll_create1, sizeof(sargs_SYS_epoll_create1), 0);
  if (!edge_syscall) goto done;

  args = (sargs_SYS_epoll_create1*)edge_syscall->data;
  // Since Linux 2.6.8, the size argument is ignored, but must be greater than
  // zero. See https://man7.org/linux/man-pages/man2/epoll_create.2.html
  args->size = 1024;

  data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_epoll_create1);
  ret      = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);

done:
  print_strace("[runtime] proxied epoll_create: %d \r\n", ret);
  return ret;
}

uintptr_t
io_syscall_epoll_ctl(int epfd, int op, int fd, uintptr_t event) {
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall;
  size_t data_len;

  uintptr_t offset = __adm_offset(event);
  if (event) {
    sargs_SYS_epoll_ctl_p* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_epoll_ctl, sizeof(sargs_SYS_epoll_ctl_p), 0);
    if (!edge_syscall) goto done;

    args        = (sargs_SYS_epoll_ctl_p*)edge_syscall->data;
    args->epfd  = epfd;
    args->op    = op;
    args->fd    = fd;
    args->event = (AdmPtr){offset};

    data_len =
        sizeof(struct edge_syscall) + sizeof(struct sargs_SYS_epoll_ctl_p);
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);
  } else {
    sargs_SYS_epoll_ctl* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_epoll_ctl, sizeof(sargs_SYS_epoll_ctl), 0);
    if (!edge_syscall) goto done;

    args       = (sargs_SYS_epoll_ctl*)edge_syscall->data;
    args->epfd = epfd;
    args->op   = op;
    args->fd   = fd;

    copy_from_user(&args->event, (void*)event, sizeof(struct epoll_event));

    data_len = sizeof(struct edge_syscall) + sizeof(struct sargs_SYS_epoll_ctl);
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);
  }

done:
  print_strace("[runtime] proxied epoll_create: %d \r\n", ret);
  return ret;
}

uintptr_t
io_syscall_fcntl(int fd, int cmd, uintptr_t arg) {
  struct edge_syscall* edge_syscall;
  uintptr_t ret = -1;
  size_t data_len;

  if (cmd == F_SETLK || cmd == F_SETLKW || cmd == F_GETLK) {
    print_strace("F_SETLK, FSETLKW, or FGETLK");
    uintptr_t struct_offset = __adm_offset(arg);
    if (struct_offset) {
      sargs_SYS_fcntl_p* args;
      edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
          SYS_fcntl | SYSCALL_ADMPTR_BUFFER, sizeof(sargs_SYS_fcntl_p), 0);
      if (!edge_syscall) goto done;

      args             = (sargs_SYS_fcntl_p*)edge_syscall->data;
      args->has_struct = 1;
      args->fd         = fd;
      args->cmd        = cmd;
      args->arg        = (AdmPtr){struct_offset};

      data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_fcntl_p);
    } else {
      sargs_SYS_fcntl* args;
      edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
          SYS_fcntl, sizeof(sargs_SYS_fcntl), sizeof(struct flock));
      if (!edge_syscall) goto done;

      args             = (sargs_SYS_fcntl*)edge_syscall->data;
      args->has_struct = 1;
      args->fd         = fd;
      args->cmd        = cmd;
      copy_from_user(args->arg, (void*)arg, sizeof(struct flock));

      data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_fcntl) +
                 sizeof(struct flock);
    }
  } else {
    sargs_SYS_fcntl* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_fcntl, sizeof(sargs_SYS_fcntl), sizeof(uintptr_t));
    if (!edge_syscall) goto done;

    args                   = (sargs_SYS_fcntl*)edge_syscall->data;
    args->has_struct       = 0;
    args->fd               = fd;
    args->cmd              = cmd;
    *(uintptr_t*)args->arg = arg;

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_fcntl) +
               sizeof(uintptr_t);
  }

  ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);

done:
  print_strace("[runtime] proxied fcntl = %li\r\n", ret);
  return ret;
}

uintptr_t
io_syscall_getcwd(char* buf, size_t size) {
  struct edge_syscall* edge_syscall;
  size_t data_len;

  uintptr_t offset = __adm_offset((uintptr_t)buf);
  if (offset) {
    sargs_SYS_getcwd_p* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_getcwd | SYSCALL_ADMPTR_BUFFER, sizeof(sargs_SYS_getcwd_p), 0);
    if (!edge_syscall) return (uintptr_t)NULL;

    args       = (sargs_SYS_getcwd_p*)edge_syscall->data;
    args->size = size;
    args->buf  = (AdmPtr){offset};

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_getcwd_p);
    dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);
  } else {
    sargs_SYS_getcwd* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_getcwd, sizeof(sargs_SYS_getcwd), size);
    if (!edge_syscall) return (uintptr_t)NULL;

    args       = (sargs_SYS_getcwd*)edge_syscall->data;
    args->size = size;

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_getcwd) + size;
    dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);
    copy_to_user(buf, args->buf, size);
  }

  print_strace("[runtime] proxied getcwd\r\n");
  return (uintptr_t)buf;
}

uintptr_t
io_syscall_chdir(char* path) {
  path          = "./";
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall;
  size_t data_len;

  uintptr_t offset = __adm_offset((uintptr_t)path);
  size_t pathlen;
  ALLOW_USER_ACCESS(pathlen = _strlen(path) + 1);

  if (offset) {
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_chdir | SYSCALL_ADMPTR_BUFFER, sizeof(AdmPtr), 0);
    if (!edge_syscall) goto done;

    AdmPtr* pathptr = (AdmPtr*)edge_syscall->data;
    pathptr->offset = offset;
    data_len        = sizeof(struct edge_syscall) + sizeof(AdmPtr);
  } else {
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_chdir, sizeof(sargs_SYS_chdir), pathlen);
    if (!edge_syscall) goto done;

    copy_from_user(edge_syscall->data, path, pathlen);
    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_chdir) + pathlen;
  }

  ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);

done:
  print_strace("[runtime] proxied chdir: %s\r\n", (char*)edge_syscall->data);
  return ret;
}

uintptr_t
io_syscall_epoll_pwait(int epfd, uintptr_t events, int maxevents, int timeout) {
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall;
  size_t data_len;

  uintptr_t offset = __adm_offset(events);
  if (offset) {
    sargs_SYS_epoll_pwait_p* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_epoll_pwait | SYSCALL_ADMPTR_BUFFER,
        sizeof(sargs_SYS_epoll_pwait_p), 0);
    if (!edge_syscall) goto done;

    args            = (sargs_SYS_epoll_pwait_p*)edge_syscall->data;
    args->epfd      = epfd;
    args->events    = (AdmPtr){offset};
    args->maxevents = maxevents;
    args->timeout   = timeout;

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_epoll_pwait_p);
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);
  } else {
    sargs_SYS_epoll_pwait* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_epoll_pwait, sizeof(sargs_SYS_epoll_pwait), 0);
    if (!edge_syscall) goto done;

    args            = (sargs_SYS_epoll_pwait*)edge_syscall->data;
    args->epfd      = epfd;
    args->maxevents = maxevents;
    args->timeout   = timeout;
    copy_from_user(&args->events, (void*)events, sizeof(struct epoll_event));

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_epoll_pwait);
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);

    copy_from_user((void*)events, &args->events, sizeof(struct epoll_event));
  }

done:
  print_strace(
      "[runtime] proxied epoll_pwait: epfd: %d, ret: %d\r\n", epfd, ret);
  return ret;
}

uintptr_t
io_syscall_renameat2(
    int olddirfd, uintptr_t oldpath, int newdirfd, uintptr_t newpath,
    unsigned int flags) {
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall;
  size_t data_len;

  uintptr_t oldpath_offset = __adm_offset(oldpath);
  uintptr_t newpath_offset = __adm_offset(newpath);

  if (oldpath_offset && newpath_offset) {
    sargs_SYS_renameat2_p* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_renameat2 | SYSCALL_ADMPTR_BUFFER, sizeof(sargs_SYS_renameat2_p),
        0);
    if (!edge_syscall) goto done;

    args           = (sargs_SYS_renameat2_p*)edge_syscall->data;
    args->olddirfd = olddirfd;
    args->oldpath  = (AdmPtr){oldpath_offset};
    args->newdirfd = newdirfd;
    args->newpath  = (AdmPtr){newpath_offset};
    args->flags    = flags;

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_renameat2_p);
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);
  } else if (!oldpath_offset && !newpath_offset) {
    sargs_SYS_renameat2* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_renameat2, sizeof(sargs_SYS_renameat2), 0);
    if (!edge_syscall) goto done;

    args           = (sargs_SYS_renameat2*)edge_syscall->data;
    args->olddirfd = olddirfd;
    args->newdirfd = newdirfd;
    args->flags    = flags;

    copy_from_user(args->oldpath, (void*)oldpath, 128);
    copy_from_user(args->newpath, (void*)newpath, 128);

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_renameat2);
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);
  } else {
    // invalid
    // oldpath and newpath must be located in same memory
    goto done;
  }

done:
  print_strace("[runtime] proxied renameat2: ret: %d\r\n", ret);
  return ret;
}

uintptr_t
io_syscall_umask(int mask) {
  uintptr_t ret = -1;
  struct edge_syscall* edge_syscall;
  sargs_SYS_umask* args;
  size_t data_len;

  edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
      SYS_umask, sizeof(sargs_SYS_umask), 0);
  if (!edge_syscall) return (uintptr_t)-1;

  args       = (sargs_SYS_umask*)edge_syscall->data;
  args->mask = mask;

  data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_umask);
  ret      = dispatch_edgecall_syscall_protected(edge_syscall, data_len, false);

  print_strace(
      "[runtime] proxied umask: mask: %d, ret: %d\r\n", args->mask, ret);
  return ret;
}

uintptr_t
io_syscall_fstat(int fd, struct stat* statbuf) {
  struct edge_syscall* edge_syscall;
  uintptr_t ret = -1;
  size_t data_len;

  uintptr_t offset = __adm_offset((uintptr_t)statbuf);
  if (offset) {
    sargs_SYS_fstat_p* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_fstat | SYSCALL_ADMPTR_BUFFER, sizeof(sargs_SYS_fstat_p), 0);
    if (!edge_syscall) goto done;

    args        = (sargs_SYS_fstat_p*)edge_syscall->data;
    args->fd    = fd;
    args->stats = (AdmPtr){offset};

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_fstat_p);
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);
  } else {
    sargs_SYS_fstat* args;
    edge_syscall = (struct edge_syscall*)setup_adm_syscall_metadata(
        SYS_fstat, sizeof(sargs_SYS_fstat), 0);
    if (!edge_syscall) goto done;

    args     = (sargs_SYS_fstat*)edge_syscall->data;
    args->fd = fd;

    data_len = sizeof(struct edge_syscall) + sizeof(sargs_SYS_fstat_p);
    ret = dispatch_edgecall_syscall_protected(edge_syscall, data_len, true);

    if (ret == 0) copy_to_user(statbuf, &args->stats, sizeof(struct stat));
  }

done:
  print_strace("[runtime] proxied fstat = %li\r\n", ret);
  return ret;
}
#endif /* USE_EDGE_PROTECTION */

#endif /* USE_IO_SYSCALL */
