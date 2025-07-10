internal void log_printf(Log_Level level, const char *fmt, ...) {
    u64 max_length = 4096;
    char *buffer = new char[max_length];

    // Initialize the buffer to empty
    buffer[0] = '\0';

    switch (level) {
        case LOG_LEVEL_INFO:
            strcat_s(buffer, max_length, "[INFO] ");
            break;

        case LOG_LEVEL_FATAL:
            strcat_s(buffer, max_length, "[FATAL] ");
            break;

        default: return;
    }

    va_list args;
    va_start(args, fmt);
    u64 buffer_length = strlen(buffer);
    vsnprintf_s(
        buffer + buffer_length,
        max_length - buffer_length,
        max_length - buffer_length - 1,
        fmt, args);
    va_end(args);

    fprintf(stderr, buffer, "\n");

    delete[] buffer;
}
