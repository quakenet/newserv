/* 
 * splitline: splits a line into a list of parameters.
 * 
 * This function works "in place", i.e. the original input string is destroyed.
 *
 * This function splits lines on each space, up to a maximum of maxparams.
 * Spaces at the beginning/between words are replaced with '\0', as are all
 * \r or \n characters.
 *
 * If "coloncheck" is nonzero, a parameter beginning with ':' will be treated 
 * as the last.  The inevitable exception to this is the _first_ parameter...
 */
 
int splitline(char *inputstring, char **outputvector, int maxparams, int coloncheck) {
  char *c;
  int instr=0; /* State variable: 0 = between params, 1 = in param */
  int paramcount=0;
  
  for (c=inputstring;*c;c++) {
    if (instr) {
      if (*c==' ') {
        /* Space in string -- end string, obliterate */
        *c='\0';
        instr=0;
      }
    } else {
      if (*c==' ') {
        /* Space when not in string: obliterate */
        *c='\0';
      } else {
        /* Non-space character, start new word. */
	if (*c==':' && coloncheck && paramcount) {
	  outputvector[paramcount++]=c+1;
	  break;
	} else if (paramcount+1==maxparams) {
	  outputvector[paramcount++]=c;
	  break;
	} else {
	  outputvector[paramcount++]=c;
          instr=1;
        }
      }
    }
  }

  return paramcount;
}

/*
 * This function reconnects extra arguments together with spaces.
 *
 * Multiple spaces will be not be removed
 *
 * USE WITH CARE -- you don't have to worry about the original string
 * being untrusted, but you must get the arguments right :)
 */

void rejoinline(char *input, int argstojoin) {
  int i=0;
  int inword=0;
  char *ch;
  
  if (argstojoin<2) {
    return;
  }
  
  for (ch=input;;ch++) {
    if (inword) {
      if (*ch=='\0') {
        i++;
        if (i==argstojoin) {
          /* We're done */
          return;
        } else {
	  *ch=' ';
          inword=0;
        }
      }
    } else {
      /* not in word.. */
      if (*ch!='\0') {
        inword=1;
      } else {
        *ch=' ';
      }
    }
  }
}

