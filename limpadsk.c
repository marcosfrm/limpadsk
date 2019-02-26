#define _FILE_OFFSET_BITS 64
// strverscmp
#define _GNU_SOURCE
// limite do Windows
#define MAXPARTS 128

#include <blkid/blkid.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Linux < 2.6.28
// https://github.com/torvalds/linux/commit/d30a2605be9d5132d95944916e8f578fcfe4f976
#ifndef BLKDISCARD
#define BLKDISCARD _IO(0x12,119)
#endif

int zera_setores(int fd, off_t inicio, size_t num_setores, int setor_sz)
{
    uint8_t *buffer;
    ssize_t temp;

    if (lseek(fd, inicio, SEEK_SET) >= 0)
    {
        buffer = calloc((size_t)setor_sz, sizeof(uint8_t));
        if (!buffer)
        {
            return -1;
        }

        while (num_setores)
        {
            temp = write(fd, buffer, (size_t)setor_sz);
            // escritas parciais (temp != setor_sz) são OK
            if (temp > 0)
            {
                num_setores--;
            }
            else if (temp < 0)
            {
                free(buffer);
                return -1;
            }
        }

        fsync(fd);
        free(buffer);
    }
    else
    {
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int fd, i, n, setor_sz, r1, r2;
    size_t num_setores = 10 * 2048;
    uint64_t dev_sz, trim[2];
    off_t offset_fim;
    struct stat sb;
    struct {
        int indice;
        blkid_loff_t inicio;
        blkid_loff_t tamanho;
    } particoes[MAXPARTS] = {{0}};
    const char *blkver = NULL;
    const char *blkdate = NULL;
    blkid_probe pr = NULL;
    blkid_partlist ls = NULL;
    blkid_partition par = NULL;

    if (argc != 2)
    {
        fprintf(stderr, "uso: %s /dev/dispositivo\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    if (fstat(fd, &sb) < 0)
    {
        perror("fstat");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (!S_ISBLK(sb.st_mode))
    {
        fprintf(stderr, "'%s' precisa ser dispositivo de bloco\n", argv[1]);
        close(fd);
        exit(EXIT_FAILURE);
    }

    // https://systemd.io/BLOCK_DEVICE_LOCKING.html
    flock(fd, LOCK_EX);

    // wipefs-like
    // -----------------------------------------------------
    pr = blkid_new_probe();
    if (pr && blkid_probe_set_device(pr, fd, 0, 0) == 0)
    {
        blkid_probe_enable_superblocks(pr, 1);
        blkid_probe_set_superblocks_flags(pr, BLKID_SUBLKS_MAGIC|BLKID_SUBLKS_BADCSUM);
        blkid_probe_enable_partitions(pr, 1);
        blkid_probe_set_partitions_flags(pr, BLKID_PARTS_MAGIC|BLKID_PARTS_FORCE_GPT);

        // https://github.com/karelzak/util-linux/commit/445e6b1ec82642a298419bdd18a81110593bfbaa
        blkid_get_library_version(&blkver, &blkdate);
        if (strverscmp(blkver, "2.29") > 0)
        {
            printf("Usando libblkid %s (%s)\n", blkver, blkdate);

            ls = blkid_probe_get_partitions(pr);
            if (ls)
            {
                n = blkid_partlist_numof_partitions(ls);
                if (n)
                {
                    for (i = 0; i < n && i < MAXPARTS; i++)
                    {
                        par = blkid_partlist_get_partition(ls, i);
                        particoes[i].indice = blkid_partition_get_partno(par);
                        // valor em setores de 512 bytes
                        particoes[i].inicio = blkid_partition_get_start(par) * 512;
                        particoes[i].tamanho = blkid_partition_get_size(par) * 512;
                    }
                }
            }
        }
        else
        {
            fprintf(stderr, "libblkid < 2.29, assinaturas nao serao removidas das particoes\n");
        }

        for (i = 0; i < MAXPARTS; i++)
        {
            // offset 0
            if (i == 0)
            {
                printf("Apagando assinatura(s) do inicio do dispositivo\n");

                while (blkid_do_probe(pr) == 0)
                {
                    blkid_do_wipe(pr, 0);
                }
            }

            // partições
            if (particoes[i].indice)
            {
                if (blkid_probe_set_device(pr, fd, particoes[i].inicio, particoes[i].tamanho) == 0)
                {
                    printf("Apagando assinatura(s) da particao %d (%" PRId64 " bytes)\n",
                           particoes[i].indice, particoes[i].tamanho);

                    while (blkid_do_probe(pr) == 0)
                    {
                        blkid_do_wipe(pr, 0);
                    }
                }
            }
        }

        blkid_free_probe(pr);
    }
    else
    {
        fprintf(stderr, "Falha ao inicializar libblkid, assinaturas nao serao removidas\n");
    }
    // -----------------------------------------------------

    if (ioctl(fd, BLKSSZGET, &setor_sz))
    {
        perror("ioctl BLKSSZGET");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (ioctl(fd, BLKGETSIZE64, &dev_sz))
    {
        perror("ioctl BLKGETSIZE64");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // < 20  MiB (setores de 512 bytes)
    // < 160 MiB (setores de 4 KiB)
    if (dev_sz < (2 * num_setores * setor_sz))
    {
        num_setores = 2048;
    }

    printf("Zerando %" PRId64 " bytes no inicio do dispositivo\n", num_setores * setor_sz);

    r1 = zera_setores(fd, 0, num_setores, setor_sz);

    offset_fim = dev_sz - (num_setores * setor_sz);

    printf("Zerando %" PRId64 " bytes no fim do dispositivo (offset %" PRId64 " bytes)\n",
           num_setores * setor_sz, offset_fim);

    r2 = zera_setores(fd, offset_fim, num_setores, setor_sz);

    // - TRIM não garante setores zerados
    // - sem efeito em HDDs
    trim[0] = 0;
    trim[1] = ULLONG_MAX;
    ioctl(fd, BLKDISCARD, &trim);

    ioctl(fd, BLKRRPART);

    close(fd);

    return (r1 || r2);
}
