# limpadsk

## Motivação

Sempre sanitizo dispositivos de bloco (ex. `/dev/sdx`) antes de restaurar imagens com o [FSArchiver](https://github.com/fdupoux/fsarchiver). `limpadsk` tem esse objetivo. Linka a libblkid (parte de suíte [util-linux](https://github.com/karelzak/util-linux)) e faz o seguinte:

* apaga assinaturas de:
  * sistemas de arquivos de todas as partições (máximo 128)
  * do dispositivo (offset 0), geralmente tabela de partições
* zera os primeiros e últimos 20480 setores (ou 2048 dependendo do tamanho), removendo restos de bootloaders
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

* libblkid >= 2.29: funcionará com versões anteriores, mas assinaturas das partições não serão apagadas por causa [deste bug](https://github.com/karelzak/util-linux/commit/445e6b1ec82642a298419bdd18a81110593bfbaa)

#### Compilação

```
gcc limpadsk.c -o limpadsk -lblkid
```

Em [_releases_](https://github.com/marcosfrm/limpadsk/releases) disponibilizo compilação estática sempre com a última versão disponível da libblkid no momento da publicação.

## Uso

```
limpadsk /dev/dispositivo
```

**ATENÇÃO**: `limpadsk` escreverá zeros em áreas importantes do dispositivo especificado. Para fins práticos, **TODOS** os dados serão perdidos. Esta ferramenta é para ser usada por quem sabe o que está fazendo.
