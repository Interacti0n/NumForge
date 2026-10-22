#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* No network access: act as a live Quick Tunnel, or fail as a bad backend. */
int main(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "tunnel") != 0) return 2;
    (void)fprintf(stderr, "https://numforge-test-%lu.trycloudflare.com\n", (unsigned long)GetCurrentProcessId());
    (void)fflush(stderr);
    Sleep(INFINITE);
    return 0;
}
