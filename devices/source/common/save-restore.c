
uint8_t register_file[REG_FILE_SIZE];

DLL_PREFIX
void module_save(char * PATH) {
    store_data(register_file, REG_FILE_SIZE, PATH);
}

DLL_PREFIX
void module_restore(char * PATH) {
    uint8_t *data = restore_data(PATH);
    for(uint32_t i=0; i<REG_FILE_SIZE; i++) {
        register_file[i] = data[i];
    }
}