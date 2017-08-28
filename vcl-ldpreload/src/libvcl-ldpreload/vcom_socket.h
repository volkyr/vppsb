/*
 * Copyright (c) 2017 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef included_vcom_socket_h
#define included_vcom_socket_h

#include <libvcl-ldpreload/vcom_glibc_socket.h>
#include <vppinfra/types.h>

#define INVALID_SESSION_ID (~0)
#define INVALID_FD (~0)

typedef enum
{
  SOCKET_TYPE_UNBOUND = 0,
  SOCKET_TYPE_KERNEL_BOUND,
  SOCKET_TYPE_VPPCOM_BOUND
} vcom_socket_type_t;

typedef struct
{
  /* file descriptor -
   * fd 0, 1, 2 have special meaning and are reserved,
   * -1 denote invalid fd */
  i32 fd;

  /* session id - -1 denote invalid sid */
  i32 sid;

  /* socket type */
  vcom_socket_type_t type;

  /* vcom socket attributes here */

} vcom_socket_t;

static inline char *
vcom_socket_type_str (vcom_socket_type_t t)
{
  switch (t)
    {
    case SOCKET_TYPE_UNBOUND:
      return "SOCKET_TYPE_UNBOUND";

    case SOCKET_TYPE_KERNEL_BOUND:
      return "SOCKET_TYPE_KERNEL_BOUND";

    case SOCKET_TYPE_VPPCOM_BOUND:
      return "SOCKET_TYPE_VPPCOM_BOUND";

    default:
      return "SOCKET_TYPE_UNKNOWN";
    }
}

static inline int
vcom_socket_type_is_vppcom_bound (vcom_socket_type_t t)
{
  return t == SOCKET_TYPE_VPPCOM_BOUND;
}

static inline void
vsocket_init (vcom_socket_t * vsock)
{
  vsock->fd = INVALID_FD;
  vsock->sid = INVALID_SESSION_ID;
  vsock->type = SOCKET_TYPE_UNBOUND;
  /* vcom socket attributes init here */
}

static inline void
vsocket_set (vcom_socket_t * vsock, i32 fd, i32 sid, vcom_socket_type_t type)
{
  vsock->fd = fd;
  vsock->sid = sid;
  vsock->type = type;
  /* vcom socket attributes set here */
}

static inline int
vsocket_is_vppcom_bound (vcom_socket_t * vsock)
{
  return vcom_socket_type_is_vppcom_bound (vsock->type);
}

int vcom_socket_open_socket (int domain, int type, int protocol);

int vcom_socket_close_socket (int fd);

int vcom_socket_main_init (void);

void vcom_socket_main_destroy (void);

void vcom_socket_main_show (void);

int vcom_socket_is_vcom_fd (int fd);

int vcom_socket_close (int __fd);

ssize_t vcom_socket_read (int __fd, void *__buf, size_t __nbytes);

ssize_t vcom_socket_write (int __fd, const void *__buf, size_t __n);

int vcom_socket_fcntl_va (int __fd, int __cmd, va_list __ap);

int
vcom_socket_select (int vcom_nfds, fd_set * __restrict vcom_readfds,
		    fd_set * __restrict vcom_writefds,
		    fd_set * __restrict vcom_exceptfds,
		    struct timeval *__restrict timeout);


int vcom_socket_socket (int __domain, int __type, int __protocol);

int
vcom_socket_socketpair (int __domain, int __type, int __protocol,
			int __fds[2]);

int vcom_socket_bind (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len);

int
vcom_socket_getsockname (int __fd, __SOCKADDR_ARG __addr,
			 socklen_t * __restrict __len);

int
vcom_socket_connect (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len);

int
vcom_socket_getpeername (int __fd, __SOCKADDR_ARG __addr,
			 socklen_t * __restrict __len);

ssize_t
vcom_socket_send (int __fd, const void *__buf, size_t __n, int __flags);

ssize_t vcom_socket_recv (int __fd, void *__buf, size_t __n, int __flags);

/*
 * RETURN   1 if __fd is (SOCK_STREAM, SOCK_SEQPACKET),
 * 0 otherwise
 * */
int vcom_socket_is_connection_mode_socket (int __fd);

ssize_t
vcom_socket_sendto (int __fd, const void *__buf, size_t __n,
		    int __flags, __CONST_SOCKADDR_ARG __addr,
		    socklen_t __addr_len);

ssize_t
vcom_socket_recvfrom (int __fd, void *__restrict __buf, size_t __n,
		      int __flags, __SOCKADDR_ARG __addr,
		      socklen_t * __restrict __addr_len);

ssize_t
vcom_socket_sendmsg (int __fd, const struct msghdr *__message, int __flags);

#ifdef __USE_GNU
int
vcom_socket_sendmmsg (int __fd, struct mmsghdr *__vmessages,
		      unsigned int __vlen, int __flags);
#endif

ssize_t vcom_socket_recvmsg (int __fd, struct msghdr *__message, int __flags);

#ifdef __USE_GNU
int
vcom_socket_recvmmsg (int __fd, struct mmsghdr *__vmessages,
		      unsigned int __vlen, int __flags,
		      struct timespec *__tmo);
#endif

int
vcom_socket_getsockopt (int __fd, int __level, int __optname,
			void *__restrict __optval,
			socklen_t * __restrict __optlen);

int
vcom_socket_setsockopt (int __fd, int __level, int __optname,
			const void *__optval, socklen_t __optlen);

int vcom_socket_listen (int __fd, int __n);

int
vcom_socket_accept (int __fd, __SOCKADDR_ARG __addr,
		    socklen_t * __restrict __addr_len);

#ifdef __USE_GNU
int
vcom_socket_accept4 (int __fd, __SOCKADDR_ARG __addr,
		     socklen_t * __restrict __addr_len, int __flags);
#endif

int vcom_socket_shutdown (int __fd, int __how);

#endif /* included_vcom_socket_h */

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
