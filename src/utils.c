#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "qq.h"
#include <pthread.h>
#include <stdatomic.h>

void display_message(const char *message, const char *color) {
    if (color) {
        printf("%s%s%s\n", color, message, COLOR_RESET);
    } else {
        printf("%s\n", message);
    }
}

void print_markdown(const char *text) {
    if (!text) return;

    const char *ptr = text;
    bool in_code_block = false;
    bool in_inline_code = false;
    bool in_bold = false;
    bool in_italic = false;
    bool in_list = false;
    bool in_header = false;
    bool in_table = false;
    bool in_table_header = false;
    int header_level = 0;
    int line_length = 0;
    const int max_line_length = 120; // Prevent excessive line wrapping

    while (*ptr) {
        // True only right after a source-text newline (or at the very
        // start) — gates list/header/rule detection so a bare "* " or "- "
        // mid-sentence (a bullet symbol, a minus sign, "3 * 4") can never
        // be mistaken for list/header/rule syntax the way it could before.
        // Without this, a bulleted list's leading "* " was being consumed
        // by the italic-toggle check below before the list check ever ran,
        // corrupting the color state for the rest of the line.
        bool at_line_start = (ptr == text) || (*(ptr - 1) == '\n');

        // Handle code blocks (```)
        if (strncmp(ptr, "```", 3) == 0) {
            if (!in_code_block) {
                printf("\n%s", COLOR_BLUE);
                printf("┌─ Code Block ──────────────────────────────────────────────┐\n");
                in_code_block = true;
                ptr += 3;
                while (*ptr && !isspace(*ptr) && *ptr != '\n') {
                    ptr++;
                }
                while (*ptr && isspace(*ptr) && *ptr != '\n') {
                    ptr++;
                }
                continue;
            } else {
                printf("└─────────────────────────────────────────────────────────────┘\n");
                printf("%s", COLOR_RESET);
                in_code_block = false;
                ptr += 3;
                continue;
            }
        }

        // Handle inline code (`)
        if (*ptr == '`' && !in_code_block) {
            if (!in_inline_code) {
                printf("%s", COLOR_BLUE);
                in_inline_code = true;
            } else {
                printf("%s", COLOR_RESET);
                in_inline_code = false;
            }
            ptr++;
            continue;
        }

        // Handle headers (#) — line-start only, so "issue #123" or "C#"
        // mid-sentence can't be mistaken for a header.
        if (at_line_start && *ptr == '#' && !in_code_block && !in_inline_code) {
            header_level = 0;
            const char *header_start = ptr;
            while (*ptr == '#') {
                header_level++;
                ptr++;
            }
            if (header_level <= 6 && *ptr && isspace(*ptr)) {
                printf("\n%s", COLOR_GREEN);
                for (int i = 0; i < header_level; i++) {
                    printf("#");
                }
                printf(" ");
                in_header = true;
                line_length = 0;
                continue;
            } else {
                ptr = header_start; // Reset if not a valid header
            }
        }

        // Handle lists (- or *) — line-start only.
        if (at_line_start && (*ptr == '-' || *ptr == '*') && !in_code_block && !in_inline_code && !in_header) {
            const char *list_start = ptr;
            ptr++;
            if (*ptr && isspace(*ptr)) {
                printf("\n%s• ", COLOR_BLUE);
                in_list = true;
                line_length = 0;
                continue;
            } else {
                ptr = list_start; // Reset if not a valid list
            }
        }

        // Handle numbered lists — line-start only, so "See section 3. Also"
        // mid-sentence can't be mistaken for a numbered list item.
        if (at_line_start && isdigit(*ptr) && !in_code_block && !in_inline_code && !in_header) {
            const char *num_start = ptr;
            int num = 0;
            while (isdigit(*ptr)) {
                num = num * 10 + (*ptr - '0');
                ptr++;
            }
            if (*ptr == '.' && *(ptr + 1) && isspace(*(ptr + 1))) {
                printf("\n%s%d. ", COLOR_BLUE, num);
                in_list = true;
                line_length = 0;
                ptr++;
                continue;
            } else {
                ptr = num_start; // Reset if not a valid numbered list
            }
        }

        // Handle horizontal rules (--- or ***) — line-start only.
        if (at_line_start && (*ptr == '-' || *ptr == '*') && !in_code_block && !in_inline_code) {
            const char *rule_start = ptr;
            char rule_char = *ptr;
            int rule_count = 0;
            while (*ptr == rule_char) {
                rule_count++;
                ptr++;
            }
            if (rule_count >= 3 && (*ptr == '\n' || *ptr == '\0')) {
                printf("\n%s", COLOR_BLUE);
                printf("─────────────────────────────────────────────────────────────\n");
                printf("%s", COLOR_RESET);
                line_length = 0;
                continue;
            } else {
                ptr = rule_start; // Reset if not a valid rule
            }
        }

        // Handle bold (**)
        if (strncmp(ptr, "**", 2) == 0 && !in_code_block && !in_inline_code) {
            if (!in_bold) {
                printf("%s", COLOR_YELLOW);
                in_bold = true;
            } else {
                printf("%s", COLOR_RESET);
                in_bold = false;
            }
            ptr += 2;
            continue;
        }

        // Handle italic (*)
        if (*ptr == '*' && !in_code_block && !in_inline_code && !in_bold) {
            if (!in_italic) {
                printf("%s", COLOR_ORANGE);
                in_italic = true;
            } else {
                printf("%s", COLOR_RESET);
                in_italic = false;
            }
            ptr++;
            continue;
        }

        // Handle table separators (|)
        if (*ptr == '|' && !in_code_block && !in_inline_code) {
            if (!in_table) {
                in_table = true;
                printf("%s", COLOR_CYAN);
            }
            printf("│");
            line_length++;
            ptr++;
            continue;
        }

        // Handle table header separators (|---|)
        if (strncmp(ptr, "|---", 4) == 0 && in_table) {
            in_table_header = true;
            printf("%s", COLOR_CYAN);
            printf("├─");
            while (*ptr == '-' || *ptr == '|') {
                if (*ptr == '|') {
                    printf("─┤");
                } else {
                    printf("─");
                }
                ptr++;
            }
            printf("%s", COLOR_RESET);
            line_length = 0;
            continue;
        }

        // Handle HTML-style breaks (<br>)
        if (strncmp(ptr, "<br>", 4) == 0 && !in_code_block) {
            printf("\n");
            line_length = 0;
            ptr += 4;
            continue;
        }

        // Handle line breaks
        if (*ptr == '\n') {
            if (in_list) {
                in_list = false;
            }
            if (in_header) {
                in_header = false;
            }
            if (in_table_header) {
                in_table_header = false;
            }
            if (in_table && !in_table_header) {
                in_table = false;
            }
            printf("\n");
            line_length = 0;
            ptr++;
            continue;
        }

        // Print the character with smart wrapping
        if (line_length >= max_line_length && *ptr == ' ' && !in_code_block) {
            printf("\n");
            line_length = 0;
        }

        printf("%c", *ptr);
        line_length++;
        ptr++;
    }

    // Reset any active formatting
    if (in_code_block || in_inline_code || in_bold || in_italic) {
        printf("%s", COLOR_RESET);
    }

    printf("\n");
}

// --- Concurrent progress animation (Dot Scanner Window) ---
static pthread_t g_progress_thread;
static atomic_int g_progress_active = 0;
static atomic_int g_progress_should_run = 0;
static char g_progress_message[1024];

static void* progress_animation_thread(void *arg) {
    (void)arg;
    static const char *DOTS_LIGHT[] = {"⠁","⠂","⠄","⡀","⢀","⠠","⠐","⠈"};
    static const int DOTS_LIGHT_N = 8;
    static const char *DOTS_DENSE[] = {"⠃","⠇","⡇","⣇","⣧","⣷","⣿","⣷","⣧","⣇","⡇","⠇","⠃"};
    static const int DOTS_DENSE_N = 13;

    const int left_width = 12;
    const int right_width = 12;
    int j = 0;
    while (atomic_load(&g_progress_should_run)) {
        int win = j % 16;
        printf("\r%s", COLOR_BLUE);
        for (int i = 0; i < left_width; i++) {
            if (i == win/2 || i == (win/2)+1) {
                printf("%s", DOTS_DENSE[(j + i) % DOTS_DENSE_N]);
            } else {
                printf("%s", DOTS_LIGHT[(j + i) % DOTS_LIGHT_N]);
            }
        }
        printf(" %s ", g_progress_message);
        for (int i = 0; i < right_width; i++) {
            if (i == (15 - win)/2 || i == (15 - win)/2 + 1) {
                printf("%s", DOTS_DENSE[(j + i * 2) % DOTS_DENSE_N]);
            } else {
                printf("%s", DOTS_LIGHT[(j + i * 2) % DOTS_LIGHT_N]);
            }
        }
        printf("%s\033[K", COLOR_RESET);
        fflush(stdout);
        usleep(85000);
        j++;
    }
    return NULL;
}

void start_progress_animation(const char *status_text, bool enable_animation) {
    if (!enable_animation) {
        return;
    }
    // The \r/ANSI-clear redraws only make sense on a real terminal — on a
    // pipe or redirect they'd flood output with every animation frame.
    if (!isatty(STDOUT_FILENO)) {
        return;
    }
    if (atomic_load(&g_progress_active)) {
        return; // already running
    }
    size_t len = strlen(status_text);
    if (len >= sizeof(g_progress_message)) len = sizeof(g_progress_message) - 1;
    memcpy(g_progress_message, status_text, len);
    g_progress_message[len] = '\0';

    atomic_store(&g_progress_should_run, 1);
    if (pthread_create(&g_progress_thread, NULL, progress_animation_thread, NULL) == 0) {
        atomic_store(&g_progress_active, 1);
    } else {
        atomic_store(&g_progress_should_run, 0);
    }
}

void stop_progress_animation(void) {
    if (!atomic_load(&g_progress_active)) {
        return;
    }
    atomic_store(&g_progress_should_run, 0);
    pthread_join(g_progress_thread, NULL);
    atomic_store(&g_progress_active, 0);
    printf("\r\033[K");
    fflush(stdout);
}

char* get_home_directory(void) {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }
    return home ? strdup_safe(home) : NULL;
}

// mkdir -p equivalent — CONFIG_DIR_NAME is two levels (".config/.qq"), and
// the parent (~/.config) won't already exist on a fresh account with no
// other apps having created it yet (found via real testing on a fresh
// root account on ubt-2202-build.ashnet — a plain single-level mkdir()
// failed with ENOENT there, though it worked on dev machines whose
// ~/.config already existed from unrelated apps).
bool create_directory_if_not_exists(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == 0) {
        return true; // already exists
    }

    size_t len = strlen(path);
    if (len == 0 || len >= MAX_PATH_LEN) {
        return false;
    }

    char tmp[MAX_PATH_LEN];
    strcpy(tmp, path);

    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (stat(tmp, &st) == -1 && mkdir(tmp, 0700) != 0) {
                return false;
            }
            tmp[i] = '/';
        }
    }

    return mkdir(tmp, 0700) == 0;
}

char* read_line(void) {
    char *line = readline("");
    if (line && *line) {
        add_history(line);
    }
    return line;
}

char* strdup_safe(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *dup = malloc(len);
    if (dup) {
        strcpy(dup, str);
    }
    return dup;
}

// HTTP utility functions
struct curl_slist* create_headers(void) {
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    return headers;
}

void free_headers(struct curl_slist *headers) {
    if (headers) {
        curl_slist_free_all(headers);
    }
}

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char **response_ptr = (char**)userp;

    size_t current_len = (*response_ptr) ? strlen(*response_ptr) : 0;
    char *new_response = realloc(*response_ptr, current_len + realsize + 1);
    if (!new_response) {
        return 0; // Out of memory
    }

    *response_ptr = new_response;
    memcpy(&(*response_ptr)[current_len], contents, realsize);
    (*response_ptr)[current_len + realsize] = 0;

    return realsize;
}

bool make_http_request(const char *url, const char *post_data,
                      struct curl_slist *headers, char **response) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    *response = malloc(1);
    (*response)[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Extended timeout for slower models
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    if (post_data) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        display_message("HTTP request failed: ", COLOR_RED);
        printf("%s\n", curl_easy_strerror(res));
        free(*response);
        *response = NULL;
        return false;
    }

    if (http_code < 200 || http_code >= 300) {
        display_message("HTTP request failed with status code: ", COLOR_RED);
        printf("%ld\n", http_code);
        if (*response) {
            display_message("Response: ", COLOR_RED);
            printf("%s\n", *response);
        }
        free(*response);
        *response = NULL;
        return false;
    }

    return true;
}

char* extract_response_from_json(json_object *json_obj) {
    json_object *message_obj, *content_obj;
    if (json_object_object_get_ex(json_obj, "message", &message_obj)) {
        if (json_object_object_get_ex(message_obj, "content", &content_obj)) {
            const char *content = json_object_get_string(content_obj);
            return strdup_safe(content);
        }
    }
    return NULL;
}

// Joins a set of tokens (not necessarily a contiguous argv range) with
// single spaces into one newly-allocated string.
char* join_positional_args(char **tokens, int count) {
    if (count == 0) return NULL;

    size_t total_len = 0;
    for (int j = 0; j < count; j++) {
        total_len += strlen(tokens[j]) + 1; // +1 for space/terminator
    }

    char *result = malloc(total_len + 1);
    if (!result) return NULL;

    result[0] = '\0';
    for (int j = 0; j < count; j++) {
        if (j > 0) {
            strcat(result, " ");
        }
        strcat(result, tokens[j]);
    }

    return result;
}

// Command line argument parsing.
// Only "--long" flags are recognized, never single-dash short forms — a
// single dash is far more likely to appear legitimately inside a typed
// question (e.g. "-i", a minus sign) than a literal "--word" is. Flags are
// recognized regardless of position; anything else is collected, in its
// original order, as the query text.
cli_args_t parse_arguments(int argc, char *argv[]) {
    cli_args_t args = {0};
    char **positional = malloc(sizeof(char*) * (size_t)(argc > 1 ? argc - 1 : 1));
    int positional_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--setup") == 0) {
            args.setup = true;
        } else if (strcmp(argv[i], "--model-change") == 0) {
            args.model_change = true;
        } else if (strcmp(argv[i], "--pretty") == 0) {
            args.pretty = true;
        } else if (strcmp(argv[i], "--stream") == 0) {
            args.stream = true;
        } else if (strcmp(argv[i], "--show-config") == 0) {
            args.show_config = true;
        } else if (strcmp(argv[i], "--delete-config") == 0) {
            args.delete_config = true;
        } else if (strcmp(argv[i], "--interactive") == 0) {
            args.interactive = true;
        } else if (strcmp(argv[i], "--version") == 0) {
            args.version = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            args.help = true;
        } else if (strcmp(argv[i], "--no-animation") == 0) {
            args.no_animation = true;
        } else {
            positional[positional_count++] = argv[i];
        }
    }

    args.query_text = join_positional_args(positional, positional_count);
    free(positional);

    return args;
}

void free_cli_args(cli_args_t *args) {
    if (args->query_text) {
        free(args->query_text);
        args->query_text = NULL;
    }
}
