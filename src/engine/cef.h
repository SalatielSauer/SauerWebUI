#pragma once

typedef enum {
	_POINTER,
	_CROSS,
	_HAND,
} c_cef_cursor_type_t;

typedef enum {
    CEF_KEYEVENT_RAWKEYDOWN,
    CEF_KEYEVENT_KEYUP,
    CEF_KEYEVENT_CHAR
} c_cef_keyevent_type_t;

typedef struct {
    c_cef_keyevent_type_t type;
    int key;         // SDL keysym.sym or equivalent
    int scancode;    // SDL keysym.scancode or equivalent
    int modifiers;   // SDL mod state or custom
    int is_system_key;
    int is_repeat;
    unsigned short character;            // For text input (UTF-16)
    unsigned short unmodified_character; // For text input (UTF-16)
} c_cef_key_event_t;

void cef_initialize(void* window_handle);
void cef_create_browser(void* window_handle, char* file);
void cef_do_message_loop_work();
void cef_shutdown();
void cef_cleanup();
int cef_execute_process(void* instance_or_argc_argv);

void* cef_get_browser();

void cef_browser_move_mouse(int x, int y);
void cef_browser_click_mouse(int button, bool mouse_up, int click_count);
void cef_browser_mouse_wheel(int delta_x, int delta_y);
void cef_browser_send_key_event(const c_cef_key_event_t* event);
void cef_browser_text_input(const char* str, int len);
void cef_browser_key_input(int code, bool isdown, int modstate);

extern bool cef_input_active;

void cef_run_javascript(char* code);

typedef void (*cef_js_result_callback_t)(const char* result);

void cef_set_js_result_callback(cef_js_result_callback_t callback);

extern void* cef_pixel_buffer;
extern int cef_pixel_width, cef_pixel_height;
extern bool cef_pixel_dirty;

void cef_browser_window_resize(int w, int h);

typedef void (*cef_browser_loaded_callback_t)(void);
void cef_set_browser_loaded_callback(cef_browser_loaded_callback_t cb);

typedef void (*cef_execident_result_callback_t)(const char* result, void* userdata);
typedef void (*cef_execident_callback_t)(const char* code, cef_execident_result_callback_t result_cb, void* userdata);
void cef_set_execident_callback(cef_execident_callback_t cb);
extern cef_execident_callback_t g_execident_callback;

typedef void (*cef_cursor_change_callback_t)(c_cef_cursor_type_t type);
void cef_set_cursor_change_callback(cef_cursor_change_callback_t cb);
extern c_cef_cursor_type_t g_cef_cursor_type; // current cursor type

typedef void (*cef_input_active_callback_t)(bool active);
extern cef_input_active_callback_t g_cef_input_active_callback;
void cef_set_input_active_callback(cef_input_active_callback_t cb);
