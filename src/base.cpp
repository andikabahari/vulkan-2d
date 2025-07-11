internal void log_printf(Log_Level level, const char *fmt, ...) {
    switch (level) {
        case LOG_LEVEL_DEBUG:
            fprintf(stderr, "[DEBUG] ");
            break;

        case LOG_LEVEL_INFO:
            fprintf(stderr, "[INFO] ");
            break;

        case LOG_LEVEL_WARNING:
            fprintf(stderr, "[WARNING] ");
            break;

        case LOG_LEVEL_ERROR:
            fprintf(stderr, "[ERROR] ");
            break;

        case LOG_LEVEL_FATAL:
            fprintf(stderr, "[FATAL] ");
            break;

        default: return;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}
