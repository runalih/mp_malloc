#include <stdlib.h>

int main() {
  void *a = malloc(256);  // 256 == 0x 100 bytes
  void *b = malloc(128);
  void *c = malloc(256);
  void *d = malloc(256);
  void *e = malloc(128);
  /* Line 6 */
  free(a);
  free(c);
  free(e);
  /* Line 10 */
  void *r1 = malloc(10);
  void *r2 = malloc(10);
  void *r4 = malloc(180);
  void *r3 = malloc(200);
}
