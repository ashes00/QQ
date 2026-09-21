#include "qq.h"

bool fetch_ollama_models(const char *server_address, char ***models, int *model_count) {
    if (!server_address || !models || !model_count) {
        return false;
    }

    display_message("Fetching models from Ollama server...", COLOR_BLUE);

    char url[MAX_PATH_LEN];
    snprintf(url, sizeof(url), "%s%s", server_address, OLLAMA_API_TAGS);

    char *response = NULL;
    struct curl_slist *headers = create_headers();

    bool success = make_http_request(url, NULL, headers, &response);
    free_headers(headers);

    if (!success || !response) {
        display_message("Failed to fetch models from Ollama server.", COLOR_RED);
        return false;
    }

    json_object *json_obj = json_tokener_parse(response);
    free(response);

    if (!json_obj) {
        display_message("Failed to parse JSON response from Ollama server.", COLOR_RED);
        return false;
    }

    json_object *models_array;
    if (!json_object_object_get_ex(json_obj, "models", &models_array)) {
        display_message("Invalid response format from Ollama server.", COLOR_RED);
        json_object_put(json_obj);
        return false;
    }

    int array_length = json_object_array_length(models_array);
    *model_count = 0;
    *models = malloc(array_length * sizeof(char*));

    if (!*models) {
        json_object_put(json_obj);
        return false;
    }

    for (int i = 0; i < array_length; i++) {
        json_object *model_obj = json_object_array_get_idx(models_array, i);
        json_object *name_obj;

        if (json_object_object_get_ex(model_obj, "name", &name_obj)) {
            const char *model_name = json_object_get_string(name_obj);
            (*models)[*model_count] = strdup_safe(model_name);
            if ((*models)[*model_count]) {
                (*model_count)++;
            }
        }
    }

    json_object_put(json_obj);
    return true;
}

// --- Streaming ---
//
// Ollama's streaming API sends one newline-delimited JSON object per token
// (or small group of tokens), e.g. {"message":{"content":"foo"},"done":false}
// followed eventually by a final {"done":true,...} line. This context
// accumulates partial lines across curl callback invocations (a single
// callback call is not guaranteed to land on a line boundary), prints each
// token's content the moment it's parsed, and separately accumulates the
// full response text (for the caller's conversation history) using a
// doubling-capacity buffer — growing by exact-size realloc on every small
// chunk would mean far more reallocations than necessary for a response
// built out of many small tokens.
typedef struct {
    char *line_buffer;
    size_t line_buffer_len;
    char *full_response;
    size_t full_response_len;
    size_t full_response_cap;
    bool first_chunk_received;
} stream_ctx_t;

static bool stream_ctx_append(stream_ctx_t *ctx, const char *content, size_t len) {
    if (ctx->full_response_len + len + 1 > ctx->full_response_cap) {
        size_t new_cap = ctx->full_response_cap == 0 ? 256 : ctx->full_response_cap;
        while (new_cap < ctx->full_response_len + len + 1) {
            new_cap *= 2;
        }
        char *new_buf = realloc(ctx->full_response, new_cap);
        if (!new_buf) {
            return false;
        }
        ctx->full_response = new_buf;
        ctx->full_response_cap = new_cap;
    }
    memcpy(ctx->full_response + ctx->full_response_len, content, len);
    ctx->full_response_len += len;
    ctx->full_response[ctx->full_response_len] = '\0';
    return true;
}

static void stream_ctx_handle_line(stream_ctx_t *ctx, char *line) {
    if (*line == '\0') {
        return;
    }

    json_object *chunk = json_tokener_parse(line);
    if (!chunk) {
        return;
    }

    json_object *message_obj, *content_obj;
    if (json_object_object_get_ex(chunk, "message", &message_obj) &&
        json_object_object_get_ex(message_obj, "content", &content_obj)) {
        const char *content = json_object_get_string(content_obj);
        if (content && *content) {
            if (!ctx->first_chunk_received) {
                stop_progress_animation();
                ctx->first_chunk_received = true;
            }
            fputs(content, stdout);
            fflush(stdout);
            stream_ctx_append(ctx, content, strlen(content));
        }
    }

    json_object_put(chunk);
}

static size_t stream_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    stream_ctx_t *ctx = (stream_ctx_t *)userp;
    size_t realsize = size * nmemb;

    char *new_buf = realloc(ctx->line_buffer, ctx->line_buffer_len + realsize + 1);
    if (!new_buf) {
        return 0;
    }
    ctx->line_buffer = new_buf;
    memcpy(ctx->line_buffer + ctx->line_buffer_len, contents, realsize);
    ctx->line_buffer_len += realsize;
    ctx->line_buffer[ctx->line_buffer_len] = '\0';

    char *line_start = ctx->line_buffer;
    char *newline;
    while ((newline = memchr(line_start, '\n',
                    ctx->line_buffer_len - (size_t)(line_start - ctx->line_buffer))) != NULL) {
        *newline = '\0';
        stream_ctx_handle_line(ctx, line_start);
        line_start = newline + 1;
    }

    size_t remaining = ctx->line_buffer_len - (size_t)(line_start - ctx->line_buffer);
    memmove(ctx->line_buffer, line_start, remaining);
    ctx->line_buffer_len = remaining;
    ctx->line_buffer[ctx->line_buffer_len] = '\0';

    return realsize;
}

static char* send_ollama_query_streaming(const char *url, const char *json_string,
                                        struct curl_slist *headers) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        return NULL;
    }

    stream_ctx_t ctx = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L); // Streaming responses can take a while overall
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_string);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    // If nothing ever printed (error before the first token), the
    // animation is still running — stop it now so the error message below
    // doesn't get overwritten by the spinner's next redraw.
    if (!ctx.first_chunk_received) {
        stop_progress_animation();
    }
    free(ctx.line_buffer);

    if (res != CURLE_OK || http_code < 200 || http_code >= 300) {
        display_message("Failed to send query to Ollama server.", COLOR_RED);
        free(ctx.full_response);
        return NULL;
    }

    if (!ctx.full_response) {
        display_message("Failed to extract response from Ollama server.", COLOR_RED);
        return NULL;
    }

    printf("\n"); // streamed output has no trailing newline of its own
    return ctx.full_response;
}

char* send_ollama_query(const char *server_address, const char *model_name,
                      const char *user_query, conversation_message_t *history,
                      int history_count, config_t *config) {
    if (!server_address || !model_name || !user_query || !config) {
        return NULL;
    }

    // Streaming and markdown rendering don't mix: print_markdown needs the
    // full text to recognize structure (list bullets, headers, code
    // fences), so pretty mode always requests a single buffered response.
    bool do_stream = config->enable_streaming && !config->pretty_output;

    char status_message[MAX_RESPONSE_LEN];
    snprintf(status_message, sizeof(status_message),
             "Sending query to %s...", server_address);

    start_progress_animation(status_message, config->show_progress_animation);

    json_object *payload = create_ollama_payload(model_name, user_query, history, history_count, do_stream);
    if (!payload) {
        stop_progress_animation();
        display_message("Failed to create request payload.", COLOR_RED);
        return NULL;
    }

    const char *json_string = json_object_to_json_string(payload);

    char url[MAX_PATH_LEN];
    snprintf(url, sizeof(url), "%s%s", server_address, OLLAMA_API_CHAT);

    struct curl_slist *headers = create_headers();

    if (do_stream) {
        char *response_text = send_ollama_query_streaming(url, json_string, headers);
        free_headers(headers);
        json_object_put(payload);
        return response_text;
    }

    char *response = NULL;
    bool success = make_http_request(url, json_string, headers, &response);
    free_headers(headers);
    json_object_put(payload);

    if (!success || !response) {
        stop_progress_animation();
        display_message("Failed to send query to Ollama server.", COLOR_RED);
        return NULL;
    }

    json_object *response_obj = json_tokener_parse(response);
    free(response);

    if (!response_obj) {
        stop_progress_animation();
        display_message("Failed to parse response from Ollama server.", COLOR_RED);
        return NULL;
    }

    char *response_text = extract_response_from_json(response_obj);
    json_object_put(response_obj);

    stop_progress_animation();

    if (!response_text) {
        display_message("Failed to extract response from Ollama server.", COLOR_RED);
        return NULL;
    }

    if (config->pretty_output) {
        print_markdown(response_text);
    } else {
        printf("%s\n", response_text);
    }

    return response_text;
}

json_object* create_ollama_payload(const char *model, const char *query,
                                  conversation_message_t *history, int history_count,
                                  bool stream) {
    json_object *payload = json_object_new_object();
    if (!payload) {
        return NULL;
    }

    json_object_object_add(payload, "model", json_object_new_string(model));

    json_object *messages = json_object_new_array();
    if (!messages) {
        json_object_put(payload);
        return NULL;
    }

    for (int i = 0; i < history_count; i++) {
        json_object *message = json_object_new_object();
        if (!message) {
            continue;
        }

        const char *role = (strcmp(history[i].role, "assistant") == 0) ? "assistant" : "user";
        json_object_object_add(message, "role", json_object_new_string(role));
        json_object_object_add(message, "content", json_object_new_string(history[i].content));
        json_object_array_add(messages, message);
    }

    json_object *current_message = json_object_new_object();
    if (current_message) {
        json_object_object_add(current_message, "role", json_object_new_string("user"));
        json_object_object_add(current_message, "content", json_object_new_string(query));
        json_object_array_add(messages, current_message);
    }

    json_object_object_add(payload, "messages", messages);
    json_object_object_add(payload, "stream", json_object_new_boolean(stream));

    return payload;
}
