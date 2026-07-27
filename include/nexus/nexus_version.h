/* NexusOS: единственное место в КОДЕ, где живёт номер версии.
 *
 * "Источник истины" по-прежнему docs/STATUS.md (см. docs/VERSIONING.md) —
 * эти define'ы обязаны совпадать с тем, что там написано, и обновляться
 * вместе с ним. Раньше версии не было даже здесь: `version`, `neofetch`
 * и `uname` хардкодили ТРИ РАЗНЫЕ строки ("shell 0.2, kernel
 * 0.3-experimental" / "0.3-experimental" / "0.3-experimental"), ни одна
 * из которых не совпадала с реальной версией проекта — каждая обновлялась
 * (или нет) независимо. Теперь все три подключают этот заголовок. */
#ifndef NEXUSOS_NEXUS_VERSION_H
#define NEXUSOS_NEXUS_VERSION_H

#define NEXUS_VERSION_MAJOR    0
#define NEXUS_VERSION_MINOR    4
#define NEXUS_VERSION_PATCH    5
#define NEXUS_VERSION_CODENAME "memoria"

#define NEXUS_VERSION_STRING "0.4.5-memoria"

#endif
