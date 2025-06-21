#define NOMINMAX

#include "cef.h"
#include "include/cef_app.h"
#include "include/cef_process_message.h"
#include "include/cef_dom.h"
#include "include/internal/cef_types.h"
#include "include/wrapper/cef_message_router.h"
#include "SDL.h"
#include <filesystem>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <unordered_map>

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

// todo: maybe find a better way to get the root path
std::string GetExeDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string fullPath(buffer);
    // strip off the executable name
    size_t pos = fullPath.find_last_of("\\/");
    return (pos == std::string::npos) ? "" : fullPath.substr(0, pos + 1);
}

static std::string download_subdir = "";

static bool pathStartsWith(const std::filesystem::path& p, const std::filesystem::path& base)
{
    auto canonicalP = std::filesystem::weakly_canonical(p);
    auto canonicalBase = std::filesystem::weakly_canonical(base);

    auto itBase = canonicalBase.begin();
    auto itPath = canonicalP.begin();
    for (; itBase != canonicalBase.end(); ++itBase, ++itPath)
    {
        if (itPath == canonicalP.end() || *itBase != *itPath)
            return false;
    }
    return true;
}

struct DownloadCallbackInfo {
    CefRefPtr<CefMessageRouterBrowserSide::Callback> callback;
    std::string path;
};

static std::unordered_map<uint32_t, DownloadCallbackInfo> g_download_callbacks;
static std::unordered_map<std::string, CefRefPtr<CefMessageRouterBrowserSide::Callback>> g_pending_downloads;

/*std::string getDownloadPath(const std::string& filename) {
    std::filesystem::path exeDir = GetExeDir();
    std::filesystem::path homePath = exeDir / ".." / "HOME";
    if (!download_subdir.empty()) {
        homePath /= download_subdir;
        std::filesystem::create_directories(homePath);
    }
    homePath /= filename;
    return std::filesystem::absolute(homePath).string();
}*/

std::string getDownloadPath(const std::string& filename) {
    std::filesystem::path exeDir = GetExeDir();
    std::filesystem::path projectDir = std::filesystem::weakly_canonical(exeDir / "..");
    std::filesystem::path homePath = projectDir / "HOME"; // todo: we should not hardcode the home folder
    std::filesystem::path targetPath = homePath;
    if (!download_subdir.empty()) {
        targetPath /= download_subdir;
    }
    targetPath /= filename;

    std::filesystem::path canonicalTarget = std::filesystem::weakly_canonical(targetPath);

    if (!pathStartsWith(canonicalTarget, projectDir)) {
        canonicalTarget = std::filesystem::weakly_canonical(homePath / filename);
    }
    std::filesystem::create_directories(canonicalTarget.parent_path());
    return canonicalTarget.string();
}

class DownloadHandler : public CefDownloadHandler {
public:
    bool OnBeforeDownload(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefDownloadItem> download_item,
        const CefString& suggested_name,
        CefRefPtr<CefBeforeDownloadCallback> callback
    ) override
    {
        OutputDebugStringA("OnBeforeDownload!!\n");

        std::string real_url = download_item->GetURL().ToString();
        //OutputDebugStringA(("OnBeforeDownload, CefDownloadItem->GetURL(): " + real_url + "\n").c_str());

        if (!g_pending_downloads.empty()) {
            auto it = g_pending_downloads.begin(); // first pending (token->callback)
            std::string token = it->first;
            //OutputDebugStringA(("Matched pending download with token: " + token + "\n").c_str());

            std::string path = getDownloadPath(suggested_name.ToString());
            callback->Continue(path, false);

            // register the callback using the download ID
            g_download_callbacks[download_item->GetId()] = { it->second, path };
            g_pending_downloads.erase(it);
            //OutputDebugStringA("Added callback to g_download_callbacks by token!\n");
        }
        else {
            //OutputDebugStringA("No pending download token found in g_pending_downloads!\n");
            std::string path = getDownloadPath(suggested_name.ToString());
            callback->Continue(path, false);
        }

        download_subdir.clear();
        return true;
    }

    void OnDownloadUpdated(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefDownloadItem> download_item,
        CefRefPtr<CefDownloadItemCallback> callback
    ) override {
        auto it = g_download_callbacks.find(download_item->GetId());
        if (it == g_download_callbacks.end()) {
            // no callback yet (probably first call), just ignore
            //OutputDebugStringA("OnDownloadUpdated: no callback registered, ignoring this event\n");
            return;
        }

        CefRefPtr<CefMessageRouterBrowserSide::Callback> cb = it->second.callback;

        int percent = 0;
        int64_t totalBytes = download_item->GetTotalBytes();
        if (totalBytes > 0) {
            percent = static_cast<int>(download_item->GetReceivedBytes() * 100 / totalBytes);
        }
        char msg[128];
        if (totalBytes > 0) {
            snprintf(msg, sizeof(msg), "{\"status\":\"progress\",\"percent\":%d}", percent);
        }
        else {
            snprintf(msg, sizeof(msg), "{\"status\":\"progress\",\"percent\":%d,\"received\":%lld}", percent, download_item->GetReceivedBytes());
        }

        cb->Success(msg);

        if (download_item->IsComplete()) {
            std::string done = std::string("{\"status\":\"complete\",\"path\":\"") + it->second.path + "\"}";
            cb->Success(done);
            g_download_callbacks.erase(it);
        }
        else if (download_item->IsCanceled()) {
            cb->Failure(-1, "canceled");
            g_download_callbacks.erase(it);
        }
    }

    IMPLEMENT_REFCOUNTING(DownloadHandler);
};


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

    // notified when the focused DOM node changes; inform the browser process
    void OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefDOMNode> node) override
    {
        CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("cef_input_active");
        msg->GetArgumentList()->SetBool(0, node.get() && node->IsEditable());
        frame->SendProcessMessage(PID_BROWSER, msg);
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

        // handle renderer-originated focus state messages
        if (message->GetName() == "cef_input_active") {
            bool active = message->GetArgumentList()->GetBool(0);
            cef_input_active = active;
            if (g_cef_input_active_callback) g_cef_input_active_callback(active);
            return true;
        }

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

    // handles target="_blank"
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
        std::string js = "window.handleTargetBlank && window.handleTargetBlank(\"" + target_url.ToString() + "\");";
        browser->GetMainFrame()->ExecuteJavaScript(js, browser->GetMainFrame()->GetURL(), 0);

        return true;
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
        if (req.rfind("cubescript:", 0) == 0) {
            std::string cmd = req.substr(strlen("cubescript:"));
            if (g_execident_callback) {
                // create a copy of the callback pointer so it survives until result_cb is called.
                CefRefPtr<Callback>* callback_copy = new CefRefPtr<Callback>(callback);

                g_execident_callback(
                    cmd.c_str(),

                    [](const char* result, void* userdata) {
                        CefRefPtr<Callback>* cbptr = static_cast<CefRefPtr<Callback>*>(userdata);
                        if (cbptr && *cbptr) {
                            (*cbptr)->Success(result ? result : "");
                        }
                        delete cbptr;
                    },
                    callback_copy
                );
            }
            else {
                callback->Failure(-1, "No execident callback set");
            }
            return true;
        }

        // download file request
        if (req.rfind("downloadfile:", 0) == 0) {
            std::string data = req.substr(strlen("downloadfile:"));
            std::string url, subdir, token;
            size_t bar1 = data.find('|');
            size_t bar2 = data.rfind('|');
            if (bar1 != std::string::npos && bar2 != std::string::npos && bar1 != bar2) {
                url = data.substr(0, bar1);
                subdir = data.substr(bar1 + 1, bar2 - bar1 - 1);
                token = data.substr(bar2 + 1);
            }
            else {
                url = data;
                subdir = "";
                token = "default";
            }
            //OutputDebugStringA(("Storing pending download for token: " + token + "\n").c_str());
            g_pending_downloads[token] = callback;
            cef_start_download(url.c_str(), subdir.c_str());
            return true;
        }

        if (req == "openDevTools") {
            browser->GetHost()->ShowDevTools(
                CefWindowInfo(),
                nullptr,
                CefBrowserSettings(),
                CefPoint()
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

    // download event
    CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return new DownloadHandler(); }
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


bool cef_input_active = false;

void cef_browser_text_input(const char* str, int len)
{
    if (!cef_input_active) return;
    if (!g_browser) return;
    g_browser->GetHost()->SetFocus(true);

    int i = 0;
    while (i < len)
    {
        uint32_t codepoint = 0;
        int consumed = utf8_nextcodepoint(str + i, &codepoint);
        if (consumed <= 0) break;

        c_cef_key_event_t ev = {};
        ev.type = CEF_KEYEVENT_CHAR;
        ev.key = codepoint;
        ev.scancode = 0;
        ev.character = codepoint;
        ev.unmodified_character = codepoint;
        cef_browser_send_key_event(&ev);

        i += consumed;
    }
}

static int sdlkey_to_cef_keycode(int key)
{
    if (key >= SDLK_a && key <= SDLK_z)
        return 'A' + (key - SDLK_a);
    if (key >= SDLK_0 && key <= SDLK_9)
        return '0' + (key - SDLK_0);

    switch (key)
    {
    case SDLK_LEFT:      return 0x25; // VK_LEFT
    case SDLK_UP:        return 0x26; // VK_UP
    case SDLK_RIGHT:     return 0x27; // VK_RIGHT
    case SDLK_DOWN:      return 0x28; // VK_DOWN
    case SDLK_BACKSPACE: return 0x08; // VK_BACK
    case SDLK_TAB:       return 0x09; // VK_TAB
    case SDLK_RETURN:    return 0x0D; // VK_RETURN
    case SDLK_ESCAPE:    return 0x1B; // VK_ESCAPE
    case SDLK_SPACE:     return 0x20; // VK_SPACE
    case SDLK_PAGEUP:    return 0x21; // VK_PRIOR
    case SDLK_PAGEDOWN:  return 0x22; // VK_NEXT
    case SDLK_END:       return 0x23; // VK_END
    case SDLK_HOME:      return 0x24; // VK_HOME
    case SDLK_INSERT:    return 0x2D; // VK_INSERT
    case SDLK_DELETE:    return 0x2E; // VK_DELETE
    case SDLK_LSHIFT:    return 0x10; // VK_SHIFT
    case SDLK_RSHIFT:    return 0x10; // VK_SHIFT
    case SDLK_LCTRL:     return 0x11; // VK_CONTROL
    case SDLK_RCTRL:     return 0x11; // VK_CONTROL
    case SDLK_LALT:      return 0x12; // VK_MENU
    case SDLK_RALT:      return 0x12; // VK_MENU
    default:             return key;  // fallback
    }
}

static int sdlmod_to_cef_flags(int modstate, int key)
{
    int flags = EVENTFLAG_NONE;
    if (modstate & KMOD_SHIFT) flags |= EVENTFLAG_SHIFT_DOWN;
    if (modstate & KMOD_CTRL)  flags |= EVENTFLAG_CONTROL_DOWN;
    if (modstate & KMOD_ALT)   flags |= EVENTFLAG_ALT_DOWN;
    if (modstate & KMOD_GUI)   flags |= EVENTFLAG_COMMAND_DOWN;
    if (modstate & KMOD_NUM)   flags |= EVENTFLAG_NUM_LOCK_ON;
    if (modstate & KMOD_CAPS)  flags |= EVENTFLAG_CAPS_LOCK_ON;
    if (modstate & KMOD_MODE)  flags |= EVENTFLAG_ALTGR_DOWN;

    if (key >= SDLK_KP_0 && key <= SDLK_KP_HEXADECIMAL)
        flags |= EVENTFLAG_IS_KEY_PAD;

    switch (key)
    {
    case SDLK_LSHIFT: case SDLK_LCTRL: case SDLK_LALT: case SDLK_LGUI:
        flags |= EVENTFLAG_IS_LEFT; break;
    case SDLK_RSHIFT: case SDLK_RCTRL: case SDLK_RALT: case SDLK_RGUI:
        flags |= EVENTFLAG_IS_RIGHT; break;
    }

    return flags;
}

void cef_browser_key_input(int sdlkey, bool isdown, int modstate)
{
    if (!g_browser) return;
    c_cef_key_event_t ev = {};
    ev.type = isdown ? CEF_KEYEVENT_RAWKEYDOWN : CEF_KEYEVENT_KEYUP;
    ev.key = sdlkey_to_cef_keycode(sdlkey);
    ev.scancode = SDL_GetScancodeFromKey(sdlkey);
    ev.modifiers = sdlmod_to_cef_flags(modstate, sdlkey);
    if (sdlkey == SDLK_LALT || sdlkey == SDLK_RALT)
        ev.is_system_key = 1;
    cef_browser_send_key_event(&ev);
}

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

class DownloadImageCallbackImpl : public CefDownloadImageCallback {
public:
    DownloadImageCallbackImpl(cef_image_data_callback_t cb, void* ud)
        : callback(cb), userdata(ud) {}

    void OnDownloadImageFinished(const CefString& image_url, int http_status_code,
        CefRefPtr<CefImage> image) override
    {
        unsigned char* data = nullptr;
        size_t size = 0;
        if (http_status_code == 200 && image && !image->IsEmpty()) {
            int w = 0, h = 0;
            CefRefPtr<CefBinaryValue> png = image->GetAsPNG(1.0f, true, w, h);
            if (png && png->GetSize() > 0) {
                size = png->GetSize();
                data = (unsigned char*)malloc(size);
                png->GetData(data, size, 0);
            }
        }

        std::string urlstr = image_url.ToString();
        if (callback) callback(urlstr.c_str(), data, size, userdata);
        // let the caller handle freeing the returned data
    }

    IMPLEMENT_REFCOUNTING(DownloadImageCallbackImpl);

private:
    cef_image_data_callback_t callback;
    void* userdata;
};

void cef_download_image(const char* url, cef_image_data_callback_t cb, void* userdata)
{
    if (!g_browser) return;
    CefRefPtr<DownloadImageCallbackImpl> callback = new DownloadImageCallbackImpl(cb, userdata);
    g_browser->GetHost()->DownloadImage(url, false, 0, false, callback);
}

void cef_start_download(const char* url, const char* subdir)
{
    if (!g_browser) return;
    download_subdir = subdir ? subdir : "";
    g_browser->GetHost()->StartDownload(url);
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

    // use a persistent cache directory so CEF data like cookies and
    // localStorage are kept between client sessions. Multiple processes
    // cannot share the same root_cache_path so we support a CEF_PROFILE
    // environment variable that defines the subdirectory name. Running
    // multiple clients with different CEF_PROFILE values allows them to
    // coexist while still persisting data for each profile.
    std::filesystem::path exeDir = GetExeDir();
    const char* profileEnv = std::getenv("CEF_PROFILE");
    std::string profile = profileEnv && *profileEnv ? profileEnv : "default";
    std::filesystem::path cachePath = exeDir / "cef_cache" / profile;

    std::filesystem::create_directories(cachePath);
    CefString(&settings.root_cache_path) = cachePath.string();
    CefString(&settings.cache_path) = cachePath.string();

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
    if (g_browser) {
        if (g_browser->GetHost()->HasDevTools()) {
            g_browser->GetHost()->CloseDevTools();
        }
        g_browser->GetHost()->CloseBrowser(true);
        g_browser = nullptr;
    }
    if (g_message_router && g_handler) {
        g_message_router->RemoveHandler(g_handler.get());
    }
    g_message_router = nullptr;
    g_handler = nullptr;
}
