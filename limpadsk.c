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

int main(int argc, char *argv[])
{
    int fd, i, n, setor_sz, r1, r2;
    int num_setores = 10 * 2048;
    uint64_t dev_sz, intervalo[2];
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
                                                                                particoes[i].indice,
                                                                                particoes[i].tamanho);
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

    intervalo[0] = 0;
    intervalo[1] = num_setores * setor_sz;

    printf("Zerando %" PRIu64 " bytes no inicio do dispositivo\n", intervalo[1]);
    r1 = ioctl(fd, BLKZEROOUT, &intervalo);

    intervalo[0] = dev_sz - intervalo[1];

    printf("Zerando %" PRIu64 " bytes no fim do dispositivo (offset %" PRIu64 " bytes)\n",
                                                                                intervalo[1],
                                                                                intervalo[0]);
    r2 = ioctl(fd, BLKZEROOUT, &intervalo);

    // não garante setores zerados
    // sem efeito em HDDs
    intervalo[0] = 0;
    intervalo[1] = ULLONG_MAX;
    ioctl(fd, BLKZEROOUT, &intervalo);

    ioctl(fd, BLKRRPART);

    close(fd);

    return (r1 || r2);
}
