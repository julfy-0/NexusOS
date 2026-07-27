#ifndef NEXUSOS_MEMINFO_H
#define NEXUSOS_MEMINFO_H

/* Показывает состояние своей paging-подсистемы (CR3, базовый диапазон
 * identity-map) и сводку по EFI memory map (kstate_mem_summary()).
 * Не блокирует, не выделяет память. */
void meminfo_run(void);

#endif
