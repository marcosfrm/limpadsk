# limpadsk

## Motivação

Sempre sanitizo dispositivos de bloco (ex. `/dev/sdx`) antes de restaurar imagens com o [FSArchiver](https://github.com/fdupoux/fsarchiver). `limpadsk` tem esse objetivo. Linka a libblkid (parte de suíte [util-linux](https://github.com/karelzak/util-linux)) e faz o seguinte:

* apaga assinaturas de:
  * sistemas de arquivos de todas as partições (máximo 128)
  * do dispositivo (offset 0), geralmente tabela de partições
* zera os primeiros e últimos 10 MiB (ou 1 MiB dependendo do tamanho), removendo restos de bootloaders
* _TRIMa_ o dispositivo (útil em SSDs, sem efeito em HDDs)

Profilaxia básica que [evita colisões](https://caixaseca.blogspot.com/2016/06/assinaturas.html) quando o dispositivo for novamente particionado e formatado.

Ahh, `limpadsk` é mais completo que `diskpart clean`, graças à libblkid.

## Gerando binário

#### Pacotes requeridos

Fedora/CentOS:

* `gcc`
* `libblkid-devel`

Debian/Ubuntu:

* `gcc`
* `libc-dev`
* `libblkid-dev`

#### Requerimentos durante a execução

* kernel >= 3.7: [requisição BLKZEROOUT](https://github.com/torvalds/linux/commit/66ba32dc167202c3cf8c86806581a9393ec7f488) da chamada de sistema `ioctl()`
* libblkid >= 2.29: funcionará com versões anteriores, mas assinaturas das partições não serão apagadas por causa [deste bug](https://github.com/karelzak/util-linux/commit/445e6b1ec82642a298419bdd18a81110593bfbaa)

#### Compilação

```
gcc limpadsk.c -o limpadsk -lblkid
```
## Uso

```
limpadsk /dev/dispositivo
```

**ATENÇÃO**: `limpadsk` escreverá zeros em áreas importantes do dispositivo especificado. Para fins práticos, **TODOS** os dados serão perdidos. Esta ferramenta é para ser usada por quem sabe o que está fazendo.
