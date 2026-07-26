# extras/c-practice/

Небольшая учебная библиотека на C (`strutils`, `dynarray`, `fileutils`,
`mathutils` + демо `main.c`), которая была найдена в `userdata/` и
`system/apps/` при последней ревизии проекта.

**Она не имеет отношения к NexusOS и не участвует в сборке ядра.**
Причина: этот код использует `<stdio.h>`, `<stdlib.h>`, `printf`,
`malloc`/`free` — обычную hosted-среду с libc. Ядро NexusOS собирается
с `-ffreestanding -nostdlib`, там принципиально нет malloc/printf (мы
сами пишем каждую функцию, которая обычно приезжает с libc). Смешать
это с ядром без переписывания с нуля под freestanding — нельзя, поэтому
я перенёс файлы сюда, а не удалил.

Если это была часть отдельного упражнения — можешь собрать её как
обычную программу под Linux:

```bash
gcc extras/c-practice/*.c -o practice_demo
./practice_demo
```
