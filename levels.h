#ifndef LEVELS_H_INCLUDED
#define LEVELS_H_INCLUDED

#include "defs.h"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// COMPRESSION LEVELS FOR DNA 
//
#define LEVEL_1 "xv -tm 4:1:0:0 -tm 13:10:0:0 -g 0.85 "
#define LEVEL_2 "xv -tm 4:1:0:0 -tm 14:20:1:0 -g 0.85 "
#define LEVEL_3 "xv -tm 4:1:0:0 -tm 13:1:0:0 "
#define LEVEL_4 "xv -tm 4:1:0:0 -tm 13:1:0:0 "
#define LEVEL_5 "xv -tm 4:1:0:0 -tm 13:1:0:0 "
#define LEVEL_6 "xv -tm 4:1:0:0 -tm 13:1:0:0 "
#define LEVEL_7 "xv -tm 4:1:0:0 -tm 13:1:0:0 "
#define LEVEL_8 "xv -tm 1:1:0:0 -tm 4:1:0:0 -tm 6:1:0:0 -tm 9:1:0:0 -tm 11:1:0:0 -tm 13:10:0:0 -tm 14:20:1:0 -tm 19:20:1:3 -c 40 -g 0.8 "
#define LEVEL_9 "xv -tm 4:1:0:0 -tm 13:1:0:0 "

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

char    *GetLevels  (uint8_t);
void    PrintLevels (void);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#endif

