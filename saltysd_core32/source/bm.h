#ifndef BM_H
#define BM_H

// highly optimized data search, requires (256)+(4*patlen) stack 
// returns NULL if pat not found
void* boyer_moore_search(void *string, int stringlen, void *pat, int patlen);

#endif /* BM_H */
