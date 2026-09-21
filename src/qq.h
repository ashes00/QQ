#ifndef QQ_H
#define QQ_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "version.h"

// Configuration constants
#define CONFIG_DIR_NAME ".config/.qq"
#define CONFIG_FILE_NAME "qq.conf"
#define MAX_PATH_LEN 1024
#define MAX_RESPONSE_LEN 8192
#define MAX_MODEL_NAME_LEN 128
#define MAX_SERVER_ADDR_LEN 256

// Ollama API endpoints
#define OLLAMA_API_CHAT "/api/chat"
#define OLLAMA_API_TAGS "/api/tags"

// Color codes for terminal output
#define COLOR_RED     "\033[91m"
#define COLOR_GREEN   "\033[92m"
#define COLOR_YELLOW  "\033[93m"
#define COLOR_BLUE    "\033[94m"
#define COLOR_CYAN    "\033[96m"
#define COLOR_ORANGE  "\033[33m"
#define COLOR_RESET   "\033[0m"

// Structures
typedef struct {
    char server_address[MAX_SERVER_ADDR_LEN];
    char model[MAX_MODEL_NAME_LEN];
} ollama_config_t;

typedef struct {
    char role[16];  // "user" or "assistant"
    char content[MAX_RESPONSE_LEN];
} conversation_message_t;

typedef struct {
    ollama_config_t ollama;
    int interactive_history_limit;
    bool enable_streaming;
    bool show_progress_animation;
    bool pretty_output;    // colored + markdown-rendered output; default off (plain/pipe-friendly)
} config_t;

// Function prototypes

// Main functions
int main(int argc, char *argv[]);
void print_help(config_t *config);
void print_version(void);

// Configuration functions
char* get_config_path(void);
char* get_config_file_path(void);
config_t* load_config(void);
bool save_config(config_t *config);
config_t* get_default_config(void);
void free_config(config_t *config);
bool delete_config_file(void);

// Settings functions
bool configure_ollama_service(config_t *config);
bool change_active_model(config_t *config);
bool toggle_pretty_output(config_t *config);
bool toggle_streaming(config_t *config);
bool toggle_progress_animation(config_t *config);
bool set_history_limit(config_t *config);
void show_active_configuration(config_t *config);

// Ollama functions
bool fetch_ollama_models(const char *server_address, char ***models, int *model_count);
// Prints the response itself (streamed token-by-token, or buffered/rendered
// per config->pretty_output) — the returned text is for the caller's
// conversation history only, not for printing again.
char* send_ollama_query(const char *server_address, const char *model_name,
                      const char *user_query, conversation_message_t *history,
                      int history_count, config_t *config);
json_object* create_ollama_payload(const char *model, const char *query,
                                  conversation_message_t *history, int history_count,
                                  bool stream);

// Utility functions
void display_message(const char *message, const char *color);
void print_markdown(const char *text);
void start_progress_animation(const char *status_text, bool enable_animation);
void stop_progress_animation(void);
char* get_home_directory(void);
bool create_directory_if_not_exists(const char *path);
char* read_line(void);
char* strdup_safe(const char *str);
char* extract_response_from_json(json_object *json_obj);

// HTTP functions
struct curl_slist* create_headers(void);
void free_headers(struct curl_slist *headers);
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
bool make_http_request(const char *url, const char *post_data,
                      struct curl_slist *headers, char **response);

// Interactive mode. initial_query, if non-NULL/non-empty, is sent as the
// first turn before dropping into the prompt loop.
bool start_interactive_session(config_t *config, const char *initial_query);

// Command line parsing
typedef struct {
    bool setup;
    bool model_change;
    bool pretty;
    bool stream;
    bool show_config;
    bool delete_config;
    bool interactive;
    bool version;
    bool help;
    bool no_animation;
    char *query_text;
} cli_args_t;

char* join_positional_args(char **tokens, int count);
cli_args_t parse_arguments(int argc, char *argv[]);
void free_cli_args(cli_args_t *args);

#endif // QQ_H
