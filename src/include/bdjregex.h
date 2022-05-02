#ifndef INC_BDJREGEX_H
#define INC_BDJREGEX_H

typedef struct bdjregex bdjregex_t;

bdjregex_t * regexInit (const char *pattern);
void regexFree (bdjregex_t *rx);
char * regexEscape (const char *str);
bool regexMatch (bdjregex_t *rx, const char *str);
char ** regexGet (bdjregex_t *rx, const char *str);
void regexGetFree (char **val);

#endif /* INC_BDJREGEX_H */
