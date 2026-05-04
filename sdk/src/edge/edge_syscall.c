#include "edge_syscall.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>

#include "edge/adm.h"
// Special edge-call handler for syscall proxying
// normal syscall delegation w/ shared memory
void
incoming_syscall(struct edge_call* edge_call) {
  struct edge_syscall* syscall_info;

  size_t args_size;

  if (edge_call_args_ptr(edge_call, (uintptr_t*)&syscall_info, &args_size) != 0)
    goto syscall_error;

  // NOTE: Right now we assume that the args data is safe, even though
  // it may be changing under us. This should be safer in the future.

  edge_call->return_data.call_status = CALL_STATUS_OK;

  int64_t ret;
  int is_str_ret = 0;
  char* retbuf;

  // Right now we only handle some io syscalls. See runtime for how
  // others are handled.
  switch (syscall_info->syscall_num) {
    case (SYS_openat):;
      sargs_SYS_openat* openat_args = (sargs_SYS_openat*)syscall_info->data;
      ret                           = openat(
          openat_args->dirfd, openat_args->path, openat_args->flags,
          openat_args->mode);
      break;
    case (SYS_unlinkat):;
      sargs_SYS_unlinkat* unlinkat_args =
          (sargs_SYS_unlinkat*)syscall_info->data;
      ret = unlinkat(
          unlinkat_args->dirfd, unlinkat_args->path, unlinkat_args->flags);
      break;
    case (SYS_ftruncate):;
      sargs_SYS_ftruncate* ftruncate_args =
          (sargs_SYS_ftruncate*)syscall_info->data;
      ret = ftruncate(ftruncate_args->fd, ftruncate_args->offset);
      break;
    case (SYS_fstatat):;
      sargs_SYS_fstatat* fstatat_args = (sargs_SYS_fstatat*)syscall_info->data;
      // Note the use of the implicit buffer in the stat args object (stats)
      ret = fstatat(
          fstatat_args->dirfd, fstatat_args->pathname, &fstatat_args->stats,
          fstatat_args->flags);
      break;
    case (SYS_fstat):;
      sargs_SYS_fstat* fstat_args = (sargs_SYS_fstat*)syscall_info->data;
      // Note the use of the implicit buffer in the stat args object (stats)
      ret = fstat(fstat_args->fd, &fstat_args->stats);
      break;
    case (SYS_getcwd):;  // TODO: how to handle string return
      sargs_SYS_getcwd* getcwd_args = (sargs_SYS_getcwd*)syscall_info->data;
      retbuf     = getcwd(getcwd_args->buf, getcwd_args->size);
      is_str_ret = 1;
      break;
    case (SYS_write):;
      sargs_SYS_write* write_args = (sargs_SYS_write*)syscall_info->data;
      ret = write(write_args->fd, write_args->buf, write_args->len);
      break;
    case (SYS_read):;
      sargs_SYS_read* read_args = (sargs_SYS_read*)syscall_info->data;
      ret = read(read_args->fd, read_args->buf, read_args->len);
      break;
    case (SYS_sync):;
      sync();
      ret = 0;
      break;
    case (SYS_fsync):;
      sargs_SYS_fsync* fsync_args = (sargs_SYS_fsync*)syscall_info->data;
      ret                         = fsync(fsync_args->fd);
      break;
    case (SYS_close):;
      sargs_SYS_close* close_args = (sargs_SYS_close*)syscall_info->data;
      ret                         = close(close_args->fd);
      break;
    case (SYS_lseek):;
      sargs_SYS_lseek* lseek_args = (sargs_SYS_lseek*)syscall_info->data;
      ret = lseek(lseek_args->fd, lseek_args->offset, lseek_args->whence);
      break;
    case (SYS_pipe2):;
      int* fds = (int*)syscall_info->data;
      ret      = pipe(fds);
      break;
    case (SYS_epoll_create1):;
      sargs_SYS_epoll_create1* epoll_args =
          (sargs_SYS_epoll_create1*)syscall_info->data;
      ret = epoll_create(epoll_args->size);
      break;
    case (SYS_chdir):;
      sargs_SYS_chdir* chdir_args = (sargs_SYS_chdir*)syscall_info->data;
      ret                         = chdir(chdir_args->path);
      break;
    case (SYS_epoll_ctl):;
      sargs_SYS_epoll_ctl* epoll_ctl_args =
          (sargs_SYS_epoll_ctl*)syscall_info->data;
      ret = epoll_ctl(
          epoll_ctl_args->epfd, epoll_ctl_args->op, epoll_ctl_args->fd,
          (struct epoll_event*)&epoll_ctl_args->event);
      break;
    case (SYS_epoll_pwait):;
      sargs_SYS_epoll_pwait* epoll_pwait_args =
          (sargs_SYS_epoll_pwait*)syscall_info->data;
      ret = epoll_wait(
          epoll_pwait_args->epfd, &epoll_pwait_args->events,
          epoll_pwait_args->maxevents, epoll_pwait_args->timeout);
      break;
    case (SYS_getpeername):;
      sargs_SYS_getpeername* getpeername_args =
          (sargs_SYS_getpeername*)syscall_info->data;
      ret = getpeername(
          getpeername_args->sockfd, (struct sockaddr*)&getpeername_args->addr,
          &getpeername_args->addrlen);
      break;
    case (SYS_getsockname):;
      sargs_SYS_getsockname* getsockname_args =
          (sargs_SYS_getsockname*)syscall_info->data;
      ret = getsockname(
          getsockname_args->sockfd, (struct sockaddr*)&getsockname_args->addr,
          &getsockname_args->addrlen);
      break;
    case (SYS_renameat2):;
      sargs_SYS_renameat2* renameat_args =
          (sargs_SYS_renameat2*)syscall_info->data;
      ret = renameat(
          renameat_args->olddirfd, renameat_args->oldpath,
          renameat_args->newdirfd, renameat_args->newpath);
      break;
    case (SYS_umask):;
      sargs_SYS_umask* umask_args = (sargs_SYS_umask*)syscall_info->data;
      ret                         = umask(umask_args->mask);
      break;
    case (SYS_socket):;
      sargs_SYS_socket* socket_args = (sargs_SYS_socket*)syscall_info->data;
      ret =
          socket(socket_args->domain, socket_args->type, socket_args->protocol);
      break;
    case (SYS_setsockopt):;
      sargs_SYS_setsockopt* setsockopt_args =
          (sargs_SYS_setsockopt*)syscall_info->data;
      ret = setsockopt(
          setsockopt_args->socket, setsockopt_args->level,
          setsockopt_args->option_name, &setsockopt_args->option_value,
          setsockopt_args->option_len);
      break;
    case (SYS_connect):;
      sargs_SYS_connect* connect_args = (sargs_SYS_connect*)syscall_info->data;
      ret                             = connect(
          connect_args->sockfd, (struct sockaddr*)&connect_args->addr,
          connect_args->addrlen);
      break;
    case (SYS_bind):;
      sargs_SYS_bind* bind_args = (sargs_SYS_bind*)syscall_info->data;
      ret                       = bind(
          bind_args->sockfd, (struct sockaddr*)&bind_args->addr,
          bind_args->addrlen);
      break;
    case (SYS_listen):;
      sargs_SYS_listen* listen_args = (sargs_SYS_listen*)syscall_info->data;
      ret = listen(listen_args->sockfd, listen_args->backlog);
      break;
    case (SYS_accept):;
      sargs_SYS_accept* accept_args = (sargs_SYS_accept*)syscall_info->data;
      ret                           = accept(
          accept_args->sockfd, (struct sockaddr*)&accept_args->addr,
          &accept_args->addrlen);
      break;
    case (SYS_recvfrom):;
      sargs_SYS_recvfrom* recvfrom_args =
          (sargs_SYS_recvfrom*)syscall_info->data;
      struct sockaddr* src_addr =
          recvfrom_args->src_addr_is_null ? NULL : &recvfrom_args->src_addr;
      socklen_t* addrlen =
          recvfrom_args->src_addr_is_null ? NULL : &recvfrom_args->addrlen;
      ret = recvfrom(
          recvfrom_args->sockfd, recvfrom_args->buf, recvfrom_args->len,
          recvfrom_args->flags, src_addr, addrlen);
      break;
    case (SYS_sendto):;
      sargs_SYS_sendto* sendto_args = (sargs_SYS_sendto*)syscall_info->data;
      struct sockaddr* dest_addr =
          sendto_args->dest_addr_is_null ? NULL : &sendto_args->dest_addr;
      socklen_t dest_addrlen =
          sendto_args->dest_addr_is_null ? 0 : sendto_args->addrlen;
      ret = sendto(
          sendto_args->sockfd, sendto_args->buf, sendto_args->len,
          sendto_args->flags, dest_addr, dest_addrlen);
      break;
    case (SYS_sendfile):;
      sargs_SYS_sendfile* sendfile_args =
          (sargs_SYS_sendfile*)syscall_info->data;
      ret = sendfile(
          sendfile_args->out_fd, sendfile_args->in_fd, &sendfile_args->offset,
          sendfile_args->count);
      break;
    case (SYS_fcntl):;
      sargs_SYS_fcntl* fcntl_args = (sargs_SYS_fcntl*)syscall_info->data;
      if (!fcntl_args->has_struct)
        ret = fcntl(fcntl_args->fd, fcntl_args->cmd, fcntl_args->arg[0]);
      else
        ret = fcntl(fcntl_args->fd, fcntl_args->cmd, fcntl_args->arg);
      break;
    case (SYS_getuid):;
      ret = getuid();
      break;
    case (SYS_pselect6):;
      sargs_SYS_pselect* pselect_args = (sargs_SYS_pselect*)syscall_info->data;
      fd_set* readfds =
          pselect_args->readfds_is_null ? NULL : &pselect_args->readfds;
      fd_set* writefds =
          pselect_args->writefds_is_null ? NULL : &pselect_args->writefds;
      fd_set* exceptfds =
          pselect_args->exceptfds_is_null ? NULL : &pselect_args->exceptfds;
      struct timespec* timeout =
          pselect_args->timeout_is_null ? NULL : &pselect_args->timeout;
      sigset_t* sigmask =
          pselect_args->sigmask_is_null ? NULL : &pselect_args->sigmask;
      ret = pselect(
          pselect_args->nfds, readfds, writefds, exceptfds, timeout, sigmask);
      break;
    default:
      goto syscall_error;
  }

  /* Setup return value */
  void* ret_data_ptr = (void*)edge_call_data_ptr();
  if (is_str_ret) {
    *(char**)ret_data_ptr = retbuf;  // TODO: check ptr stuff
    if (edge_call_setup_ret(edge_call, ret_data_ptr, sizeof(int64_t)) != 0)
      goto syscall_error;
  } else {
    *(int64_t*)ret_data_ptr = ret;
    if (edge_call_setup_ret(edge_call, ret_data_ptr, sizeof(int64_t)) != 0)
      goto syscall_error;
  }

  return;

syscall_error:
  edge_call->return_data.call_status = CALL_STATUS_SYSCALL_FAILED;
  return;
}

// protected syscall delegation w/ additional data memory
void
protected_incoming_syscall(struct edge_call* edge_call) {
  struct edge_syscall* syscall_info;
  size_t size;
  uintptr_t args;
  uintptr_t call_ret;

  int write_permitted = edge_call->allowed_adm_write;

  // retrive syscall info & args
  {
    int ret;
    ret =
        adm_get_region(ADM_UID_SYSCALL_META, (uintptr_t*)&syscall_info, &size);
    // ignore syscall_info->data
    if (ret || !syscall_info) {
      goto syscall_error;
    }
    args     = (uintptr_t)syscall_info->data;
    call_ret = (uintptr_t)((uintptr_t)syscall_info + size - sizeof(int64_t));
  }

  edge_call->return_data.call_status = CALL_STATUS_OK;

  int64_t ret;
  int is_str_ret = 0;
  char* retbuf;

  if ((0xf000 & syscall_info->syscall_num) !=
      0x0) {  // calls with direct ptr to adm
    size_t unmasked_num = 0x0fff & syscall_info->syscall_num;
    switch (unmasked_num) {
      case (SYS_openat):;
        sargs_SYS_openat_p* openat_args = (sargs_SYS_openat_p*)args;
        uintptr_t openpath              = adm2ptr(&openat_args->path);
        if (!openpath) goto syscall_error;
        ret = openat(
            openat_args->dirfd, (char*)openpath, openat_args->flags,
            openat_args->mode);
        break;
      case (SYS_unlinkat):;
        sargs_SYS_unlinkat_p* unlinkat_args = (sargs_SYS_unlinkat_p*)args;
        uintptr_t unlinkpath                = adm2ptr(&unlinkat_args->path);
        if (!unlinkpath) goto syscall_error;
        ret = unlinkat(
            unlinkat_args->dirfd, (char*)unlinkpath, unlinkat_args->flags);
        break;
      case (SYS_fstatat):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_fstatat_p* fstatat_args = (sargs_SYS_fstatat_p*)args;
        uintptr_t fstatpath               = adm2ptr(&fstatat_args->pathname);
        struct stat* fstatat_stats =
            (struct stat*)adm2ptr(&fstatat_args->stats);
        if (!fstatpath || !fstatat_stats) goto syscall_error;
        ret = fstatat(
            fstatat_args->dirfd, (char*)fstatpath, fstatat_stats,
            fstatat_args->flags);
        break;
      case (SYS_fstat):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_fstat_p* fstat_args = (sargs_SYS_fstat_p*)args;
        struct stat* fstat_stats = (struct stat*)adm2ptr(&fstat_args->stats);
        if (!fstat_stats) goto syscall_error;
        ret = fstat(fstat_args->fd, fstat_stats);
        break;
      case (SYS_getcwd):;  // TODO: how to handle string return
        if (!write_permitted) goto syscall_error;
        sargs_SYS_getcwd_p* getcwd_args = (sargs_SYS_getcwd_p*)args;
        uintptr_t getcwdbuf             = adm2ptr(&getcwd_args->buf);
        if (!getcwdbuf) goto syscall_error;
        retbuf     = getcwd((char*)getcwdbuf, getcwd_args->size);
        is_str_ret = 1;
        ret        = 0;
        break;
      case (SYS_chdir):;
        AdmPtr* path  = (AdmPtr*)args;
        uintptr_t ptr = adm2ptr(path);
        if (!ptr) goto syscall_error;
        ret = chdir((char*)ptr);
        break;
      case (SYS_epoll_ctl):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_epoll_ctl_p* epoll_ctl_args = (sargs_SYS_epoll_ctl_p*)args;
        struct epoll_event* event =
            (struct epoll_event*)adm2ptr(&epoll_ctl_args->event);
        if (!event) goto syscall_error;
        ret = epoll_ctl(
            epoll_ctl_args->epfd, epoll_ctl_args->op, epoll_ctl_args->fd,
            event);
        break;
      case (SYS_epoll_pwait):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_epoll_pwait_p* epoll_pwait_args =
            (sargs_SYS_epoll_pwait_p*)args;
        struct epoll_event* events =
            (struct epoll_event*)adm2ptr(&epoll_pwait_args->events);
        if (!events) goto syscall_error;
        ret = epoll_wait(
            epoll_pwait_args->epfd, events, epoll_pwait_args->maxevents,
            epoll_pwait_args->timeout);
        break;
      case (SYS_renameat2):;
        sargs_SYS_renameat2_p* renameat_args = (sargs_SYS_renameat2_p*)args;
        char* oldpath = (char*)adm2ptr(&renameat_args->oldpath);
        char* newpath = (char*)adm2ptr(&renameat_args->newpath);
        if (!oldpath || !newpath) goto syscall_error;
        ret = renameat(
            renameat_args->olddirfd, oldpath, renameat_args->newdirfd, newpath);
        break;
      case (SYS_write):;
        sargs_SYS_write_p* write_args = (sargs_SYS_write_p*)args;
        uintptr_t writebuf            = adm2ptr(&write_args->buf);
        if (!writebuf) goto syscall_error;
        ret = write(write_args->fd, (void*)writebuf, write_args->len);
        break;
      case (SYS_read):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_read_p* read_args = (sargs_SYS_read_p*)args;
        uintptr_t read_buf          = adm2ptr(&read_args->buf);
        if (!read_buf) goto syscall_error;
        ret = read(read_args->fd, (void*)read_buf, read_args->len);
        break;
      case (SYS_setsockopt):;
        sargs_SYS_setsockopt_p* setsockopt_args = (sargs_SYS_setsockopt_p*)args;
        uintptr_t optval_buf = adm2ptr(&setsockopt_args->option_value);
        if (!optval_buf) goto syscall_error;
        ret = setsockopt(
            setsockopt_args->socket, setsockopt_args->level,
            setsockopt_args->option_name, (void*)optval_buf,
            setsockopt_args->option_len);
        break;
      case (SYS_connect):;
        sargs_SYS_connect_p* connect_args = (sargs_SYS_connect_p*)args;
        struct sockaddr* connect_addr =
            (struct sockaddr*)adm2ptr(&connect_args->addr);
        if (!connect_addr) goto syscall_error;
        ret =
            connect(connect_args->sockfd, connect_addr, connect_args->addrlen);
        break;
      case (SYS_bind):;
        sargs_SYS_bind_p* bind_args = (sargs_SYS_bind_p*)args;
        struct sockaddr* bind_addr =
            (struct sockaddr*)adm2ptr(&bind_args->addr);
        if (!bind_addr) goto syscall_error;
        ret = bind(bind_args->sockfd, bind_addr, bind_args->addrlen);
        break;
      case (SYS_accept):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_accept_p* accept_args = (sargs_SYS_accept_p*)args;
        struct sockaddr* accept_addr =
            (struct sockaddr*)adm2ptr(&accept_args->addr);
        socklen_t* accept_addrlen = (socklen_t*)adm2ptr(&accept_args->addrlen);
        if (!accept_addr || !accept_addrlen) goto syscall_error;
        ret = accept(accept_args->sockfd, accept_addr, accept_addrlen);
        break;
      case (SYS_recvfrom):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_recvfrom_p* recvfrom_args = (sargs_SYS_recvfrom_p*)args;
        uintptr_t recv_buf                  = adm2ptr(&recvfrom_args->buf);
        if (!recv_buf) goto syscall_error;

        struct sockaddr* recv_addr =
            recvfrom_args->src_addr_is_null
                ? NULL
                : (struct sockaddr*)adm2ptr(&recvfrom_args->src_addr);
        socklen_t* recv_addrlen =
            recvfrom_args->src_addr_is_null
                ? NULL
                : (socklen_t*)adm2ptr(&recvfrom_args->addrlen);
        if ((recv_addr && recv_addrlen) || (!recv_addr && !recv_addrlen))
          goto syscall_error;

        ret = recvfrom(
            recvfrom_args->sockfd, (void*)recv_buf, recvfrom_args->len,
            recvfrom_args->flags, recv_addr, recv_addrlen);
        break;
      case (SYS_sendto):;
        sargs_SYS_sendto_p* sendto_args = (sargs_SYS_sendto_p*)args;
        uintptr_t send_buf              = adm2ptr(&sendto_args->buf);
        if (!send_buf) goto syscall_error;

        struct sockaddr* dest_addr =
            sendto_args->dest_addr_is_null
                ? NULL
                : (struct sockaddr*)adm2ptr(&sendto_args->dest_addr);
        socklen_t dest_addrlen =
            sendto_args->dest_addr_is_null ? 0 : sendto_args->addrlen;
        if ((dest_addr && dest_addrlen) || (!dest_addr && !dest_addrlen))
          goto syscall_error;

        ret = sendto(
            sendto_args->sockfd, (void*)send_buf, sendto_args->len,
            sendto_args->flags, dest_addr, dest_addrlen);
        break;
      case (SYS_fcntl):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_fcntl_p* fcntl_args = (sargs_SYS_fcntl_p*)args;
        uintptr_t cmd_args_buf        = adm2ptr(&fcntl_args->arg);
        if (!cmd_args_buf) goto syscall_error;
        if (!fcntl_args->has_struct)
          ret = fcntl(
              fcntl_args->fd, fcntl_args->cmd, *(unsigned long*)cmd_args_buf);
        else
          ret = fcntl(fcntl_args->fd, fcntl_args->cmd, cmd_args_buf);
        break;
      case (SYS_getpeername):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_getpeername_p* getpeername_args =
            (sargs_SYS_getpeername_p*)args;
        struct sockaddr* getpeername_addr =
            (struct sockaddr*)adm2ptr(&getpeername_args->addr);
        socklen_t* getpeername_addrlen =
            (socklen_t*)adm2ptr(&getpeername_args->addrlen);
        if (!getpeername_addr || !getpeername_addrlen) goto syscall_error;
        ret = getpeername(
            getpeername_args->sockfd, getpeername_addr, getpeername_addrlen);
        break;
      case (SYS_getsockname):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_getsockname_p* getsockname_args =
            (sargs_SYS_getsockname_p*)args;
        struct sockaddr* getsockname_addr =
            (struct sockaddr*)adm2ptr(&getsockname_args->addr);
        socklen_t* getsockname_addrlen =
            (socklen_t*)adm2ptr(&getsockname_args->addrlen);
        if (!getsockname_addr || !getsockname_addrlen) goto syscall_error;
        ret = getsockname(
            getsockname_args->sockfd, getsockname_addr, getsockname_addrlen);
        break;
      case (SYS_pselect6):;
        sargs_SYS_pselect_p* pselect_args = (sargs_SYS_pselect_p*)args;
        fd_set* readfds                   = pselect_args->readfds_is_null
                                                ? NULL
                                                : (fd_set*)adm2ptr(&pselect_args->readfds);
        fd_set* writefds                  = pselect_args->writefds_is_null
                                                ? NULL
                                                : (fd_set*)adm2ptr(&pselect_args->writefds);
        fd_set* exceptfds                 = pselect_args->exceptfds_is_null
                                                ? NULL
                                                : (fd_set*)adm2ptr(&pselect_args->exceptfds);
        struct timespec* timeout =
            pselect_args->timeout_is_null
                ? NULL
                : (struct timespec*)adm2ptr(&pselect_args->timeout);
        sigset_t* sigmask = pselect_args->sigmask_is_null
                                ? NULL
                                : (sigset_t*)adm2ptr(&pselect_args->sigmask);
        ret               = pselect(
            pselect_args->nfds, readfds, writefds, exceptfds, timeout, sigmask);
        break;
      default:
        goto syscall_error;
    }
  } else {  // calls with copy
    switch (syscall_info->syscall_num) {
      case (SYS_openat):;
        sargs_SYS_openat* openat_args = (sargs_SYS_openat*)args;
        ret                           = openat(
            openat_args->dirfd, openat_args->path, openat_args->flags,
            openat_args->mode);
        break;
      case (SYS_unlinkat):;
        sargs_SYS_unlinkat* unlinkat_args = (sargs_SYS_unlinkat*)args;
        ret                               = unlinkat(
            unlinkat_args->dirfd, unlinkat_args->path, unlinkat_args->flags);
        break;
      case (SYS_ftruncate):;
        sargs_SYS_ftruncate* ftruncate_args = (sargs_SYS_ftruncate*)args;
        ret = ftruncate(ftruncate_args->fd, ftruncate_args->offset);
        break;
      case (SYS_fstatat):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_fstatat* fstatat_args = (sargs_SYS_fstatat*)args;
        // Note the use of the implicit buffer in the stat args object (stats)
        ret = fstatat(
            fstatat_args->dirfd, fstatat_args->pathname, &fstatat_args->stats,
            fstatat_args->flags);
        break;
      case (SYS_fstat):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_fstat* fstat_args = (sargs_SYS_fstat*)args;
        // Note the use of the implicit buffer in the stat args object (stats)
        ret = fstat(fstat_args->fd, &fstat_args->stats);
        break;
      case (SYS_getcwd):;  // TODO: how to handle string return
        if (!write_permitted) goto syscall_error;
        sargs_SYS_getcwd* getcwd_args = (sargs_SYS_getcwd*)args;
        retbuf     = getcwd(getcwd_args->buf, getcwd_args->size);
        is_str_ret = 1;
        ret        = 0;
        break;
      case (SYS_write):;
        sargs_SYS_write* write_args = (sargs_SYS_write*)args;
        ret = write(write_args->fd, write_args->buf, write_args->len);
        break;
      case (SYS_read):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_read* read_args = (sargs_SYS_read*)args;
        ret = read(read_args->fd, read_args->buf, read_args->len);
        break;
      case (SYS_sync):;
        sync();
        ret = 0;
        break;
      case (SYS_fsync):;
        sargs_SYS_fsync* fsync_args = (sargs_SYS_fsync*)args;
        ret                         = fsync(fsync_args->fd);
        break;
      case (SYS_close):;
        sargs_SYS_close* close_args = (sargs_SYS_close*)args;
        ret                         = close(close_args->fd);
        break;
      case (SYS_lseek):;
        sargs_SYS_lseek* lseek_args = (sargs_SYS_lseek*)args;
        ret = lseek(lseek_args->fd, lseek_args->offset, lseek_args->whence);
        break;
      case (SYS_pipe2):;
        int* fds = (int*)args;
        ret      = pipe(fds);
        break;
      case (SYS_epoll_create1):;
        sargs_SYS_epoll_create1* epoll_args = (sargs_SYS_epoll_create1*)args;
        ret                                 = epoll_create(epoll_args->size);
        break;
      case (SYS_chdir):;
        char* path = (char*)args;
        ret        = chdir(path);
        break;
      case (SYS_epoll_ctl):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_epoll_ctl* epoll_ctl_args = (sargs_SYS_epoll_ctl*)args;
        ret                                 = epoll_ctl(
            epoll_ctl_args->epfd, epoll_ctl_args->op, epoll_ctl_args->fd,
            (struct epoll_event*)&epoll_ctl_args->event);
        break;
      case (SYS_epoll_pwait):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_epoll_pwait* epoll_pwait_args = (sargs_SYS_epoll_pwait*)args;
        ret                                     = epoll_wait(
            epoll_pwait_args->epfd, &epoll_pwait_args->events,
            epoll_pwait_args->maxevents, epoll_pwait_args->timeout);
        break;
      case (SYS_getpeername):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_getpeername* getpeername_args = (sargs_SYS_getpeername*)args;
        ret                                     = getpeername(
            getpeername_args->sockfd, (struct sockaddr*)&getpeername_args->addr,
            &getpeername_args->addrlen);
        break;
      case (SYS_getsockname):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_getsockname* getsockname_args = (sargs_SYS_getsockname*)args;
        ret                                     = getsockname(
            getsockname_args->sockfd, (struct sockaddr*)&getsockname_args->addr,
            &getsockname_args->addrlen);
        break;
      case (SYS_renameat2):;
        sargs_SYS_renameat2* renameat_args = (sargs_SYS_renameat2*)args;
        ret                                = renameat(
            renameat_args->olddirfd, renameat_args->oldpath,
            renameat_args->newdirfd, renameat_args->newpath);
        break;
      case (SYS_umask):;
        sargs_SYS_umask* umask_args = (sargs_SYS_umask*)args;
        ret                         = umask(umask_args->mask);
        break;
      case (SYS_socket):;
        sargs_SYS_socket* socket_args = (sargs_SYS_socket*)args;
        ret                           = socket(
            socket_args->domain, socket_args->type, socket_args->protocol);
        break;
      case (SYS_setsockopt):;
        sargs_SYS_setsockopt* setsockopt_args = (sargs_SYS_setsockopt*)args;
        ret                                   = setsockopt(
            setsockopt_args->socket, setsockopt_args->level,
            setsockopt_args->option_name, setsockopt_args->option_value,
            setsockopt_args->option_len);
        break;
      case (SYS_connect):;
        sargs_SYS_connect* connect_args = (sargs_SYS_connect*)args;
        ret                             = connect(
            connect_args->sockfd, (struct sockaddr*)&connect_args->addr,
            connect_args->addrlen);
        break;
      case (SYS_bind):;
        sargs_SYS_bind* bind_args = (sargs_SYS_bind*)args;
        ret                       = bind(
            bind_args->sockfd, (struct sockaddr*)&bind_args->addr,
            bind_args->addrlen);
        break;
      case (SYS_listen):;
        sargs_SYS_listen* listen_args = (sargs_SYS_listen*)args;
        ret = listen(listen_args->sockfd, listen_args->backlog);
        break;
      case (SYS_accept):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_accept* accept_args = (sargs_SYS_accept*)args;
        ret                           = accept(
            accept_args->sockfd, (struct sockaddr*)&accept_args->addr,
            &accept_args->addrlen);
        break;
      case (SYS_recvfrom):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_recvfrom* recvfrom_args = (sargs_SYS_recvfrom*)args;
        struct sockaddr* src_addr =
            recvfrom_args->src_addr_is_null ? NULL : &recvfrom_args->src_addr;
        socklen_t* addrlen =
            recvfrom_args->src_addr_is_null ? NULL : &recvfrom_args->addrlen;
        ret = recvfrom(
            recvfrom_args->sockfd, recvfrom_args->buf, recvfrom_args->len,
            recvfrom_args->flags, src_addr, addrlen);
        break;
      case (SYS_sendto):;
        sargs_SYS_sendto* sendto_args = (sargs_SYS_sendto*)args;
        struct sockaddr* dest_addr =
            sendto_args->dest_addr_is_null ? NULL : &sendto_args->dest_addr;
        socklen_t dest_addrlen =
            sendto_args->dest_addr_is_null ? 0 : sendto_args->addrlen;
        ret = sendto(
            sendto_args->sockfd, sendto_args->buf, sendto_args->len,
            sendto_args->flags, dest_addr, dest_addrlen);
        break;
      case (SYS_sendfile):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_sendfile* sendfile_args = (sargs_SYS_sendfile*)args;
        ret                               = sendfile(
            sendfile_args->out_fd, sendfile_args->in_fd, &sendfile_args->offset,
            sendfile_args->count);
        break;
      case (SYS_fcntl):;
        if (!write_permitted) goto syscall_error;
        sargs_SYS_fcntl* fcntl_args = (sargs_SYS_fcntl*)args;
        if (!fcntl_args->has_struct)
          ret = fcntl(
              fcntl_args->fd, fcntl_args->cmd,
              *(unsigned long*)fcntl_args->arg);
        else
          ret = fcntl(fcntl_args->fd, fcntl_args->cmd, fcntl_args->arg);
        break;
      case (SYS_getuid):;
        ret = getuid();
        break;
      case (SYS_pselect6):;
        sargs_SYS_pselect* pselect_args = (sargs_SYS_pselect*)args;
        fd_set* readfds =
            pselect_args->readfds_is_null ? NULL : &pselect_args->readfds;
        fd_set* writefds =
            pselect_args->writefds_is_null ? NULL : &pselect_args->writefds;
        fd_set* exceptfds =
            pselect_args->exceptfds_is_null ? NULL : &pselect_args->exceptfds;
        struct timespec* timeout =
            pselect_args->timeout_is_null ? NULL : &pselect_args->timeout;
        sigset_t* sigmask =
            pselect_args->sigmask_is_null ? NULL : &pselect_args->sigmask;
        ret = pselect(
            pselect_args->nfds, readfds, writefds, exceptfds, timeout, sigmask);
        break;
      default:
        goto syscall_error;
    }
  }

  /* Setup return value */
  if (write_permitted) {
    // string return is not handled
    *((int64_t*)call_ret) = ret;
  } else {  // when cannot write to ADM
    void* ret_data_ptr = (void*)edge_call_data_ptr();
    if (is_str_ret) {
      *(char**)ret_data_ptr = retbuf;  // TODO: check ptr stuff
      if (edge_call_setup_ret(edge_call, ret_data_ptr, sizeof(int64_t)) != 0)
        goto syscall_error;
    } else {
      *(int64_t*)ret_data_ptr = ret;
      if (edge_call_setup_ret(edge_call, ret_data_ptr, sizeof(int64_t)) != 0)
        goto syscall_error;
    }
  }

  return;

syscall_error:
  edge_call->return_data.call_status = CALL_STATUS_SYSCALL_FAILED;
  return;
}
