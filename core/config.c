/*
 * config.c:
 *
 * Facilities for handling the config file
 */

#define __USE_GNU
#include <string.h>

#include "../lib/sstring.h"
#include "../lib/array.h"
#include "error.h"
#include "config.h"
#include <stdio.h>

char *theconfig;

typedef struct {
  sstring *key;
  array values;
} configitem;

typedef struct {
  sstring *modulename;
  array items;
} configsection;

array sections;

void rehashconfig() {
  Error("config",ERR_INFO,"Rehashing config file.");
  freeconfig();
  initconfig(NULL);
}

void freeconfig() {
  int i,j,k;
  configsection *sects;
  configitem *items;
  sstring **values;
  
  sects=(configsection *)(sections.content);
  for (i=0;i<sections.cursi;i++) {
    items=(configitem *)(sects[i].items.content);
    for (j=0;j<sects[i].items.cursi;j++) {
      freesstring(items[j].key);
      values=(sstring **)(items[j].values.content);
      for (k=0;k<items[j].values.cursi;k++) {
        freesstring(values[k]);
      }
      array_free(&(items[j].values));
    }
    array_free(&(sects[i].items));
    freesstring(sects[i].modulename);
  }
  array_free(&sections);
}

void initconfig(char *filename) {
  FILE *fp;
  configsection *sects=NULL;
  configitem *items=NULL;
  sstring **values=NULL;
  char buf[255];
  char *cp;
  int si=-1;
  int ii,j;
  int matched;

  if (filename==NULL) {
    filename=theconfig;
  } else {
    theconfig=filename;
  }
  
  array_init((&sections),sizeof(configsection));
  
  if ((fp=fopen(filename,"r"))==NULL) {
    Error("core",ERR_FATAL,"Couldn't load config file.");
    exit(1);
  }
  
  while (!feof(fp)) {
    /* Read in a line */
    fgets(buf,255,fp);
    /* Check we got something */
    if (feof(fp))
      break;
  
    /* Allow some comment chars */
    if (buf[0]=='#' || buf[0]==';' || (buf[0]=='/' && buf[1]=='/'))
      continue;

    /* Blow away the line ending */
    for (cp=buf;*cp;cp++)
      if (*cp=='\n' || *cp=='\r') {
        *cp='\0';
        break;
      }

    /* Check it's long enough */
    if (strlen(buf)<3)
      continue;
      
    if (buf[0]=='[') {
      /* New section (possibly) -- hunt for the ']' */
      for (cp=&(buf[2]);*cp;cp++) {
        if (*cp==']') {
          si=array_getfreeslot(&sections);
          sects=(configsection *)(sections.content);         
          array_init(&(sects[si].items),sizeof(configitem));
          array_setlim1(&(sects[si].items),10);
          *cp='\0';
          sects[si].modulename=getsstring(&(buf[1]),255);
          break;
        }
      }
    } else {
      /* Ignore if we're not in a valid section */
      if (si<0)
        continue;
        
      for (cp=buf;*cp;cp++) {
        if (*cp=='=') {
          *cp='\0';
          matched=0;
          for (ii=0;ii<sects[si].items.cursi;ii++) {
            if (!strcmp(items[ii].key->content,buf)) {
              /* Another value for an existing key */
              j=array_getfreeslot(&(items[ii].values));
              values=(sstring **)(items[ii].values.content);
              values[j]=getsstring(cp+1,512);
              matched=1;
            }
          }
          
          if (matched==0) {
            /* New key */
            ii=array_getfreeslot(&(sects[si].items));
            items=(configitem *)(sects[si].items.content);
            items[ii].key=getsstring(buf,512);
            array_init(&(items[ii].values),sizeof(sstring *));
            array_setlim1(&(items[ii].values),5);
            j=array_getfreeslot(&(items[ii].values));
            values=(sstring **)(items[ii].values.content);
            values[j]=getsstring(cp+1,512); /* looks nasty but is OK, this char is '=' 
                                             * and we know 'buf' is null-terminated */
          }
          break;
        }
      }
    }
  }
  
  fclose(fp);
}

void dumpconfig() {
  int i,j,k;
  configsection *sects;
  configitem *items;
  sstring **values;
  
  printf("Dumping complete configuration database.\n");
  printf("Total sections: %d\n",sections.cursi);
  
  sects=(configsection *)(sections.content);
  for (i=0;i<sections.cursi;i++) {
    printf ("\nSection %02d: [%s] has %d items\n",i,sects[i].modulename->content,sects[i].items.cursi);
    items=(configitem *)(sects[i].items.content);
    for(j=0;j<sects[i].items.cursi;j++) {
      printf("  Item %02d: [%s] has %d values\n",j,items[j].key->content,items[j].values.cursi);
      values=(sstring **)(items[j].values.content);
      for (k=0;k<items[j].values.cursi;k++) {
        printf("    Value %2d: [%s]\n",k,values[k]->content);
      }
    }
  }
  
  printf("\n\nEnd of configuration database.\n");
}  

/*
 * Two routes for extacting config info:
 *
 *  - getconfigitem() is for keys which can only meaningfully have one value.
 *    It returns the last value for that key (so the config file has "last
 *    value overrides" semantics.
 *  - getconfigitems() is for keys which can have multiple values, it returns
 *    a pointer to the array of values.
 */
 
array *getconfigitems(char *module, char *key) {
  int i,j;
  configsection *sects;
  configitem *items;
  
  sects=(configsection *)(sections.content);
  for (i=0;i<sections.cursi;i++) {
    if (!strcmp(module,sects[i].modulename->content)) {
      /* Found the module */
      items=(configitem *)(sects[i].items.content);
      for (j=0;j<sects[i].items.cursi;j++) {
        if (!strcmp(key,items[j].key->content)) {
          return (&items[j].values);
        }
      }
      return NULL;
    }
  }  
  return NULL;
}

sstring *getconfigitem(char *module, char *key) {
  array *a;
  sstring **values;
  
  if ((a=getconfigitems(module,key))==NULL) {
    return NULL;
  }
  
  values=(sstring **)(a->content);
  return values[(a->cursi-1)];
}

sstring *getcopyconfigitem(char *module, char *key, char *defaultvalue, int len) {
  sstring *ss;
  
  ss=getconfigitem(module,key);
  if (ss!=NULL) {
    return getsstring(ss->content,len);
  } else {
    return getsstring(defaultvalue,len);
  }  
}  
