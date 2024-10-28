#ifndef NDWM_UTILS_H
#define NDWM_UTILS_H

#include <stdio.h>

#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B)        ((A) <= (X) && (X) <= (B))
#define LENGTH(X)               (sizeof X / sizeof X[0])

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);

#endif
