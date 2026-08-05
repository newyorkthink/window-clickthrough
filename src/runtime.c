#define _POSIX_C_SOURCE 200809L

#include "window-clickthrough.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static uint64_t fnv1a64(const char *text)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    const unsigned char *cursor = (const unsigned char *)text;

    while (*cursor != '\0') {
        hash ^= (uint64_t)*cursor++;
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

int build_runtime_paths(char *state_path,
                        size_t state_path_size,
                        char *lock_path,
                        size_t lock_path_size,
                        const char *display_name)
{
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    char fallback_dir[PATH_MAX];
    uint64_t display_hash = fnv1a64(display_name != NULL ? display_name : "");

    if (runtime_dir == NULL || runtime_dir[0] != '/' || access(runtime_dir, W_OK) != 0) {
        int length = snprintf(fallback_dir,
                              sizeof(fallback_dir),
                              "/tmp/%s-%lu",
                              PROGRAM_NAME,
                              (unsigned long)getuid());
        if (length < 0 || (size_t)length >= sizeof(fallback_dir)) {
            return -1;
        }

        if (mkdir(fallback_dir, 0700) != 0 && errno != EEXIST) {
            perror("mkdir");
            return -1;
        }

        struct stat directory_stat;
        if (lstat(fallback_dir, &directory_stat) != 0 ||
            !S_ISDIR(directory_stat.st_mode) ||
            directory_stat.st_uid != getuid()) {
            fprintf(stderr, "%s: unsafe fallback runtime directory\n", PROGRAM_NAME);
            return -1;
        }

        runtime_dir = fallback_dir;
    }

    int state_length = snprintf(state_path,
                                state_path_size,
                                "%s/%s-%016" PRIx64 ".state",
                                runtime_dir,
                                PROGRAM_NAME,
                                display_hash);
    int lock_length = snprintf(lock_path,
                               lock_path_size,
                               "%s/%s-%016" PRIx64 ".lock",
                               runtime_dir,
                               PROGRAM_NAME,
                               display_hash);

    if (state_length < 0 || (size_t)state_length >= state_path_size ||
        lock_length < 0 || (size_t)lock_length >= lock_path_size) {
        fprintf(stderr, "%s: runtime path is too long\n", PROGRAM_NAME);
        return -1;
    }

    return 0;
}

int lock_instance(const char *lock_path)
{
    int descriptor = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor < 0) {
        perror("open lock file");
        return -1;
    }

    if (flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
        if (errno == EWOULDBLOCK) {
            fprintf(stderr, "%s: another instance is already running\n", PROGRAM_NAME);
        } else {
            perror("flock");
        }
        close(descriptor);
        return -1;
    }

    return descriptor;
}

int generate_token(uint32_t token[TOKEN_WORDS])
{
    ssize_t result = getrandom(token, sizeof(uint32_t) * TOKEN_WORDS, 0);
    if (result == (ssize_t)(sizeof(uint32_t) * TOKEN_WORDS)) {
        return 0;
    }

    int descriptor = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (descriptor >= 0) {
        size_t offset = 0;
        unsigned char *buffer = (unsigned char *)token;

        while (offset < sizeof(uint32_t) * TOKEN_WORDS) {
            ssize_t bytes_read = read(descriptor,
                                      buffer + offset,
                                      sizeof(uint32_t) * TOKEN_WORDS - offset);
            if (bytes_read > 0) {
                offset += (size_t)bytes_read;
            } else if (bytes_read < 0 && errno == EINTR) {
                continue;
            } else {
                break;
            }
        }

        close(descriptor);
        if (offset == sizeof(uint32_t) * TOKEN_WORDS) {
            return 0;
        }
    }

    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return -1;
    }

    token[0] = (uint32_t)((uint64_t)now.tv_nsec ^ (uint64_t)getpid());
    token[1] = (uint32_t)((uint64_t)now.tv_sec ^ ((uint64_t)getuid() << 16));
    return 0;
}

int write_state_file(const char *state_path, const struct state_record *state)
{
    char temporary_path[PATH_MAX];
    int length = snprintf(temporary_path,
                          sizeof(temporary_path),
                          "%s.tmp.%ld",
                          state_path,
                          (long)getpid());
    if (length < 0 || (size_t)length >= sizeof(temporary_path)) {
        fprintf(stderr, "%s: temporary state path is too long\n", PROGRAM_NAME);
        return -1;
    }

    int descriptor = open(temporary_path,
                          O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC | O_NOFOLLOW,
                          0600);
    if (descriptor < 0) {
        perror("open state file");
        return -1;
    }

    char buffer[256];
    int content_length = snprintf(buffer,
                                  sizeof(buffer),
                                  "version=%d\n"
                                  "client=0x%lx\n"
                                  "frame=0x%lx\n"
                                  "token0=%" PRIu32 "\n"
                                  "token1=%" PRIu32 "\n",
                                  state->version,
                                  (unsigned long)state->client,
                                  (unsigned long)state->frame,
                                  state->token[0],
                                  state->token[1]);

    if (content_length < 0 || (size_t)content_length >= sizeof(buffer)) {
        close(descriptor);
        unlink(temporary_path);
        return -1;
    }

    size_t offset = 0;
    while (offset < (size_t)content_length) {
        ssize_t written = write(descriptor, buffer + offset, (size_t)content_length - offset);
        if (written > 0) {
            offset += (size_t)written;
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            perror("write state file");
            close(descriptor);
            unlink(temporary_path);
            return -1;
        }
    }

    if (fsync(descriptor) != 0) {
        perror("fsync state file");
        close(descriptor);
        unlink(temporary_path);
        return -1;
    }

    if (close(descriptor) != 0) {
        perror("close state file");
        unlink(temporary_path);
        return -1;
    }

    if (rename(temporary_path, state_path) != 0) {
        perror("rename state file");
        unlink(temporary_path);
        return -1;
    }

    return 0;
}

static int parse_unsigned_long(const char *text, unsigned long *value)
{
    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(text, &end, 0);

    if (errno != 0 || end == text || *end != '\0') {
        return -1;
    }

    *value = parsed;
    return 0;
}

int read_state_file(const char *state_path, struct state_record *state)
{
    FILE *stream = fopen(state_path, "r");
    if (stream == NULL) {
        return errno == ENOENT ? 0 : -1;
    }

    memset(state, 0, sizeof(*state));
    char line[128];
    unsigned fields = 0;

    while (fgets(line, sizeof(line), stream) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        char *separator = strchr(line, '=');
        if (separator == NULL) {
            continue;
        }

        *separator = '\0';
        const char *key = line;
        const char *value = separator + 1;
        unsigned long parsed = 0;

        if (strcmp(key, "version") == 0 && parse_unsigned_long(value, &parsed) == 0) {
            state->version = (int)parsed;
            fields |= 1U << 0;
        } else if (strcmp(key, "client") == 0 && parse_unsigned_long(value, &parsed) == 0) {
            state->client = (Window)parsed;
            fields |= 1U << 1;
        } else if (strcmp(key, "frame") == 0 && parse_unsigned_long(value, &parsed) == 0) {
            state->frame = (Window)parsed;
            fields |= 1U << 2;
        } else if (strcmp(key, "token0") == 0 && parse_unsigned_long(value, &parsed) == 0 &&
                   parsed <= UINT32_MAX) {
            state->token[0] = (uint32_t)parsed;
            fields |= 1U << 3;
        } else if (strcmp(key, "token1") == 0 && parse_unsigned_long(value, &parsed) == 0 &&
                   parsed <= UINT32_MAX) {
            state->token[1] = (uint32_t)parsed;
            fields |= 1U << 4;
        }
    }

    int read_error = ferror(stream);
    int close_result = fclose(stream);

    if (read_error || close_result != 0) {
        return -1;
    }

    if (fields != 0x1fU || state->version != STATE_VERSION || state->client == None) {
        return -1;
    }

    return 1;
}
