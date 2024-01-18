void f0 (void *p, const void *q, int n)
{ 
  if (n > 0) return;
  __builtin_memcpy (p, q, n);
}

void f1 (void *p, const void *q, int n)
{
  if (n > 0) return;
  __builtin_memmove (p, q, n);
}

void f2 (char *p, const char *q, int n)
{
  if (n > 0) return;
  __builtin_strncpy (p, q, n);
}

void f3 (void *p, int n)
{ 
  if (n > 0) return;
  __builtin_memset (p, 0, n);
}

void* f4 (const void *p, int n)
{
  if (n > 0) return 0;
  return __builtin_memchr (p, 0, n);
}

int f5 (const void *p, const void *q, int n)
{
  if (n > 0) return 0;
  return __builtin_memcmp (p, q, n);
}