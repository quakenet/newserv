#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../lib/array.h"
#include "../lib/base64.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"

#include "qabot.h"

void qabot_freetext(qab_bot* bot, qab_text* text) {
  qab_textsection* section;
  qab_textsection* nsection;
  
  for (section = text->sections; section; section = nsection) {
    nsection = section->next;
    qabot_freesection(text, section);
  }
  
  if (text->next)
    text->next->prev = text->prev;
  if (text->prev)
    text->prev->next = text->next;
  else
    bot->texts = text->next;
  free(text);
}

void qabot_freesection(qab_text* text, qab_textsection* section) {
  qab_spam* line;
  qab_spam* nline;
  
  for (line = section->lines; line; line = nline) {
    nline = line->next;
    
    free(line->message);
    free(line);
  }
  
  if (section->next)
    section->next->prev = section->prev;
  if (section->prev)
    section->prev->next = section->next;
  else
    text->sections = section->next;
  
  if (text->section_tail == section)
    text->section_tail = section->prev;
  
  free(section);
  
  text->section_count--;
  if (text->section_count < 0)
    text->section_count = 0;
}
