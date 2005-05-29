/*
  Easy async socket library with HELIX encryption and authentication
  Copyright (C) 2004-2005 Chris Porter.

  v1.03
    - changed nonce logic
  v1.02
    - added some \n stripping in crypto code
  v1.01
    - noticed small problem in _read, if ret == -1 && errno != EAGAIN then it would mess up
    - now disconnects on write buffer filling up
    - increased buffer size and maximum queue size
*/

#include "esockets.h"
#include <stdio.h>
#include <sys/poll.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <netinet/in.h>

#include "../lib/sha1.h"
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

  newsock->in.on_parse = buffer_parse_ascii;
  newsock->in.startpos = newsock->in.curpos = newsock->in.writepos = newsock->in.data;
  newsock->in.buffer_size = MAX_ASCII_LINE_SIZE;
  newsock->in.packet_length = 0;
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

int esocket_read(struct esocket *sock) {
  struct esocket_in_buffer *buf = &sock->in;
  int ret, bytesread = read(sock->fd, buf->writepos, buf->buffer_size - (buf->writepos - buf->data));

  if(!bytesread || ((bytesread == -1) && (errno != EAGAIN)))
    return 1;

  if((bytesread == -1) && (errno == EAGAIN))
    return 0;

  buf->writepos+=bytesread;

  do {
    ret = buf->on_parse(sock);
    if((ret != BUF_CONT) && ret)
      return ret;
  } while(ret);

  if((buf->curpos == (buf->data + buf->buffer_size)) && (buf->startpos == buf->data))
    return 1;

  if(buf->startpos != buf->curpos) {
    int moveback = buf->writepos - buf->startpos;
    memmove(buf->data, buf->startpos, moveback);
    buf->writepos = buf->data + moveback;
  } else {
    buf->writepos = buf->data;
  }

  buf->curpos = buf->writepos;
  buf->startpos = buf->data;

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
      ret = write(sock->fd, buf->head->line, buf->head->size);
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
        buf->head->line+=ret;
        buf->head->size-=ret;
      }
    }
    return 0;
  }

  /* something went wrong */
  return 1;
}

unsigned char *increase_nonce(unsigned char *nonce) {
  u_int64_t *inonce = (u_int64_t *)(nonce + 8);
  *inonce = htonq(ntohq(*inonce) + 1);
  return nonce;
}

int esocket_write(struct esocket *sock, char *buffer, int bytes) {
  int ret;
  if(sock->in.on_parse == buffer_parse_ascii) {
    ret = esocket_raw_write(sock, buffer, bytes);
  } else {
    unsigned char mac[MAC_LEN];
    char newbuf[MAX_BUFSIZE + USED_MAC_LEN + sizeof(packet_t)];
    packet_t packetlength;
    if(bytes > MAX_BUFSIZE)
      return 1;
    packetlength = htons(bytes + USED_MAC_LEN);

    memcpy(newbuf, &packetlength, sizeof(packet_t));
    h_nonce(&sock->keysend, increase_nonce(sock->sendnonce));
    h_encrypt(&sock->keysend, (unsigned char *)buffer, bytes, mac);

    memcpy(newbuf + sizeof(packet_t), buffer, bytes);
    memcpy(newbuf + sizeof(packet_t) + bytes, mac, USED_MAC_LEN);

    ret = esocket_raw_write(sock, newbuf, bytes + sizeof(packet_t) + USED_MAC_LEN);
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
  
  if(sock->in.on_parse == buffer_parse_ascii) {
    vsnprintf(nbuf, sizeof(nbuf) - 1, format, va); /* snprintf() and vsnprintf() will write at most size-1 of the characters, one for \n */
  } else {
    vsnprintf(nbuf, sizeof(nbuf), format, va);
  }
  va_end(va);

  len = strlen(nbuf);

  if(sock->in.on_parse == buffer_parse_ascii)
    nbuf[len++] = '\n';

  nbuf[len] = '\0';

  return esocket_write(sock, nbuf, len);
}

int buffer_parse_ascii(struct esocket *sock) {
  struct esocket_in_buffer *buf = &sock->in;

  for(;buf->curpos<buf->writepos;buf->curpos++) {
    if((*buf->curpos == '\n') || !*buf->curpos) {
      int ret;
      char *newline = buf->startpos;

      *buf->curpos = '\0';

      buf->startpos = buf->curpos + 1;

      ret = sock->events.on_line(sock, newline);
      if(ret)
        return ret;

      if(buf->curpos + 1 < buf->writepos) {
        buf->curpos++;
        return BUF_CONT;
      }
    }
  }

  return 0;
}

int buffer_parse_crypt(struct esocket *sock) {
  struct esocket_in_buffer *buf = &sock->in;
  unsigned char mac[MAC_LEN];

  if(!buf->packet_length) {
    if(buf->writepos - buf->startpos > sizeof(packet_t)) {
      memcpy(&buf->packet_length, buf->startpos, sizeof(packet_t));
      buf->startpos = buf->startpos + sizeof(packet_t);
      buf->curpos = buf->startpos;

      buf->packet_length = ntohs(buf->packet_length);
      if((buf->packet_length > buf->buffer_size - sizeof(packet_t)) || (buf->packet_length <= 0))
        return 1;

    } else {
      buf->curpos = buf->writepos;
      return 0;
    }
  }
 
  if(buf->packet_length <= buf->writepos - buf->startpos) {
    int ret;
    char *newline, *p;
    h_nonce(&sock->keyreceive, increase_nonce(sock->recvnonce));
    h_decrypt(&sock->keyreceive, (unsigned char *)buf->startpos, buf->packet_length - USED_MAC_LEN, mac);
    
    if(memcmp(mac, buf->startpos + buf->packet_length - USED_MAC_LEN, USED_MAC_LEN))
      return 1;
    
    p = newline = buf->startpos;
    newline[buf->packet_length - USED_MAC_LEN] = '\0';
    
    for(;*p;p++) /* shouldn't happen */
      if(*p == '\n')
        *p = '\0';

    buf->startpos = buf->startpos + buf->packet_length;
    buf->packet_length = 0;

    ret = sock->events.on_line(sock, newline);
    if(ret)
      return ret;

    return BUF_CONT;
  }
  
  buf->curpos = buf->writepos;
  return 0;
}

void switch_buffer_mode(struct esocket *sock, char *key, unsigned char *ournonce, unsigned char *theirnonce) {
  unsigned char ukey[20];
  SHA1_CTX context;

  memcpy(sock->sendnonce, ournonce, sizeof(sock->sendnonce));
  memcpy(sock->recvnonce, theirnonce, sizeof(sock->recvnonce));

  SHA1Init(&context);
  SHA1Update(&context, (unsigned char *)key, strlen(key));
  SHA1Update(&context, (unsigned char *)" ", 1);
  /* not sure if this is cryptographically secure! */
  SHA1Update(&context, (unsigned char *)ournonce, NONCE_LEN);
  SHA1Final(ukey, &context);

  sock->in.on_parse = buffer_parse_crypt;
  sock->in.buffer_size = MAX_BINARY_LINE_SIZE;
  
  h_key(&sock->keysend, ukey, sizeof(ukey));

  SHA1Init(&context);
  SHA1Update(&context, (unsigned char *)key, strlen(key));
  SHA1Update(&context, (unsigned char *)" ", 1);
  SHA1Update(&context, (unsigned char *)theirnonce, NONCE_LEN);
  SHA1Final(ukey, &context);

  h_key(&sock->keyreceive, ukey, sizeof(ukey));
}

