/* file.h */

char* vl_expandPath(char* restrict, char* restrict);
char* getEnvironVars(char**, char*, char*);
int setStackPath(void);
int loadFile(char*, char**);
int saveFile(char*, char*);
char* makeAbsolutePath(const char* path);
