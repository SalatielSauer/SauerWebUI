#define NOMINMAX

#include "cef.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_message_router.h"
#include "SDL.h"

static CefRefPtr<CefBrowser> g_browser;

// main message router for handling browser-side javascript queries
static CefRefPtr<CefMessageRouterBrowserSide> g_message_router = nullptr;

// callback used by the engine to receive javascript execution results
static cef_js_result_callback_t g_js_result_callback = nullptr;
void cef_set_js_result_callback(cef_js_result_callback_t cb) {
    g_js_result_callback = cb;
}

// callback invoked when a page finishes loading in the browser
static cef_browser_loaded_callback_t g_browser_loaded_callback = nullptr;
void cef_set_browser_loaded_callback(cef_browser_loaded_callback_t cb) {
    g_browser_loaded_callback = cb;
}

// callback allowing the engine to handle 'execident' requests from javascript via cefQuery

cef_execident_callback_t g_execident_callback = nullptr;
void cef_set_execident_callback(cef_execident_callback_t cb) {
    g_execident_callback = cb;
}

// holds the current cef cursor type (updated on cursor changes)
c_cef_cursor_type_t g_cef_cursor_type = _POINTER;

// callback invoked when the mouse cursor changes in the browser
static cef_cursor_change_callback_t g_cursor_change_callback = nullptr;
void cef_set_cursor_change_callback(cef_cursor_change_callback_t cb) {
    g_cursor_change_callback = cb;
}

cef_input_active_callback_t g_cef_input_active_callback = nullptr;
void cef_set_input_active_callback(cef_input_active_callback_t cb) {
    g_cef_input_active_callback = cb;
}

// handles application-level callbacks and render process events for cef
class SimpleApp : public CefApp, public CefRenderProcessHandler {
public:

    // returns the render process handler for this app
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

    // called when the webkit subsystem is initialized; sets up the renderer-side message router
    void OnWebKitInitialized() override {
        CefMessageRouterConfig config;
        message_router_renderer_side_ = CefMessageRouterRendererSide::Create(config);
    }

    // called when a v8 (javascript) context is created; notifies the message router
    void OnContextCreated(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override
    {
        if (message_router_renderer_side_) {
            message_router_renderer_side_->OnContextCreated(browser, frame, context);
        }
    }

    // called when a v8 (javascript) context is released; notifies the message router
    void OnContextReleased(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override
    {
        if (message_router_renderer_side_) {
            message_router_renderer_side_->OnContextReleased(browser, frame, context);
        }
    }

    // handles process messages received from other processes via the message router
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefProcessId source_process,
        CefRefPtr<CefProcessMessage> message) override {
        if (message_router_renderer_side_ && message_router_renderer_side_->OnProcessMessageReceived(
            browser, frame, source_process, message)) {
            return true;
        }
        return false;
    }

private:
    // message router for handling javascript queries on the renderer side
    CefRefPtr<CefMessageRouterRendererSide> message_router_renderer_side_;
    IMPLEMENT_REFCOUNTING(SimpleApp);
};


void* cef_pixel_buffer = nullptr;
int cef_pixel_width = 0;
int cef_pixel_height = 0;
bool cef_pixel_dirty = false; // set to true when the pixel buffer needs to be updated

// current view width and height for the browser rendering area
int cef_view_width, cef_view_height;

// handles cef client callbacks for browser events, rendering, messaging etc
class SimpleHandler : public CefClient, public CefLifeSpanHandler, public CefMessageRouterBrowserSide::Handler, public CefDisplayHandler, public CefRenderHandler, public CefLoadHandler {
public:
    // constructor
    SimpleHandler() {}

    IMPLEMENT_REFCOUNTING(SimpleHandler);

    // returns this as the lifespan handler for browser life cycle events
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    // returns this as the display handler for display-related browser events
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    // returns this as the load handler for loading events
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    // returns this as the render handler for offscreen rendering
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

    // handles messages received from other cef processes via the message router
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefProcessId source_process,
        CefRefPtr<CefProcessMessage> message) override {
        if (g_message_router)
            return g_message_router->OnProcessMessageReceived(browser, frame, source_process, message);
        return false;
    }

    // called after the browser is created; sets the global browser reference
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        g_browser = browser;
        g_browser->GetHost()->SetFocus(true);
    }

    // called before the browser is closed; notifies the message router
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        if (g_message_router)
            g_message_router->OnBeforeClose(browser);
    }

    bool OnBeforePopup(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        int popup_id,
        const CefString& target_url,
        const CefString& target_frame_name,
        WindowOpenDisposition target_disposition,
        bool user_gesture,
        const CefPopupFeatures& popupFeatures,
        CefWindowInfo& windowInfo,
        CefRefPtr<CefClient>& client,
        CefBrowserSettings& settings,
        CefRefPtr<CefDictionaryValue>& extra_info,
        bool* no_javascript_access
    ) override
    {
        // Pass the URL to your JS handler (using target_url)
        std::string js = "window.handleTargetBlank && window.handleTargetBlank(\"" + target_url.ToString() + "\");";
        browser->GetMainFrame()->ExecuteJavaScript(js, browser->GetMainFrame()->GetURL(), 0);

        return true; // Cancel popup
    }


    // handles javascript queries sent from the renderer process via cefQuery
    bool OnQuery(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        int64_t query_id,
        const CefString& request,
        bool persistent,
        CefRefPtr<Callback> callback) override
    {
        std::string req = request;
        // handles a javascript result message
        if (req.rfind("myFunctionResult:", 0) == 0) {
            std::string result = req.substr(strlen("myFunctionResult:"));
            //printf("JS Result: %s\n", result.c_str());
            if (g_js_result_callback) g_js_result_callback(result.c_str());
            callback->Success("OK");
            return true;
        }
        // handles cubescript commands from javascript
        /*if (req.rfind("cubescript:", 0) == 0) {
            std::string cmd = req.substr(strlen("cubescript:"));
            if (g_execident_callback) g_execident_callback(cmd.c_str());
            callback->Success("OK");
            return true;
        }*/
        // handles cubescript commands from javascript
        if (req.rfind("cubescript:", 0) == 0) {
            std::string cmd = req.substr(strlen("cubescript:"));
            if (g_execident_callback) {
                // Create a copy of the callback pointer so it survives until result_cb is called.
                CefRefPtr<Callback>* callback_copy = new CefRefPtr<Callback>(callback);

                // Use a static function pointer, not a lambda.
                g_execident_callback(
                    cmd.c_str(),
                    // Result callback function (static, not a lambda)
                    [](const char* result, void* userdata) {
                        CefRefPtr<Callback>* cbptr = static_cast<CefRefPtr<Callback>*>(userdata);
                        if (cbptr && *cbptr) {
                            (*cbptr)->Success(result ? result : "");
                        }
                        delete cbptr; // Prevent leak
                    },
                    callback_copy
                );
            }
            else {
                callback->Failure(-1, "No execident callback set");
            }
            return true;
        }

        // workaround to determine if we are editing a text field
        if (req.rfind("cef_input:", 0) == 0) {
            bool active = req == "cef_input:1";
            cef_input_active = active;
            if (g_cef_input_active_callback) g_cef_input_active_callback(active);
            callback->Success("OK");
            return true;
        }

        if (req == "openDevTools") {
            browser->GetHost()->ShowDevTools(
                CefWindowInfo(),      // Default popup
                nullptr,              // No custom client
                CefBrowserSettings(), // Default settings
                CefPoint()            // Default position
            );
            callback->Success("OK");
            return true;
        }


        return false;
    }

    // handles console messages from javascript; forwards to callback
    bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
        cef_log_severity_t level,
        const CefString& message,
        const CefString& source,
        int line) override {
        if (g_js_result_callback) {
            std::string msg = message.ToString();
            g_js_result_callback(msg.c_str());
        }
        return false; // allow normal logging
    }

    // called when a page load completes; notifies the engine if it is the main frame
    void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override {
        if (frame->IsMain()) {
            g_browser_loaded_callback();
        }
    }

    // called when the browser's mouse cursor changes; updates and notifies the engine
    bool OnCursorChange(CefRefPtr<CefBrowser> browser,
        CefCursorHandle cursor,
        cef_cursor_type_t type,
        const CefCursorInfo& custom_cursor_info) override
    {
        g_cef_cursor_type = (c_cef_cursor_type_t)type;
        if (g_cursor_change_callback) g_cursor_change_callback((c_cef_cursor_type_t)type);
        return false;
    }

    // provides the view rectangle dimensions for offscreen rendering
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
        rect = CefRect(0, 0, cef_view_width, cef_view_height);
    }

    // called when a new frame is painted; updates the pixel buffer with the new image data
    void OnPaint(CefRefPtr<CefBrowser> browser,
        PaintElementType type,
        const RectList& dirtyRects,
        const void* buffer,
        int width, int height) override
    {
        // resize the pixel buffer if needed and copy the latest frame pixels
        if (!cef_pixel_buffer || cef_pixel_width != width || cef_pixel_height != height) {
            free(cef_pixel_buffer);
            cef_pixel_buffer = malloc(width * height * 4); // 4 bytes per pixel
            cef_pixel_width = width;
            cef_pixel_height = height;
        }
        memcpy(cef_pixel_buffer, buffer, width * height * 4);
        cef_pixel_dirty = true; // mark buffer as dirty for the renderer to update
    }
};

static CefRefPtr<SimpleHandler> g_handler;

static int last_cef_mouse_x = 0, last_cef_mouse_y = 0;
// moves the mouse cursor within the browser to the given x and y coordinates
void cef_browser_move_mouse(int x, int y)
{
    last_cef_mouse_x = x;
    last_cef_mouse_y = y;

    if (!g_browser) return;
    CefMouseEvent mouse_event;
    mouse_event.x = x;
    mouse_event.y = y;
    mouse_event.modifiers = 0;
    g_browser->GetHost()->SendMouseMoveEvent(mouse_event, false);
}

// converts an sdl mouse button code to the equivalent cef mouse button type
CefBrowserHost::MouseButtonType sdl_button_to_cef(int button) {
    switch (button) {
    case 1: return MBT_LEFT;   // sdl left mouse button
    case 2: return MBT_MIDDLE; // sdl middle mouse button
    case 3: return MBT_RIGHT;  // sdl right mouse button
    default: return MBT_LEFT;  // fallback to left button
    }
}

// sends a mouse button click event to the browser at the last known mouse position
void cef_browser_click_mouse(int button, bool mouse_up, int click_count)
{
    if (!g_browser) return;

    CefMouseEvent mouse_event;
    mouse_event.x = last_cef_mouse_x;
    mouse_event.y = last_cef_mouse_y;
    mouse_event.modifiers = 0;

    CefBrowserHost::MouseButtonType cefbtn = sdl_button_to_cef(button);

    g_browser->GetHost()->SendMouseClickEvent(mouse_event, cefbtn, mouse_up, click_count);
}

// sends a mouse wheel event to the browser at the last known mouse position
void cef_browser_mouse_wheel(int delta_x, int delta_y)
{
    if (!g_browser) return;

    CefMouseEvent mouse_event;
    mouse_event.x = last_cef_mouse_x;
    mouse_event.y = last_cef_mouse_y;
    mouse_event.modifiers = 0;

    g_browser->GetHost()->SendMouseWheelEvent(mouse_event, delta_x, delta_y);
}

void cef_browser_send_key_event(const c_cef_key_event_t* event)
{
    if (!g_browser) return;

    CefKeyEvent cef_event;
    switch (event->type) {
    case CEF_KEYEVENT_RAWKEYDOWN: cef_event.type = KEYEVENT_RAWKEYDOWN; break;
    case CEF_KEYEVENT_KEYUP:      cef_event.type = KEYEVENT_KEYUP; break;
    case CEF_KEYEVENT_CHAR:       cef_event.type = KEYEVENT_CHAR; break;
    default: return;
    }
    cef_event.windows_key_code = event->key;
    cef_event.native_key_code = event->scancode;
    cef_event.modifiers = event->modifiers;
    cef_event.is_system_key = (event->is_system_key != 0);
    cef_event.character = event->character;
    cef_event.unmodified_character = event->unmodified_character;
    // Focus and other fields can be filled as needed

    g_browser->GetHost()->SendKeyEvent(cef_event);
}

int utf8_nextcodepoint(const char* s, uint32_t* cp) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) { *cp = c; return 1; }
    else if ((c & 0xE0) == 0xC0) { *cp = ((c & 0x1F) << 6) | (s[1] & 0x3F); return 2; }
    else if ((c & 0xF0) == 0xE0) { *cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); return 3; }
    else if ((c & 0xF8) == 0xF0) { *cp = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); return 4; }
    else return 1;
}


bool cef_input_active = true;
/*void cef_browser_text_input(const char* str, int len)
{
    if (!cef_input_active) return;
    if (g_browser) g_browser->GetHost()->SetFocus(true);

    int i = 0;
    while (i < len) {
        uint32_t codepoint = 0;
        int consumed = utf8_nextcodepoint(str + i, &codepoint);
        if (consumed <= 0) break;
        c_cef_key_event_t ev = {};
        ev.type = CEF_KEYEVENT_CHAR;
        ev.character = codepoint;
        ev.unmodified_character = codepoint;
        cef_browser_send_key_event(&ev);
        i += consumed;
        char dbg[128];
        snprintf(dbg, sizeof(dbg), "CHAR INPUT: 0x%X '%c'\n", codepoint, (char)codepoint);
        OutputDebugStringA(dbg);
    }
}*/

/*
Ideally, we would just send the main events from sdl and cef would take care of the rest,
but it doesn't seem to want to do that for text-related events, so we're brute-forcing it: we take the key sent
from SDL and send it to JavaScript, where we intercept it and implement the actions manually.
*/

void cef_browser_text_input(const char* str, int len)
{
    if (!cef_input_active) return;
    if (!g_browser) return;
    g_browser->GetHost()->SetFocus(true);

    int i = 0;
    while (i < len) {
        uint32_t codepoint = 0;
        int consumed = utf8_nextcodepoint(str + i, &codepoint);
        if (consumed <= 0) break;

        char jsbuf[128];
        if (codepoint >= 32 && codepoint <= 126) {
            snprintf(jsbuf, sizeof(jsbuf), "window._cef_insert_character('%c');", (char)codepoint);
        }
        else {
            snprintf(jsbuf, sizeof(jsbuf), "window._cef_insert_character('\\u%04x');", codepoint);
        }

        g_browser->GetMainFrame()->ExecuteJavaScript(jsbuf, "", 0);

        i += consumed;
    }
}


const char* SDLKeyToJSKeyName(int sdlkey)
{
    // letters
    if (sdlkey >= SDLK_a && sdlkey <= SDLK_z) {
        static char key[2];
        key[0] = 'a' + (sdlkey - SDLK_a);
        key[1] = '\0';
        return key;
    }
    // numbers (top row)
    if (sdlkey >= SDLK_0 && sdlkey <= SDLK_9) {
        static char key[2];
        key[0] = '0' + (sdlkey - SDLK_0);
        key[1] = '\0';
        return key;
    }
    // numpad numbers
    if (sdlkey >= SDLK_KP_0 && sdlkey <= SDLK_KP_9) {
        static char key[2];
        key[0] = '0' + (sdlkey - SDLK_KP_0);
        key[1] = '\0';
        return key;
    }

    switch (sdlkey) {
    case SDLK_BACKSPACE: return "Backspace";
    case SDLK_TAB:       return "Tab";
    case SDLK_RETURN:    return "Enter";
    case SDLK_RETURN2:   return "Enter";
    case SDLK_KP_ENTER:  return "Enter";
    case SDLK_ESCAPE:    return "Escape";
    case SDLK_SPACE:     return " ";
    case SDLK_QUOTE:     return "'";
    case SDLK_COMMA:     return ",";
    case SDLK_MINUS:     return "-";
    case SDLK_PERIOD:    return ".";
    case SDLK_SLASH:     return "/";
    case SDLK_SEMICOLON: return ";";
    case SDLK_EQUALS:    return "=";
    case SDLK_LEFTBRACKET:  return "[";
    case SDLK_BACKSLASH:    return "\\";
    case SDLK_RIGHTBRACKET: return "]";
    case SDLK_BACKQUOTE:        return "`";
        // arrow keys
    case SDLK_LEFT:  return "ArrowLeft";
    case SDLK_RIGHT: return "ArrowRight";
    case SDLK_UP:    return "ArrowUp";
    case SDLK_DOWN:  return "ArrowDown";
        // function keys
    case SDLK_F1: return "F1"; case SDLK_F2: return "F2";
    case SDLK_F3: return "F3"; case SDLK_F4: return "F4";
    case SDLK_F5: return "F5"; case SDLK_F6: return "F6";
    case SDLK_F7: return "F7"; case SDLK_F8: return "F8";
    case SDLK_F9: return "F9"; case SDLK_F10: return "F10";
    case SDLK_F11: return "F11"; case SDLK_F12: return "F12";
        // other useful keys
    case SDLK_DELETE:   return "Delete";
    case SDLK_INSERT:   return "Insert";
    case SDLK_HOME:     return "Home";
    case SDLK_END:      return "End";
    case SDLK_PAGEUP:   return "PageUp";
    case SDLK_PAGEDOWN: return "PageDown";
    case SDLK_CAPSLOCK: return "CapsLock";
    case SDLK_SCROLLLOCK: return "ScrollLock";
    case SDLK_PRINTSCREEN: return "PrintScreen";
    case SDLK_PAUSE:      return "Pause";
    case SDLK_LSHIFT: return "Shift";
    case SDLK_RSHIFT: return "Shift";
    case SDLK_LCTRL:  return "Control";
    case SDLK_RCTRL:  return "Control";
    case SDLK_LALT:   return "Alt";
    case SDLK_RALT:   return "Alt";
    case SDLK_LGUI:   return "Meta"; // Windows key / Cmd
    case SDLK_RGUI:   return "Meta";
    }
    return nullptr;
}

void cef_browser_key_input(int sdlkey, bool isdown, int modstate)
{
    const char* jskey = SDLKeyToJSKeyName(sdlkey);
    if (jskey && g_browser) {
        // calculate modifiers
        bool shift = (modstate & KMOD_SHIFT) != 0;
        bool ctrl = (modstate & KMOD_CTRL) != 0;
        bool alt = (modstate & KMOD_ALT) != 0;
        bool meta = (modstate & KMOD_GUI) != 0; // Windows/Cmd

        // format: window._cef_handle_key('KeyName', true/false, {shift:true,ctrl:true,alt:true,meta:true});
        char jsbuf[256];
        snprintf(jsbuf, sizeof(jsbuf),
            "window._cef_handle_key('%s', %s, {shift:%s,ctrl:%s,alt:%s,meta:%s});",
            jskey,
            isdown ? "true" : "false",
            shift ? "true" : "false",
            ctrl ? "true" : "false",
            alt ? "true" : "false",
            meta ? "true" : "false"
        );
        g_browser->GetMainFrame()->ExecuteJavaScript(jsbuf, "", 0);
    }

    c_cef_key_event_t ev;
    memset(&ev, 0, sizeof(ev));
    return;
}

/*void cef_browser_key_input(int code, bool isdown, int modstate)
{
    //char buf[512];
    //snprintf(buf, sizeof(buf), "cef_input_active is true: Sending KEY INPUT to CEF: '%d %b %d'\n", code, isdown, modstate);
    //if (cef_input_active)
    //{
        //OutputDebugStringA(buf);
        c_cef_key_event_t ev;
        memset(&ev, 0, sizeof(ev));

        ev.type = isdown ? CEF_KEYEVENT_RAWKEYDOWN : CEF_KEYEVENT_KEYUP;
        ev.key = code;
        ev.scancode = 0;
        ev.modifiers = modstate;
        cef_browser_send_key_event(&ev);
        return;
        //}
}*/

// updates the browser view size (for when the sdl window is resized)
void cef_browser_window_resize(int w, int h)
{
    cef_view_width = w;
    cef_view_height = h;
    if (g_browser) g_browser->GetHost()->WasResized();
}


void* cef_get_browser() {
    return g_browser.get();
}

// executes the given javascript code in the main frame and sends the result back via cefQuery (if available)
void cef_run_javascript(char* code)
{
    void* cef_browser_ptr = cef_get_browser();
    if (cef_browser_ptr) {
        CefBrowser* browser = static_cast<CefBrowser*>(cef_browser_ptr);
        std::string wrapped_code =
            "try { "
            "    var __cef_result = (function() { " + std::string(code) + " })();"
            "    if (window.cefQuery) {"
            "        window.cefQuery({ request: 'myFunctionResult:' + __cef_result });"
            "    } else {alert(`cefQuery not available`)};"
            "} catch(e) {"
            "    if (window.cefQuery) {"
            "        window.cefQuery({ request: 'myFunctionResult:Error: ' + e });"
            "    }"
            "}";

        browser->GetMainFrame()->ExecuteJavaScript(wrapped_code, "", 0);
    }
}

static bool cef_initialized = false;
static bool browser_created = false;

// initializes cef with custom settings and creates the message router if not already done
void cef_initialize(void* window_handle) {
    if (cef_initialized) return;
    cef_initialized = true;

#if defined(_WIN32)
    // create cef main args for windows
    CefMainArgs main_args(GetModuleHandle(NULL));
#else
    // create cef main args for other platforms
    CefMainArgs main_args(0, nullptr);
#endif

    // configure cef settings
    CefSettings settings;
    settings.no_sandbox = true;
    settings.log_severity = LOGSEVERITY_VERBOSE;
    settings.multi_threaded_message_loop = false;
    settings.windowless_rendering_enabled = true;

    // create the application instance
    CefRefPtr<SimpleApp> app = new SimpleApp();
    CefInitialize(main_args, settings, app, nullptr);

    // create the browser-side message router (only once)
    CefMessageRouterConfig config;
    g_message_router = CefMessageRouterBrowserSide::Create(config);
}

// creates a single offscreen (windowless) browser instance with transparent background
void cef_create_browser(void* window_handle, char* file) {
    if (browser_created) return; // prevent multiple browsers
    browser_created = true;

    CefWindowInfo window_info;
#if defined(_WIN32)
    // set up windowless browser with provided window handle on windows
    HWND hwnd = static_cast<HWND>(window_handle);
    window_info.SetAsWindowless(hwnd); // "invisible" by default
#else
    // set up windowless browser on other platforms
    window_info.SetAsWindowless(0);
#endif

    // configure browser settings for transparency
    CefBrowserSettings browser_settings;
    browser_settings.background_color = CefColorSetARGB(0, 0, 0, 0); // alpha, red, green, blue

    // create the browser event handler
    g_handler = new SimpleHandler();

    // add the handler to the message router if available
    if (g_message_router && g_handler)
        g_message_router->AddHandler(g_handler.get(), false);

    // create the browser with the specified settings and file (url or html)
    CefBrowserHost::CreateBrowser(window_info, g_handler,
        file, browser_settings, nullptr, nullptr);
}

void cef_do_message_loop_work() {
    CefDoMessageLoopWork();
}

void cef_shutdown() {
    CefShutdown();
}

int cef_execute_process(void* instance_or_argc_argv)
{
#if defined(_WIN32)
    CefMainArgs main_args((HINSTANCE)instance_or_argc_argv);
#else
    CefMainArgs main_args(0, nullptr);
#endif
    CefRefPtr<SimpleApp> app = new SimpleApp();
    return CefExecuteProcess(main_args, app, nullptr);
}

void cef_cleanup()
{
    // get ready to exit
    if (g_browser) {
        g_browser->GetHost()->CloseBrowser(true);
        g_browser = nullptr;
    }
    if (g_message_router && g_handler) {
        g_message_router->RemoveHandler(g_handler.get());
    }
    g_message_router = nullptr;
    g_handler = nullptr;
}