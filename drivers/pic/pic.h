#ifndef NEXUSOS_PIC_H
#define NEXUSOS_PIC_H

#include <stdint.h>

#define PIC1_OFFSET 32   /* IRQ0..7  -> векторы 32..39 */
#define PIC2_OFFSET 40   /* IRQ8..15 -> векторы 40..47 */

void pic_remap(void);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq_line, int masked);

#endif
