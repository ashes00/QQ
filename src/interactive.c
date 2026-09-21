#include "qq.h"

static void print_interactive_help(void) {
    printf("\n");
    display_message("--- QQ Interactive Mode ---", COLOR_GREEN);
    printf("Type anything to send it as a query. Conversation history is kept\n");
    printf("for this session only (not saved between runs).\n\n");
    printf("Commands:\n");
    printf("  help          Show this message\n");
    printf("  exit, quit    End the session (Ctrl+D also works)\n\n");
}

// Sends `input` as a query with the given history and appends both to
// history (trimming the oldest pair first if full). Shared by the optional
// initial query and every line typed at the prompt. send_ollama_query
// prints the response itself (streamed or buffered/rendered per config).
static void process_turn(config_t *config, const char *input,
                          conversation_message_t *history, int *history_count,
                          int max_history_items) {
    if (*history_count >= max_history_items) {
        for (int i = 0; i < *history_count - 2; i++) {
            history[i] = history[i + 2];
        }
        *history_count -= 2;
    }

    char *response_text = send_ollama_query(config->ollama.server_address, config->ollama.model,
                    input, history, *history_count, config);

    if (!response_text) {
        return;
    }

    strcpy(history[*history_count].role, "user");
    strncpy(history[*history_count].content, input, MAX_RESPONSE_LEN - 1);
    history[*history_count].content[MAX_RESPONSE_LEN - 1] = '\0';
    (*history_count)++;

    strcpy(history[*history_count].role, "assistant");
    strncpy(history[*history_count].content, response_text, MAX_RESPONSE_LEN - 1);
    history[*history_count].content[MAX_RESPONSE_LEN - 1] = '\0';
    (*history_count)++;

    free(response_text);
}

bool start_interactive_session(config_t *config, const char *initial_query) {
    if (!config || strlen(config->ollama.server_address) == 0) {
        display_message("Ollama is not configured. Run `qq --setup` first.", COLOR_RED);
        return false;
    }

    display_message("QQ interactive mode — type 'help' for commands, 'exit' or Ctrl+D to quit.", COLOR_BLUE);

    int max_history_items = config->interactive_history_limit * 2;
    conversation_message_t *history = malloc((size_t)max_history_items * sizeof(conversation_message_t));
    if (!history) {
        display_message("Failed to allocate conversation history.", COLOR_RED);
        return false;
    }
    int history_count = 0;

    // The user's own text (typed or echoed) is yellow, so a long back-and-
    // forth is easy to scan for who said what; the LLM's response is left
    // in whatever color it already prints in (untouched by this).
    if (initial_query && strlen(initial_query) > 0) {
        printf("%s> %s%s%s\n", COLOR_BLUE, COLOR_YELLOW, initial_query, COLOR_RESET);
        process_turn(config, initial_query, history, &history_count, max_history_items);
    }

    while (true) {
        // Switch to yellow *before* reading input — the terminal renders
        // typed characters in whatever color was last set, so this is what
        // actually makes the user's input appear yellow as they type it.
        printf("%s> %s", COLOR_BLUE, COLOR_YELLOW);
        fflush(stdout);

        char *user_input = read_line();
        // Reset immediately, before any other output, so the response
        // (or a command's own output) never inherits yellow.
        printf("%s", COLOR_RESET);

        if (!user_input) {
            // EOF (Ctrl+D) — a normal way to end the session, not an error.
            printf("\n");
            break;
        }

        if (strcmp(user_input, "exit") == 0 || strcmp(user_input, "quit") == 0) {
            free(user_input);
            break;
        }

        if (strcmp(user_input, "help") == 0) {
            print_interactive_help();
            free(user_input);
            continue;
        }

        if (strlen(user_input) == 0) {
            free(user_input);
            continue;
        }

        process_turn(config, user_input, history, &history_count, max_history_items);
        free(user_input);
    }

    free(history);
    return true;
}
