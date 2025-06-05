// Include after registers definition

DLL_PREFIX
void module_save(char * PATH) {
    store_data(&regs, sizeof(device_regs_t), PATH);
}

DLL_PREFIX
void module_restore(char * PATH) {
    uint8_t *data = restore_data(PATH);
    uint8_t *ptr = (uint8_t*)&regs;
    for(uint32_t i=0; i<sizeof(device_regs_t); i++) {
        ptr[i] = data[i];
    }
}