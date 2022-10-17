#define _FILE_OFFSET_BITS 64
// asprintf, basename
#define _GNU_SOURCE
// setores a apagar
#define NSETORES 4096

#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <glob.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int fd, setor_sz, status;
    int r = EXIT_FAILURE;
    uint64_t dev_sz, intervalo[2];
    struct stat sb;
    char *devname, *devglob;
    pid_t pid;

    if (argc != 2)
    {
        fprintf(stderr, "uso: %s dispositivo\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }

    if (stat(argv[1], &sb) != 0 || !S_ISBLK(sb.st_mode))
    {
        fprintf(stderr, "Dispositivo inexistente ou não de bloco.\n");
        exit(EXIT_FAILURE);
    }

    devname = realpath(argv[1], NULL);
    if (devname == NULL)
    {
        perror("realpath");
        exit(EXIT_FAILURE);
    }

    if (asprintf(&devglob, "%s*", devname) < 0)
    {
        perror("asprintf");
        goto fim1;
    }

    fd = open(devname, O_RDWR|O_EXCL);
    if (fd < 0)
    {
        perror("open");
        goto fim2;
    }

    // https://systemd.io/BLOCK_DEVICE_LOCKING/
    flock(fd, LOCK_EX);

    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        goto fim3;
    }

    if (pid == 0)
    {
        glob_t gl;

        gl.gl_offs = 3;
        glob(devglob, GLOB_DOOFFS|GLOB_NOSORT, NULL, &gl);
        gl.gl_pathv[0] = "wipefs";
        gl.gl_pathv[1] = "--all";
        gl.gl_pathv[2] = "--force";

        if (gl.gl_pathc > 0)
        {
            unsetenv("LOCK_BLOCK_DEVICE");
            execvp(gl.gl_pathv[0], gl.gl_pathv);
            perror(gl.gl_pathv[0]);
        }

        exit(EXIT_FAILURE);
    }

    while (waitpid(pid, &status, 0) < 0)
    {
        continue;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        fprintf(stderr, "Erro ao rodar wipefs: assinaturas possivelmente não apagadas.\n");
    }

    if (ioctl(fd, BLKSSZGET, &setor_sz))
    {
        perror("ioctl BLKSSZGET");
        goto fim3;
    }

    if (ioctl(fd, BLKGETSIZE64, &dev_sz))
    {
        perror("ioctl BLKGETSIZE64");
        goto fim3;
    }

    intervalo[0] = 0;
    intervalo[1] = NSETORES * setor_sz;

    if (dev_sz >= (2 * intervalo[1]))
    {
        printf("Zerando %" PRIu64 " bytes no início do dispositivo... ", intervalo[1]);
        fflush(stdout);
        r = ioctl(fd, BLKZEROOUT, &intervalo);
        printf("%s.\n", r ? "falha" : "sucesso");
        fflush(stdout);

        intervalo[0] = dev_sz - intervalo[1];

        printf("Zerando %" PRIu64 " bytes no fim do dispositivo (offset %" PRIu64 " bytes)... ",
               intervalo[1], intervalo[0]);
        fflush(stdout);
        r = ioctl(fd, BLKZEROOUT, &intervalo);
        printf("%s.\n", r ? "falha" : "sucesso");
        fflush(stdout);
    }
    else
    {
        fprintf(stderr, "Dispositivo pequeno demais: início e fim não zerados.\n");
    }

    intervalo[0] = 0;
    intervalo[1] = dev_sz;

    printf("TRIMando o dispositivo... ");
    fflush(stdout);
    r = ioctl(fd, BLKDISCARD, &intervalo);
    printf("%s.\n", r ? "sem suporte" : "sucesso");
    fflush(stdout);

    ioctl(fd, BLKRRPART);

    r = EXIT_SUCCESS;
fim3:
    close(fd);
fim2:
    free(devglob);
fim1:
    free(devname);

    return r;
}
