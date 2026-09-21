#include "qq.h"
#include <signal.h>

// Async-signal-safe only: write()/_exit(), nothing that could touch stdio
// or join threads. SIGTSTP (Ctrl+Z) is handled the same as SIGINT
// (Ctrl+C) — there's no use case for backgrounding a one-shot query tool,
// and a suspended process left holding the binary open is exactly what
// broke a rebuild once already (see docs/RELEASE-NOTES.md v0.0.3).
static void handle_exit_signal(int sig) {
    static const char clear_line[] = "\r\033[K";
    write(STDOUT_FILENO, clear_line, sizeof(clear_line) - 1);
    _exit(128 + sig);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_exit_signal);
    signal(SIGTSTP, handle_exit_signal);

    curl_global_init(CURL_GLOBAL_ALL);

    cli_args_t args = parse_arguments(argc, argv);

    config_t *config = load_config();
    bool config_loaded = (config != NULL);
    if (!config) {
        config = get_default_config();
    }

    if (args.help) {
        print_help(config);
        free_config(config);
        free_cli_args(&args);
        curl_global_cleanup();
        return 0;
    }

    if (args.version) {
        print_version();
        free_config(config);
        free_cli_args(&args);
        curl_global_cleanup();
        return 0;
    }

    bool needs_setup = !config_loaded || strlen(config->ollama.server_address) == 0;

    if (needs_setup && !args.setup && !args.delete_config) {
        display_message("No Ollama configuration found. Running setup...", COLOR_YELLOW);
        if (!configure_ollama_service(config) || !save_config(config)) {
            display_message("Setup incomplete. Exiting.", COLOR_RED);
            free_config(config);
            free_cli_args(&args);
            curl_global_cleanup();
            return 1;
        }

        if (args.query_text == NULL) {
            display_message("Setup complete. Run qq again with a question.", COLOR_GREEN);
            free_config(config);
            free_cli_args(&args);
            curl_global_cleanup();
            return 0;
        }
    }

    if (args.setup) {
        bool ok = configure_ollama_service(config) && save_config(config);
        free_config(config);
        free_cli_args(&args);
        curl_global_cleanup();
        return ok ? 0 : 1;
    }

    if (args.delete_config) {
        if (delete_config_file()) {
            display_message("Configuration file deleted successfully.", COLOR_GREEN);
        } else {
            display_message("Failed to delete configuration file.", COLOR_RED);
        }
        free_config(config);
        free_cli_args(&args);
        curl_global_cleanup();
        return 0;
    }

    if (args.show_config) {
        show_active_configuration(config);
        free_config(config);
        free_cli_args(&args);
        curl_global_cleanup();
        return 0;
    }

    if (args.model_change) {
        bool ok = change_active_model(config);
        if (ok) {
            save_config(config);
        } else {
            display_message("Failed to change model.", COLOR_RED);
        }
        free_config(config);
        free_cli_args(&args);
        curl_global_cleanup();
        return ok ? 0 : 1;
    }

    if (args.pretty) {
        bool ok = toggle_pretty_output(config) && save_config(config);
        free_config(config);
        free_cli_args(&args);
        curl_global_cleanup();
        return ok ? 0 : 1;
    }

    if (args.stream) {
        bool ok = toggle_streaming(config) && save_config(config);
        free_config(config);
        free_cli_args(&args);
        curl_global_cleanup();
        return ok ? 0 : 1;
    }

    if (args.interactive) {
        bool ok = start_interactive_session(config, args.query_text);
        free_config(config);
        free_cli_args(&args);
        curl_global_cleanup();
        return ok ? 0 : 1;
    }

    if (args.query_text) {
        if (strlen(config->ollama.server_address) == 0) {
            display_message("Ollama server address not configured. Run `qq --setup` first.", COLOR_RED);
            free_config(config);
            free_cli_args(&args);
            curl_global_cleanup();
            return 1;
        }

        if (args.no_animation) {
            config->show_progress_animation = false;
        }

        // send_ollama_query prints the response itself (streamed or
        // buffered/rendered per config) — the return value here is unused
        // since a one-shot query has no history to append it to.
        char *response_text = send_ollama_query(config->ollama.server_address, config->ollama.model,
                        args.query_text, NULL, 0, config);

        int exit_code = 0;
        if (response_text) {
            free(response_text);
        } else {
            exit_code = 1;
        }

        free_config(config);
        free_cli_args(&args);
        curl_global_cleanup();
        return exit_code;
    }

    print_help(config);
    display_message("\nError: a question is required, e.g. qq \"What is the capital of France?\"", COLOR_RED);

    free_config(config);
    free_cli_args(&args);
    curl_global_cleanup();
    return 1;
}

void print_help(config_t *config) {
    printf("QQ: quick queries against a local-network Ollama instance.\n\n");
    printf("Usage: qq [OPTIONS] QUESTION\n\n");
    printf("Options:\n");
    printf("  --setup          Run/redo the Ollama server + model setup\n");
    printf("  --model-change   Change the active model\n");
    printf("  --pretty         Toggle colored/markdown-rendered output (saved to config)\n");
    printf("  --stream         Toggle token-by-token streaming (saved to config; ignored when pretty output is on)\n");
    printf("  --show-config    Show the current configuration\n");
    printf("  --delete-config  Delete the configuration file after confirmation\n");
    printf("  --interactive    Start a stateful chat session (conversation history kept for the session)\n");
    printf("  --no-animation   Disable the progress animation for this run\n");
    printf("  --version        Show version and exit\n");
    printf("  --help           Show this help message and exit\n\n");
    printf("Examples:\n");
    printf("  qq what is the capital of France\n");
    printf("  qq --setup\n");
    printf("  qq --model-change\n");
    printf("  qq --interactive\n");
    printf("  qq --interactive what is a cat\n\n");
    if (config && strlen(config->ollama.server_address) > 0) {
        printf("Config: %s (%s)  pretty=%s  stream=%s  animation=%s  history=%d\n",
               config->ollama.server_address, config->ollama.model,
               config->pretty_output ? "on" : "off",
               config->enable_streaming ? "on" : "off",
               config->show_progress_animation ? "on" : "off",
               config->interactive_history_limit);
    } else {
        printf("Config: not set up yet — run qq to configure\n");
    }
    printf("\n%sNOTE: --pretty overrides --stream — no visible streaming while pretty output is on.\n", COLOR_YELLOW);
    printf("To enable streaming, turn off pretty with qq --pretty%s\n", COLOR_RESET);
}

void print_version(void) {
    printf("%s version %s\n", QQ_APP_NAME, QQ_VERSION);
}
