/*
  Easy async socket library
  Copyright (C) 2004-2007 Chris Porter.
*/

#include "esockets.h"
#include <stdio.h>
#include <sys/poll.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <netinet/in.h>

#include "../core/events.h"

struct esocket *socklist = NULL;
char signal_set = 0;
unsigned short token = 1;

void sigpipe_handler(int moo) { }

unsigned short esocket_token(void) {
  return token++;
}

struct esocket *esocket_add(int fd, char socket_type, struct esocket_events *events, unsigned short token) {
  int flags;
  struct esocket *newsock = (struct esocket *)malloc(sizeof(struct esocket));
  if(!newsock)
    return NULL;

  memcpy(&newsock->events, events, sizeof(newsock->events));

  newsock->socket_type = socket_type;

  switch(socket_type) {
    case ESOCKET_LISTENING:
      /* listening sockets require POLLIN, which is triggered on connect */
      flags = POLLIN;
      newsock->socket_status = ST_LISTENING;
      break;
    case ESOCKET_OUTGOING:
      /* outgoing connections require POLLOUT (triggered on connect) */
      flags = POLLOUT;
      newsock->socket_status = ST_CONNECTING;
      break;
    case ESOCKET_OUTGOING_CONNECTED:
      /* outgoing connections that have connected (think localhost) require just POLLIN */
      flags = POLLIN;
      newsock->socket_status = ST_CONNECTED;
      break;
    case ESOCKET_INCOMING:
      /* sockets that have just connected to us require (initially) POLLIN */
      flags = POLLIN;
      newsock->socket_status = ST_CONNECTED;
      break;
    default:
      return NULL;
  }

  registerhandler(fd, flags | POLLERR | POLLHUP, esocket_poll_event);
  newsock->fd = fd;
  newsock->next = socklist;

  newsock->in.mode = PARSE_ASCII;
  newsock->in.data = NULL;
  newsock->in.size = 0;
  newsock->in.cryptobuf = NULL;
  newsock->in.cryptobufsize = 0;
  newsock->in.mac = 0;

  newsock->out.head = NULL;
  newsock->out.end = NULL;
  newsock->out.count = 0;
  newsock->token = token;
  newsock->tag = NULL;
  
  if(!signal_set) {
    signal(SIGPIPE, sigpipe_handler);
    signal_set = 1;
  }

  socklist = newsock;

  return newsock;
}

struct esocket *find_esocket_from_fd(int fd) {
  struct esocket *p = socklist;

  for(;p;p=p->next)
    if(p->fd == fd)
      return p;

  return NULL;
}

void esocket_clean_by_token(unsigned short token) {
  struct esocket *cp = socklist, *np;

  /* not efficient but boohoo */
  for(;cp;) {
    if(cp->token == token) {
      np = cp->next;
      esocket_disconnect(cp);
      cp = np;
    } else {
      cp = cp->next;
    }
  }
}

void esocket_poll_event(int fd, short events) {
  struct esocket *active = find_esocket_from_fd(fd);

  if(!active)
    return;

  switch(active->socket_status) {
    case ST_LISTENING:
      active->events.on_accept(active);
      break;
    case ST_CONNECTING:
      if(events & (POLLERR | POLLHUP)) {
        esocket_disconnect(active);
        return;
      }
      if(events & POLLOUT) { /* connected if connecting! */
        deregisterhandler(active->fd, 0);
        registerhandler(active->fd, POLLIN | POLLERR | POLLHUP, esocket_poll_event);
        active->socket_status = ST_CONNECTED;
        active->events.on_connect(active);
      }
      break;
    case ST_CONNECTED:
    case ST_BLANK:
      if(events & (POLLERR | POLLHUP)) {
        esocket_disconnect(active);
        return;
      }
      if(events & POLLOUT) { /* flush buffer */
        if(esocket_raw_write(active, NULL, 0)) {
          esocket_disconnect(active);
          return;
        }
        return;
      }
      if(events & POLLIN) { /* read buffer */
        int ret = esocket_read(active);
        if(ret) {
          if(ret != BUF_ERROR)
            esocket_disconnect(active);
          return;
        }
      }

      break;
  }    
}

void esocket_disconnect(struct esocket *active) {
  struct esocket *l = NULL, *p = socklist;

  if(active->events.on_disconnect)
    active->events.on_disconnect(active);

  for(;p;l=p,p=p->next)
    if(p == active) {
      struct esocket_packet *pkt = p->out.head, *npkt;
      if(l) {
        l->next = p->next;
      } else {
        socklist = p->next;
      }
     

      deregisterhandler(p->fd, 1);

      for(;pkt;) {
        npkt = pkt->next;
        free(pkt->line);
        free(pkt);
        pkt = npkt;
      }

      if(p->in.data)
        free(p->in.data);
      if(p->in.cryptobuf)
        free(p->in.cryptobuf);

      free(p);
      break;
    }  
}

void esocket_disconnect_when_complete(struct esocket *active) {
  if(active->out.count) {
    active->socket_status = ST_BLANK;
  } else {
    esocket_disconnect(active);
  }
}

int parse_ascii(struct esocket *sock, int *bytes_to_strip) {
  struct esocket_in_buffer *buf = &sock->in;
  char *p;
  int i, ret;

  for(p=buf->data,i=0;i<buf->size;i++,p++) {
    if(*p == '\0' || *p == '\n') {
      *p = '\0';

      *bytes_to_strip = i + 1;
      ret = sock->events.on_line(sock, buf->data);
      if(ret)
        return ret;

      return 0;
    }
  }

  *bytes_to_strip = 0;
  return 0;
}

void seqno_update(hmacsha256 *h, u_int64_t value) {
  u_int64_t v2 = htonq(value);
  hmacsha256_update(h, (unsigned char *)&v2, 8);
}

int crypto_newblock(struct esocket *sock, unsigned char *block) {
  unsigned char *p, *p2;
  int i;
  struct esocket_in_buffer *buf = &sock->in;

  if(buf->mac) {
    unsigned char digest[32];
    int ret;
    hmacsha256_final(&sock->clienthmac, digest);

    if(memcmp(block, digest, 16)) /* mac error */
      return 1;

    hmacsha256_init(&sock->clienthmac, sock->clienthmackey, 32);
    seqno_update(&sock->clienthmac, sock->clientseqno);
    sock->clientseqno++;

    ret = sock->events.on_line(sock, (char *)buf->cryptobuf);
    free(buf->cryptobuf);
    buf->cryptobuf = NULL;
    buf->cryptobufsize = 0;
    buf->mac = 0;

    return ret;
  }

  hmacsha256_update(&sock->clienthmac, block, 16);
  p = rijndaelcbc_decrypt(sock->clientcrypto, block);
  for(p2=p,i=0;i<16;i++,p2++) { /* locate terminator */
    if(*p2 == '\n' || *p2 == '\0') {
      *p2 = '\0';
      buf->mac = 1;
    }
  }

  p2 = realloc(buf->cryptobuf, buf->cryptobufsize + 16);
  if(!p2)
    Error("nterface", ERR_STOP, "realloc() failed in crypto_newblock (esockets.c)");
  buf->cryptobuf = p2;

  memcpy(p2 + buf->cryptobufsize, p, 16);
  buf->cryptobufsize+=16;

  return 0;
}

int parse_crypt(struct esocket *sock, int *bytes_to_strip) {
  struct esocket_in_buffer *buf = &sock->in;
  int remaining = buf->size, ret;

  *bytes_to_strip = 0;
  if(remaining < 16)
    return 0;

  ret = crypto_newblock(sock, (unsigned char *)buf->data);
  *bytes_to_strip = 16;

  return ret;
}

int esocket_read(struct esocket *sock) {
  struct esocket_in_buffer *buf = &sock->in;
  char bufd[16384], *p;
  int bytesread, ret, strip;

  bytesread = read(sock->fd, bufd, sizeof(bufd));
  if(!bytesread || ((bytesread == -1) && (errno != EAGAIN)))
    return 1;

  if((bytesread == -1) && (errno == EAGAIN))
    return 0;

  p = realloc(buf->data, buf->size + bytesread);
  if(!p)
    Error("nterface", ERR_STOP, "realloc() failed in esocket_read (esockets.c)");

  buf->data = p;
  memcpy(buf->data + buf->size, bufd, bytesread);
  buf->size+=bytesread;

  do {
    if(buf->mode == PARSE_ASCII) {
      ret = parse_ascii(sock, &strip);
    } else {
      ret = parse_crypt(sock, &strip);
    }

    if(strip > 0) {
      if(buf->size - strip == 0) {
        free(buf->data);
        buf->data = NULL;
      } else {
        memmove(buf->data, buf->data + strip, buf->size - strip);
        p = realloc(buf->data, buf->size - strip);
        if(!p)
          Error("nterface", ERR_STOP, "realloc() failed in esocket_read (esockets.c)");

        buf->data = p;
      }

      buf->size-=strip;
    }

    if(ret)
      return ret;
  } while(strip && buf->data);

  return 0;
}

struct esocket_packet *esocket_new_packet(struct esocket_out_buffer *buf, char *buffer, int bytes) {
  struct esocket_packet *nw;

  if(buf->count == MAX_OUT_QUEUE_SIZE)
    return NULL;

  nw = malloc(sizeof(struct esocket_packet));
  if(!nw)
    return NULL;

  nw->line = malloc(bytes);
  if(!nw->line) {
    free(nw);
    return NULL;
  }

  if(!buf->head) {
    buf->head = nw;
  } else {
    buf->end->next = nw;
  }

  buf->end = nw;

  nw->next = NULL;
  nw->size = bytes;
  nw->startpos = 0;

  memcpy(nw->line, buffer, bytes);

  buf->count++;

  return nw;
}

int esocket_raw_write(struct esocket *sock, char *buffer, int bytes) {
  struct esocket_out_buffer *buf = &sock->out;

  if((bytes > USHRT_MAX) || (!buffer && bytes) || (buffer && !bytes))
    return 1;

  if(bytes) { /* if we're not flushing */
    if(buf->count) { /* if we're currently blocked */
      if(!esocket_new_packet(buf, buffer, bytes))
        return 1;
      return 0;
    } else { /* not currently blocked */
      int ret;
      for(;;) {
        ret = write(sock->fd, buffer, bytes);
        if(ret == bytes) {
          return 0;
        } else if(ret == -1) { /* something bad happened */
          switch(errno) {
            case EAGAIN: /* was going to block, store the data in the buffer */
              if(!esocket_new_packet(buf, buffer, bytes))
                return 1;

              deregisterhandler(sock->fd, 0); /* now we need to ignore the socket until it's unblocked */
              registerhandler(sock->fd, POLLOUT | POLLERR | POLLHUP, esocket_poll_event);

              return 0;
              break;
            case EPIPE: /* if there's some weird code disconnect the socket */
            default:
              return 1;
              break;
          }
        } else { /* less than total was written, try again */
          buffer+=ret;
          bytes-=ret;
        }
      }
    }
  } else if(buf->count) { /* if we're just flushing the buffer */
    int ret;
    for(;;) {
      ret = write(sock->fd, buf->head->line + buf->head->startpos, buf->head->size);
      if(!ret) {
        return 0;
      } else if(ret == buf->head->size) {
        struct esocket_packet *p = buf->head;
        buf->head = buf->head->next;

        free(p->line);
        free(p);

        buf->count--;
        if(!buf->head) { /* if we've exhausted the buffer */
          buf->end = NULL;

          if(sock->socket_status == ST_BLANK) {
            esocket_disconnect(sock);
          } else {
            deregisterhandler(sock->fd, 0);

            registerhandler(sock->fd, POLLIN | POLLERR | POLLHUP, esocket_poll_event);
          }
          return 0;
        }
      } else if(ret == -1) {
        switch(errno) {
          case EAGAIN: /* was going to block, wait until we're called again */
            return 0;
            break;

          case EPIPE: /* if there's some weird code disconnect the socket */
          default:
            return 1;
            break;
        }
      } else {
        buf->head->startpos+=ret;
        buf->head->size-=ret;
      }
    }
    return 0;
  }

  /* something went wrong */
  return 1;
}

int esocket_write(struct esocket *sock, char *buffer, int bytes) {
  int ret;
  if(sock->in.mode == PARSE_ASCII) {
    ret = esocket_raw_write(sock, buffer, bytes);
  } else {
    unsigned char newbuf[MAX_BUFSIZE + USED_MAC_LEN], *p = newbuf, hmacdigest[32];
    hmacsha256 hmac;
    int padding = 16 - bytes % 16, i;

    if(padding == 16)
      padding = 0;

    memcpy(newbuf, buffer, bytes);
    for(i=0;i<padding;i++)
      newbuf[bytes + i] = i;
    bytes+=padding;

    hmacsha256_init(&hmac, sock->serverhmackey, 32);
    seqno_update(&hmac, sock->serverseqno);
    sock->serverseqno++;

    i = bytes;
    while(i) {
      unsigned char *ct = rijndaelcbc_encrypt(sock->servercrypto, p);
      hmacsha256_update(&hmac, ct, 16);
      memcpy(p, ct, 16);

      buffer+=16;
      p+=16;
      i-=16;
    }

    hmacsha256_final(&hmac, hmacdigest);
    memcpy(p, hmacdigest, USED_MAC_LEN);

    ret = esocket_raw_write(sock, (char *)newbuf, bytes + USED_MAC_LEN);
  }

  /* AWOOGA!! */
  if(ret)
    esocket_disconnect(sock);

  return ret;
}

int esocket_write_line(struct esocket *sock, char *format, ...) {
  char nbuf[MAX_ASCII_LINE_SIZE];
  va_list va;
  int len;

  va_start(va, format);
  
  vsnprintf(nbuf, sizeof(nbuf) - 1, format, va); /* snprintf() and vsnprintf() will write at most size-1 of the characters, one for \n */
  va_end(va);

  len = strlen(nbuf);

  nbuf[len++] = '\n';
  nbuf[len] = '\0';

  return esocket_write(sock, nbuf, len);
}

static void derive_key(unsigned char *out, unsigned char *key, u_int64_t seqno, unsigned char *extra, int extralen) {
  hmacsha256 hmac;

  hmacsha256_init(&hmac, key, 32);
  seqno_update(&hmac, seqno);
  hmacsha256_update(&hmac, extra, extralen);

  hmacsha256_final(&hmac, out);
}

void rekey_esocket(struct esocket *sock, unsigned char *serveriv, unsigned char *clientiv) {
  unsigned char key[32];

  derive_key(key, sock->serverrawkey, sock->serverkeyno, (unsigned char *)":SKEY", 5);
  sock->servercrypto = rijndaelcbc_init(key, 256, serveriv, 0);
  derive_key(sock->serverhmackey, sock->serverrawkey, sock->serverkeyno, (unsigned char *)":SHMAC", 6);
  sock->serverkeyno++;

  derive_key(key, sock->clientrawkey, sock->clientkeyno, (unsigned char *)":CKEY", 5);
  sock->clientcrypto = rijndaelcbc_init(key, 256, clientiv, 1);
  derive_key(sock->clienthmackey, sock->clientrawkey, sock->clientkeyno, (unsigned char *)":CHMAC", 6);
  sock->clientkeyno++;
}

void switch_buffer_mode(struct esocket *sock, unsigned char *serverkey, unsigned char *serveriv, unsigned char *clientkey, unsigned char *clientiv) {
  memcpy(sock->serverrawkey, serverkey, 32);
  memcpy(sock->clientrawkey, clientkey, 32);

  sock->in.mode = PARSE_CRYPTO;

  sock->clientseqno = 0;
  sock->serverseqno = 0;

  sock->clientkeyno = 0;
  sock->serverkeyno = 0;

  rekey_esocket(sock, serveriv, clientiv);

  hmacsha256_init(&sock->clienthmac, sock->clienthmackey, 32);
  seqno_update(&sock->clienthmac, sock->clientseqno);

  sock->clientseqno++;
}

