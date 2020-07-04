#define _FILE_OFFSET_BITS 64
// asprintf, strverscmp
#define _GNU_SOURCE
// setores a apagar
#define NSETORES 4096

#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <errno.h>
#include <glob.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef BLKDISCARD
#define BLKDISCARD _IO(0x12,119)
#endif

#ifndef BLKZEROOUT
#define BLKZEROOUT _IO(0x12,127)
#endif

int main(int argc, char *argv[])
{
    int fd, setor_sz, r, status;
    uint64_t dev_sz, intervalo[2];
    struct stat sb;
    struct utsname ut;
    char *devglob;
    pid_t pid, w;

    r = uname(&ut);
    if (!r && strverscmp(ut.release, "3.7") < 0)
    {
        fprintf(stderr, "%s requer kernel >= 3.7\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (argc != 2)
    {
        fprintf(stderr, "uso: %s /dev/dispositivo\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fd = open(argv[1], O_RDWR|O_EXCL);
    if (fd < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    if (fstat(fd, &sb) < 0)
    {
        perror("fstat");
        exit(EXIT_FAILURE);
    }

    if (!S_ISBLK(sb.st_mode))
    {
        fprintf(stderr, "'%s' precisa ser dispositivo de bloco\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    // https://systemd.io/BLOCK_DEVICE_LOCKING/
    flock(fd, LOCK_EX);

    if (asprintf(&devglob, "%s%s", argv[1], "*") < 0)
    {
        perror("asprintf");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
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
            execvp("wipefs", gl.gl_pathv);
            fprintf(stderr, "Assinaturas nao apagadas: wipefs nao encontrado no PATH.\n");
        }

        exit(EXIT_SUCCESS);
    }

    do
    {
        w = waitpid(pid, &status, 0);
    }
    while (w < 0);

    if (ioctl(fd, BLKSSZGET, &setor_sz))
    {
        perror("ioctl BLKSSZGET");
        exit(EXIT_FAILURE);
    }

    if (ioctl(fd, BLKGETSIZE64, &dev_sz))
    {
        perror("ioctl BLKGETSIZE64");
        exit(EXIT_FAILURE);
    }

    intervalo[0] = 0;
    intervalo[1] = NSETORES * setor_sz;

    if (dev_sz >= (2 * intervalo[1]))
    {
        printf("Zerando %" PRIu64 " bytes no inicio do dispositivo... ", intervalo[1]);
        r = ioctl(fd, BLKZEROOUT, &intervalo);
        printf("%s.\n", r ? "falha" : "sucesso");

        intervalo[0] = dev_sz - intervalo[1];

        printf("Zerando %" PRIu64 " bytes no fim do dispositivo (offset %" PRIu64 " bytes)... ",
               intervalo[1], intervalo[0]);
        r = ioctl(fd, BLKZEROOUT, &intervalo);
        printf("%s.\n", r ? "falha" : "sucesso");
    }
    else
    {
        fprintf(stderr, "Dispositivo pequeno demais: inicio e fim nao zerados.\n");
    }

    intervalo[0] = 0;
    intervalo[1] = ULLONG_MAX;

    printf("TRIMando o dispositivo... ");
    r = ioctl(fd, BLKDISCARD, &intervalo);
    printf("%s.\n", r ? "sem suporte" : "sucesso");

    ioctl(fd, BLKRRPART);

    close(fd);

    return EXIT_SUCCESS;
}
