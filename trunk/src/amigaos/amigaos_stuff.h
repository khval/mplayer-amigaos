#ifndef __AMIGAOS_STUFF__
#define __AMIGAOS_STUFF__

// return -1 if a pb
int AmigaOS_Open(int argc, char *argv[]);
void AmigaOS_Close(void);
void AmigaOS_ParseArg(int argc, char *argv[], int *new_argc, char ***new_argv);
#endif
