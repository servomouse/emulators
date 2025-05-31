#include "utils.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

// Function to create a directory recursively
int mkdir_recursive(const char *dir) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp) == -1) {
                if (errno != EEXIST) {
                    return -1;
                }
            }
            *p = '/';
        }
    }
    if (mkdir(tmp) == -1) {
        if (errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

// Function to open a file, creating any missing directories
FILE* fopen_no_matter_what(const char *filename, const char *access_mode) {
    char *dir_name = strdup(filename);
    char *last_slash = strrchr(dir_name, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        if (mkdir_recursive(dir_name) == -1) {
            free(dir_name);
            return NULL;
        }
    }
    free(dir_name);
    // Now let's try to open the file
    FILE *file = fopen(filename, access_mode);
    if (file == NULL) {
        printf("ERROR: Failed to open file %s\n", filename);
        exit(EXIT_FAILURE);
    }
}

int is_dir_exist(const char *folder)
{
    struct stat sb;

    if (stat(folder, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        // printf("The %s folder exists\n", folder);
        return 1;
    } else {
        // printf("The %s folder does not exist\n", folder);
        return 0;
    }
}

// Function to remove a directory recursively
int remove_directory(const char *dir) {
    DIR *dp;
    struct dirent *dirp;
    struct stat statbuf;
    printf("remove_directory: removing directory %s\n", dir);

    if ((dp = opendir(dir)) == NULL) {
        return -1;
    }

    while ((dirp = readdir(dp)) != NULL) {
        if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0) {
            continue;
        }

        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir, dirp->d_name);

        if (stat(filepath, &statbuf) == -1) {
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            if (remove_directory(filepath) == -1) {
                closedir(dp);
                return -1;
            }
        } else {
            if (remove(filepath) == -1) {
                closedir(dp);
                return -1;
            }
        }
    }

    closedir(dp);

    if (rmdir(dir) == -1) {
        return -1;
    }

    return 0;
}

// Usage_example:

// int main(int argc, char* argv[])
// {
//     if (is_dir_exist(BCKP_DIR_PATH)) {
//         printf("The %s folder exists\n", BCKP_DIR_PATH);
//     } else {
//         printf("The %s folder does not exist\n", BCKP_DIR_PATH);
//     }
//
//     FILE *file = fopen_no_matter_what(BCKP_DIR_PATH"/test/file/path/test.txt", "ab");
//     if (file != NULL) {
//         fprintf (file, "%s", "buffer");
//         fclose(file);
//     } else {
//         printf("File write error, here are coeffs: %s", "buffer");
//     }
// }
