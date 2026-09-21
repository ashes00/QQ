#include "qq.h"

bool configure_ollama_service(config_t *config) {
    display_message("\n--- Ollama Service Setup ---", COLOR_GREEN);

    char server_address[MAX_SERVER_ADDR_LEN];
    char current_server[MAX_SERVER_ADDR_LEN];
    strcpy(current_server, config->ollama.server_address);

    if (strlen(current_server) > 0) {
        display_message("Current server address: ", COLOR_BLUE);
        printf("%s\n", current_server);
    }

    display_message("Enter Ollama server address (e.g., http://aisrv-01.ashnet:11434): ", COLOR_YELLOW);
    char *input = read_line();
    if (!input) {
        return false;
    }

    if (strlen(input) == 0 && strlen(current_server) > 0) {
        strcpy(server_address, current_server);
    } else if (strlen(input) > 0) {
        strcpy(server_address, input);

        // Add http:// if not present
        if (strncmp(server_address, "http://", 7) != 0 && strncmp(server_address, "https://", 8) != 0) {
            char temp[MAX_SERVER_ADDR_LEN];
            if (snprintf(temp, sizeof(temp), "http://%s", server_address) >= (int)sizeof(temp)) {
                display_message("Server address too long.", COLOR_RED);
                free(input);
                return false;
            }
            strcpy(server_address, temp);
        }
    } else {
        display_message("Server address cannot be empty.", COLOR_RED);
        free(input);
        return false;
    }
    free(input);

    // Test connection and fetch models
    char **models = NULL;
    int model_count = 0;
    if (!fetch_ollama_models(server_address, &models, &model_count)) {
        display_message("Failed to connect to Ollama server or fetch models.", COLOR_RED);
        return false;
    }

    if (model_count == 0) {
        display_message("No models found on the Ollama server.", COLOR_YELLOW);
        free(models);
        return false;
    }

    // Display available models
    display_message("\nAvailable models:", COLOR_GREEN);
    for (int i = 0; i < model_count; i++) {
        printf("  %d. %s\n", i + 1, models[i]);
    }

    // Select model
    display_message("\nEnter the number of the model to use: ", COLOR_YELLOW);
    char *model_choice = read_line();
    if (!model_choice) {
        for (int i = 0; i < model_count; i++) {
            free(models[i]);
        }
        free(models);
        return false;
    }

    int choice = atoi(model_choice);
    free(model_choice);

    if (choice < 1 || choice > model_count) {
        display_message("Invalid model selection.", COLOR_RED);
        for (int i = 0; i < model_count; i++) {
            free(models[i]);
        }
        free(models);
        return false;
    }

    strcpy(config->ollama.server_address, server_address);
    strcpy(config->ollama.model, models[choice - 1]);

    display_message("\nEnable pretty (colored/markdown) output? (y/N): ", COLOR_YELLOW);
    char *pretty_choice = read_line();
    if (pretty_choice) {
        config->pretty_output = (strlen(pretty_choice) > 0 &&
                                (pretty_choice[0] == 'y' || pretty_choice[0] == 'Y'));
        free(pretty_choice);
    }

    // Defaults to on (unlike pretty_output) — matches enable_streaming's
    // default for a fresh config, so leaving this blank doesn't silently
    // turn off a setting that's already on.
    display_message("Enable response streaming? (Y/n): ", COLOR_YELLOW);
    char *stream_choice = read_line();
    if (stream_choice) {
        config->enable_streaming = !(strlen(stream_choice) > 0 &&
                                (stream_choice[0] == 'n' || stream_choice[0] == 'N'));
        free(stream_choice);
    }

    display_message("Ollama service configured successfully!", COLOR_GREEN);

    for (int i = 0; i < model_count; i++) {
        free(models[i]);
    }
    free(models);

    return true;
}

bool change_active_model(config_t *config) {
    if (strlen(config->ollama.server_address) == 0) {
        display_message("Ollama is not configured yet. Run `qq --setup` first.", COLOR_YELLOW);
        return false;
    }

    char **models = NULL;
    int model_count = 0;
    if (!fetch_ollama_models(config->ollama.server_address, &models, &model_count)) {
        return false;
    }

    display_message("Available models:", COLOR_GREEN);
    for (int i = 0; i < model_count; i++) {
        bool is_current = (strcmp(models[i], config->ollama.model) == 0);
        printf("  %d. %s%s\n", i + 1, models[i], is_current ? " (current)" : "");
    }

    display_message("Enter model number: ", COLOR_YELLOW);
    char *choice = read_line();
    bool changed = false;
    if (choice) {
        int model_idx = atoi(choice) - 1;
        free(choice);

        if (model_idx >= 0 && model_idx < model_count) {
            strcpy(config->ollama.model, models[model_idx]);
            display_message("Model changed successfully.", COLOR_GREEN);
            changed = true;
        } else {
            display_message("Invalid model selection.", COLOR_RED);
        }
    }

    for (int i = 0; i < model_count; i++) {
        free(models[i]);
    }
    free(models);

    return changed;
}

bool toggle_pretty_output(config_t *config) {
    config->pretty_output = !config->pretty_output;
    display_message("Pretty output (color + markdown): ", COLOR_GREEN);
    printf("%s\n", config->pretty_output ? "Enabled" : "Disabled");
    return true;
}

bool toggle_streaming(config_t *config) {
    config->enable_streaming = !config->enable_streaming;
    display_message("Response streaming: ", COLOR_GREEN);
    printf("%s\n", config->enable_streaming ? "Enabled" : "Disabled");
    return true;
}

bool toggle_progress_animation(config_t *config) {
    config->show_progress_animation = !config->show_progress_animation;
    display_message("Progress animation: ", COLOR_GREEN);
    printf("%s\n", config->show_progress_animation ? "Enabled" : "Disabled");
    return true;
}

bool set_history_limit(config_t *config) {
    display_message("Current history limit: ", COLOR_BLUE);
    printf("%d\n", config->interactive_history_limit);

    display_message("Enter new history limit (1-50): ", COLOR_YELLOW);
    char *input = read_line();
    if (!input) {
        return false;
    }

    int new_limit = atoi(input);
    free(input);

    if (new_limit >= 1 && new_limit <= 50) {
        config->interactive_history_limit = new_limit;
        display_message("History limit updated successfully.", COLOR_GREEN);
        return true;
    } else {
        display_message("Invalid history limit. Must be between 1 and 50.", COLOR_RED);
        return false;
    }
}

void show_active_configuration(config_t *config) {
    display_message("\n--- Current Configuration ---", COLOR_GREEN);

    display_message("Interactive History Limit: ", COLOR_BLUE);
    printf("%d\n", config->interactive_history_limit);

    display_message("Response Streaming: ", COLOR_BLUE);
    printf("%s\n", config->enable_streaming ? "Enabled" : "Disabled");

    display_message("Progress Animation: ", COLOR_BLUE);
    printf("%s\n", config->show_progress_animation ? "Enabled" : "Disabled");

    display_message("Pretty Output: ", COLOR_BLUE);
    printf("%s\n", config->pretty_output ? "Enabled" : "Disabled");

    display_message("\nOllama Configuration:", COLOR_GREEN);
    display_message("  Server Address: ", COLOR_BLUE);
    printf("%s\n", strlen(config->ollama.server_address) > 0 ? config->ollama.server_address : "Not configured");
    display_message("  Model: ", COLOR_BLUE);
    printf("%s\n", strlen(config->ollama.model) > 0 ? config->ollama.model : "Not configured");
}
