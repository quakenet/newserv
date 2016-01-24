#ifndef __RBL_H
#define __RBL_H

struct rbl_instance;

typedef struct rbl_ops {
  int (*lookup)(struct rbl_instance *rbl, struct irc_in_addr *ip, char *message, size_t msglen);
  int (*refresh)(struct rbl_instance *rbl);
  void (*dtor)(struct rbl_instance *rbl);
} rbl_ops;

typedef struct rbl_instance {
  sstring *name;
  const rbl_ops *ops;
  void *udata;
  struct rbl_instance *next;
} rbl_instance;

extern rbl_instance *rbl_instances;

int registerrbl(const char *name, const rbl_ops *ops, void *udata);
void deregisterrblbyops(const rbl_ops *ops);

#define RBL_LOOKUP(rbl, ip, msg, msglen) rbl->ops->lookup(rbl, ip, msg, msglen)
#define RBL_REFRESH(rbl) rbl->ops->refresh(rbl)
#define RBL_DTOR(rbl) rbl->ops->dtor(rbl)

#endif /* __RBL_H */
