#define MAX_LOG_LENGTH 4096

internal void log_printf(Log_Level level, const char *fmt, ...) {
    char buffer[MAX_LOG_LENGTH];

    // Initialize the buffer to empty
    buffer[0] = '\0';

    switch (level) {
        case LOG_LEVEL_INFO:
            strcat_s(buffer, MAX_LOG_LENGTH, "[INFO] ");
            break;

        case LOG_LEVEL_FATAL:
            strcat_s(buffer, MAX_LOG_LENGTH, "[FATAL] ");
            break;

        default: return;
    }

    va_list args;
    va_start(args, fmt);
    u64 buffer_length = strlen(buffer);
    vsnprintf_s(
        buffer + buffer_length,
        MAX_LOG_LENGTH - buffer_length,
        MAX_LOG_LENGTH - buffer_length - 1,
        fmt, args);
    va_end(args);

    fprintf(stderr, buffer, "\n");
}
