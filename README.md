# limpadsk

## Motivação

`limpadsk` faz o seguinte:

* roda o `wipefs` para apagar assinaturas de:
  * sistemas de arquivos de todas as partições
  * do dispositivo (offset 0), geralmente tabela de partições
* zera os primeiros e últimos 4096 setores, removendo restos de bootloaders
* _TRIMa_ o dispositivo (útil em SSDs, sem efeito em HDDs)

Profilaxia básica que [evita colisões](https://caixaseca.blogspot.com/2016/06/assinaturas.html) quando o dispositivo for novamente particionado e formatado.

`limpadsk` é mais completo que `diskpart clean`, graças à invocação do `wipefs`.

## Gerando binário

#### Pacotes requeridos

Fedora/CentOS:

* `gcc`

Debian/Ubuntu:

* `gcc`
* `libc-dev`

#### Requerimentos durante a execução

* kernel >= 3.7: [requisição BLKZEROOUT](https://github.com/torvalds/linux/commit/66ba32dc167202c3cf8c86806581a9393ec7f488) da chamada de sistema `ioctl()`.
* `wipefs`, programa da suíte [util-linux](https://github.com/util-linux/util-linux): caso não esteja presente, assinaturas não serão apagadas.

#### Compilação

```
gcc limpadsk.c -o limpadsk
```

## Uso

```
limpadsk dispositivo
```

Dispositivo pode ser `/dev/sdx` ou o link equivalente em `/dev/disk/by-id/`.

**ATENÇÃO**: `limpadsk` escreverá zeros em áreas importantes do dispositivo especificado. Para fins práticos, **TODOS** os dados serão perdidos. Esta ferramenta é para ser usada por quem sabe o que está fazendo.
