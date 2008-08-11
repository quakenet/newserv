#ifndef __CCASSERT_H
#define __CCASSERT_H

#define CCASSERT(predicate) typedef char compile_time_constraint_violated[2*((predicate)!=0)-1];

#endif
