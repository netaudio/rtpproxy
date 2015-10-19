/*
 * Copyright (c) 2004-2006 Maxim Sobolev <sobomax@FreeBSD.org>
 * Copyright (c) 2006-2015 Sippy Software, Inc., http://www.sippysoft.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include "rtpp_types.h"
#include "rtpp_refcnt.h"
#include "rtpp_socket.h"
#include "rtpp_socket_fin.h"
#include "rtpp_util.h"
#include "rtpp_netio_async.h"
#include "rtp.h"
#include "rtp_packet.h"

struct rtpp_socket_priv {
    struct rtpp_socket pub;
    int fd;
    void *rco[0];
};

static void rtpp_socket_dtor(struct rtpp_socket_priv *);
static int rtpp_socket_bind(struct rtpp_socket *, const struct sockaddr *,
  int);
static int rtpp_socket_settos(struct rtpp_socket *, int);
static int rtpp_socket_setrbuf(struct rtpp_socket *, int);
static int rtpp_socket_setnonblock(struct rtpp_socket *);
static int rtpp_socket_send_pkt(struct rtpp_socket *, struct sthread_args *,
  const struct sockaddr *, int, struct rtp_packet *);
static struct rtp_packet *rtpp_socket_rtp_recv(struct rtpp_socket *);
static int rtpp_socket_getfd(struct rtpp_socket *);

#define PUB2PVT(pubp) \
  ((struct rtpp_socket_priv *)((char *)(pubp) - offsetof(struct rtpp_socket_priv, pub)))

struct rtpp_socket *
rtpp_socket_ctor(int domain, int type)
{
    struct rtpp_socket_priv *pvt;

    pvt = rtpp_zmalloc(sizeof(struct rtpp_socket_priv) + rtpp_refcnt_osize());
    if (pvt == NULL) {
        goto e0;
    }
    pvt->fd = socket(domain, type, 0);
    if (pvt->fd < 0) {
        goto e1;
    }
    pvt->pub.rcnt = rtpp_refcnt_ctor_pa(&pvt->rco[0], pvt,
      (rtpp_refcnt_dtor_t)&rtpp_socket_dtor);
    if (pvt->pub.rcnt == NULL) {
        goto e2;
    }
    pvt->pub.bind = &rtpp_socket_bind;
    pvt->pub.settos = &rtpp_socket_settos;
    pvt->pub.setrbuf = &rtpp_socket_setrbuf;
    pvt->pub.setnonblock = &rtpp_socket_setnonblock;
    pvt->pub.send_pkt = &rtpp_socket_send_pkt;
    pvt->pub.rtp_recv = &rtpp_socket_rtp_recv;
    pvt->pub.getfd = &rtpp_socket_getfd;
    return (&pvt->pub);
e2:
    close(pvt->fd);
e1:
    free(pvt);
e0:
    return (NULL);
}

static void
rtpp_socket_dtor(struct rtpp_socket_priv *pvt)
{

    rtpp_socket_fin(&pvt->pub);
    shutdown(pvt->fd, SHUT_RDWR);
    close(pvt->fd);
    free(pvt);
}

static int
rtpp_socket_bind(struct rtpp_socket *self, const struct sockaddr *addr,
  int addrlen)
{
    struct rtpp_socket_priv *pvt;

    pvt = PUB2PVT(self);
    return (bind(pvt->fd, addr, addrlen));
}

static int
rtpp_socket_settos(struct rtpp_socket *self, int tos)
{
    struct rtpp_socket_priv *pvt;

    pvt = PUB2PVT(self);
    return (setsockopt(pvt->fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)));
}

static int
rtpp_socket_setrbuf(struct rtpp_socket *self, int so_rcvbuf)
{
    struct rtpp_socket_priv *pvt;

    pvt = PUB2PVT(self);
    return (setsockopt(pvt->fd, SOL_SOCKET, SO_RCVBUF, &so_rcvbuf,
      sizeof(so_rcvbuf)));
}

static int
rtpp_socket_setnonblock(struct rtpp_socket *self)
{
    int flags;
    struct rtpp_socket_priv *pvt;

    pvt = PUB2PVT(self);
    flags = fcntl(pvt->fd, F_GETFL);
    return (fcntl(pvt->fd, F_SETFL, flags | O_NONBLOCK));
}

static int 
rtpp_socket_send_pkt(struct rtpp_socket *self, struct sthread_args *str,
  const struct sockaddr *daddr, int addrlen, struct rtp_packet *pkt)
{
    struct rtpp_socket_priv *pvt;

    pvt = PUB2PVT(self);
    return (rtpp_anetio_send_pkt(str, pvt->fd, daddr, addrlen, pkt,
      self->rcnt));
}

static struct rtp_packet *
rtpp_socket_rtp_recv(struct rtpp_socket *self)
{
    struct rtpp_socket_priv *pvt;

    pvt = PUB2PVT(self);
    return (rtp_recv(pvt->fd));
}

static int
rtpp_socket_getfd(struct rtpp_socket *self)
{
    struct rtpp_socket_priv *pvt;

    pvt = PUB2PVT(self);
    return (pvt->fd);
}
