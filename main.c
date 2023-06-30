#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "vm.h"

#define LINE_BUFFER_SIZE 1024

static void repl(struct vm *vm)
{
    char line[LINE_BUFFER_SIZE];
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        vm_interpret(vm, line);
    }
}

static char *readfile(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file %s\n", path);
        exit(EX_IOERR);
    }
    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    char *buffer = (char *)malloc(size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Could not allocate memory to read %s\n", path);
        exit(EX_IOERR);
    }
    size_t bytes_read = fread(buffer, sizeof(char), size, file);
    if (bytes_read < size) {
        fprintf(stderr, "Could not read file %s\n", path);
        exit(EX_IOERR);
    }
    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}

static int runfile(struct vm *vm, const char *path)
{
    char *source = readfile(path);
    int ret = vm_interpret(vm, source);
    free(source);
    return ret;
}

int main(int argc, char **argv)
{
    struct vm vm;
    int ret = vm_init(&vm);
    if (ret != 0) {
        fprintf(stderr, "Could not initialize vm: %d", ret);
    }

    if (argc == 1) {
        repl(&vm);
    } else if (argc == 2) {
        ret = runfile(&vm, argv[1]);
    } else {
        fprintf(stderr, "Usage: dplang [path]\n");
        exit(EX_USAGE);
    }

    if (vm_free(&vm) != 0) {
        fprintf(stderr, "Could not shut down vm: %d", ret);
    }
    return ret;
}
