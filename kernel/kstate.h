#ifndef NEXUSOS_KSTATE_H
#define NEXUSOS_KSTATE_H

#include "boot_info.h"

/* Вызывается один раз из kmain() сразу после получения boot_info. */
void kstate_set_boot_info(nexus_boot_info_t *bi);

/* Доступно отовсюду в ядре: приложениям (neofetch и т.д.), будущим
 * драйверам, аллокатору памяти. */
nexus_boot_info_t *kstate_get_boot_info(void);

/* Суммирует memory map: сколько всего страниц описано и сколько из них
 * помечены как EfiConventionalMemory ("свободная" память на момент
 * загрузки — сюда же попадёт LoaderCode/Data после ExitBootServices,
 * но мы их пока не считаем свободными, т.к. там ещё физически лежит
 * код/данные бутлоадера). */
void kstate_mem_summary(uint64_t *out_total_pages, uint64_t *out_conventional_pages);

#endif
