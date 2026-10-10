#include "unzip_name.h"

#include <stdio.h>
#include <string.h>

static int check(const char *name, int directory, int expected)
{
    int actual = unzip_name_validate(name, strlen(name), directory);
    if (actual != expected) {
        fprintf(stderr, "unexpected validation result for '%s': %d\n",
                name, actual);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += check("folder/file.txt", 0, 1);
    failures += check("café/例/пример.txt", 0, 1);
    failures += check("empty/", 1, 1);
    failures += check("/absolute", 0, 0);
    failures += check("../escape", 0, 0);
    failures += check("a/../escape", 0, 0);
    failures += check("a/./file", 0, 0);
    failures += check("a//file", 0, 0);
    failures += check("C:/drive", 0, 0);
    failures += check("a\\file", 0, 0);
    failures += check("a:stream", 0, 0);
    failures += check("CON.txt", 0, 0);
    failures += check("CONIN$.txt", 0, 0);
    failures += check("CONOUT$.txt", 0, 0);
    failures += check("a/Lpt1.dat", 0, 0);
    failures += check("a/trailing. ", 0, 0);
    failures += check("a/<invalid>", 0, 0);
    failures += check("dir/", 0, 0);
    failures += check("file", 1, 0);
    failures += check("\xc0\xaf", 0, 0);
    failures += check("\xed\xa0\x80", 0, 0);
    failures += check("\xf4\x90\x80\x80", 0, 0);
    {
        const char embedded_nul[] = { 'a', '\0', 'b' };
        if (unzip_name_validate(embedded_nul, sizeof(embedded_nul), 0)) {
            fputs("embedded NUL was accepted\n", stderr);
            ++failures;
        }
    }

    if (failures != 0) {
        return 1;
    }
    puts("unzip entry-name tests passed");
    return 0;
}
