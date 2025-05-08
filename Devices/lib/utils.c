#include <time.h>
#include <string.h>
#include "utils.h"
#include <math.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

void sleep_ms(int ms) {
    fflush(stdout); // Need to flush the stdout buffer to not delay the prints
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void sleep_s(int s) {
    sleep_ms(s * 1000);
}

/* generate a random double number from min to max */
double random_double(double min, double max) {
    // Make sure you have called srand(time(NULL));
    double range = (max - min); 
    double div = RAND_MAX / range;
    return round_to_precision(min + (rand() / div), 5);
}

uint8_t random_bit(void) {
    return rand() & 1;
}

uint8_t count_bits(size_t value) {
    uint8_t counter = 0;
    for(uint8_t i=0; i<(8*sizeof(size_t)); i++) {
        if(value & ((size_t)1<<i)) {
            counter++;
        }
    }
    return counter;
}

// Returns a random integer, min values is min, max value is max-1
uint32_t random_int(uint32_t min, uint32_t max) {
    uint32_t range = (max - min); 
    // uint32_t div = RAND_MAX / range;
    return min + (rand() % range);
}

// Generates a random vector of given length in an n-dimensional space
void gen_vector(uint32_t n, double len, double* result) {
    // Generate random components
    double sum_of_squares = 0.0;
    for (uint32_t i = 0; i < n; ++i) {
        result[i] = ((double)rand() / RAND_MAX) * 2.0 - 1.0; // Random number between -1 and 1
        sum_of_squares += result[i] * result[i];
    }

    // Normalize the vector to have unit length
    double norm = sqrt(sum_of_squares);
    for (uint32_t i = 0; i < n; ++i) {
        result[i] /= norm;
    }

    // Scale the vector to the desired length
    for (uint32_t i = 0; i < n; ++i) {
        result[i] *= len;
    }

    // Cut off the trailing digits
    for (uint32_t i = 0; i < n; ++i) {
        result[i] = round_to_precision(result[i], 5);
    }
}

// Precision is how many digits after the decimal point
double round_to_precision(double value, uint32_t precision) {
    double factor = pow(10.0, precision);
    return round(value*factor)/factor;
}

uint64_t get_hash(uint8_t *data, size_t size) {
    uint64_t hash = 5381;
    for(size_t i=0; i<size; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }
    return hash;
}

typedef struct {
    uint32_t size;  // bytes
    uint64_t hash;
    uint8_t data[0];
} stored_data_t;

int store_data(void *data, size_t size, char *filename) {
    stored_data_t data_struct = {
        .size = size,
        .hash = get_hash((uint8_t*)data, size)
    };

    FILE *file = fopen_no_matter_what(filename, "wb");
    fwrite(&data_struct, sizeof(stored_data_t), 1, file);
    fwrite((uint8_t *)data, 1, size, file);
    fclose(file);
    return EXIT_SUCCESS;
}

// Free the returned pointer after use
void *restore_data( char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("ERROR: Failed to open file %s\n", filename);
        exit(EXIT_FAILURE);
    }
    stored_data_t data_struct = {0};
    fread((uint8_t *)&data_struct, sizeof(stored_data_t), 1, file);
    uint8_t *data = calloc(data_struct.size, 1);
    size_t len_data = fread((uint8_t *)data, 1, data_struct.size, file);
    if(data_struct.hash != get_hash((uint8_t*)data, data_struct.size)) {
        printf("ERROR: File %s corrupted (hash check failed)\n", filename);
        exit(EXIT_FAILURE);
    }
    fclose(file);
    if(len_data != data_struct.size) {
        printf("ERROR: Failed reading file %s (%lld of %d bytes were read)\n", filename, len_data, data_struct.size);
        exit(EXIT_FAILURE);
    }
    return data;
}

// int store_data(void *data, size_t size, char *filename) {
//     uint64_t hash = get_hash((uint8_t*)data, size);
//     FILE *file = fopen(filename, "wb");
//     if (file == NULL) {
//         printf("ERROR: Failed to open file %s\n", filename);
//         return EXIT_FAILURE;
//     }
//     fwrite((uint8_t *)data, 1, size, file);
//     fwrite(&hash, sizeof(hash), 1, file);
//     fclose(file);
//     return EXIT_SUCCESS;
// }

// int restore_data(void *data, size_t size, char *filename) {
//     FILE *file = fopen(filename, "rb");
//     if (file == NULL) {
//         printf("ERROR: Failed to open file %s\n", filename);
//         return EXIT_FAILURE;
//     }
//     size_t len_data = fread((uint8_t *)data, 1, size, file);
//     uint64_t hash;
//     fread(&hash, sizeof(hash), 1, file);
//     if(hash != get_hash((uint8_t*)data, size)) {
//         printf("ERROR: File %s corrupted (hash check failed)\n", filename);
//         return EXIT_FAILURE;
//     }
//     fclose(file);
//     if(len_data != size) {
//         printf("ERROR: Failed reading file %s (%lld of %lld bytes were read)\n", filename, len_data, size);
//         return EXIT_FAILURE;
//     }
//     return EXIT_SUCCESS;
// }

/* Concats two strings and returns result. Don't forget to free the result buffer after use */
char *concat_strings(const char *s1, const char *s2) {
    size_t s1_length = strlen(s1);
    size_t s2_length = strlen(s2);
    size_t total_length = s1_length + s2_length;

    char *const str_buf = malloc(total_length + 1);
    if (str_buf == NULL) {
        fprintf(stderr, "Failed to allocate memory at %s: %d\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    strcpy(str_buf, s1);
    strcpy(str_buf + s1_length, s2);

    return str_buf;
}

void clear_buffer(char *buffer, uint32_t buffer_size) {
    for(uint32_t i=0; i<buffer_size; i++) {
        buffer[i] = 0;
    }
}

void write_buf_to_file(const char *buffer, const char *filename) {
    FILE *file = fopen(filename, "ab");
    if (file != NULL) {
        fprintf (file, "%s", buffer);
        fclose(file);
    } else {
        printf("File write error, here are coeffs: %s", buffer);
    }
}
