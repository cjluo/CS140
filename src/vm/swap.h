#ifndef VM_SWAP_H
#define VM_SWAP_H

void swap_table_init (void);
uint32_t write_to_swap (void *);
bool read_from_swap (uint32_t, void *);
void free_swap (uint32_t index);
#endif
