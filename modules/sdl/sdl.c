#include"mx_sdl.h"


static SDL_Event g_event;
static SDL_Window** g_windows = NULL;
static SDL_Renderer** g_renderers = NULL;
static SDL_Texture** g_textures = NULL;
static int64_t g_window_count = 0;
static int64_t g_renderer_count = 0;
static int64_t g_texture_count = 0;
static TTF_Font** g_fonts = NULL;
static int64_t g_font_count = 0;

/* --- NEW: Render target management --- */
static SDL_Texture** g_render_targets = NULL;    /* Render target textures */
static int64_t* g_target_widths = NULL;          /* Logical width of render targets */
static int64_t* g_target_heights = NULL;         /* Logical height of render targets */
static int64_t g_render_target_count = 0;
/* --- END NEW --- */

int64_t init(void) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) == 0) {
        return 1;
    }
    return 0;
}

void quit(void) {
    if (g_windows) free(g_windows);
    if (g_renderers) free(g_renderers);
    if (g_textures) free(g_textures);
    if (g_fonts) {
        for (int64_t i = 0; i < g_font_count; ++i) {
            if (g_fonts[i]) TTF_CloseFont(g_fonts[i]);
        }
        free(g_fonts);
    }
    /* Clean up render targets */
    if (g_render_targets) {
        for (int64_t i = 0; i < g_render_target_count; ++i) {
            if (g_render_targets[i]) SDL_DestroyTexture(g_render_targets[i]);
        }
        free(g_render_targets);
    }
    if (g_target_widths) free(g_target_widths);
    if (g_target_heights) free(g_target_heights);
    SDL_Quit();
}

// Create window with actual size (no render target texture created automatically)
int64_t create_window(const char* title, int64_t x, int64_t y, int64_t w, int64_t h, int64_t flags) {
    SDL_Window* window = SDL_CreateWindow(title, (int)x, (int)y, (int)w, (int)h, (Uint32)flags);
    if (!window) return -1;
    
    g_windows = realloc(g_windows, sizeof(SDL_Window*) * (g_window_count + 1));
    g_windows[g_window_count] = window;
    return g_window_count++;
}

void destroy_window(int64_t window_id) {
    if (window_id >= 0 && window_id < g_window_count && g_windows[window_id]) {
        SDL_DestroyWindow(g_windows[window_id]);
        g_windows[window_id] = NULL;
    }
}

// Create renderer (no automatic render target)
int64_t create_renderer(int64_t window_id, int64_t index, int64_t flags) {
    if (window_id < 0 || window_id >= g_window_count || !g_windows[window_id]) return -1;
    
    SDL_Renderer* renderer = SDL_CreateRenderer(g_windows[window_id], (int)index, (Uint32)flags);
    if (!renderer) return -1;
    
    g_renderers = realloc(g_renderers, sizeof(SDL_Renderer*) * (g_renderer_count + 1));
    g_renderers[g_renderer_count] = renderer;
    return g_renderer_count++;
}

void destroy_renderer(int64_t renderer_id) {
    if (renderer_id >= 0 && renderer_id < g_renderer_count && g_renderers[renderer_id]) {
        SDL_DestroyRenderer(g_renderers[renderer_id]);
        g_renderers[renderer_id] = NULL;
    }
}


int64_t create_render_target(int64_t renderer_id, int64_t width, int64_t height) {
    if (renderer_id < 0 || renderer_id >= g_renderer_count || !g_renderers[renderer_id]) return -1;
    
    SDL_Texture* target = SDL_CreateTexture(g_renderers[renderer_id], 
                                          SDL_PIXELFORMAT_RGBA8888, 
                                          SDL_TEXTUREACCESS_TARGET, 
                                          (int)width, (int)height);
    if (!target) return -1;
    

    g_render_targets = realloc(g_render_targets, sizeof(SDL_Texture*) * (g_render_target_count + 1));
    g_target_widths = realloc(g_target_widths, sizeof(int64_t) * (g_render_target_count + 1));
    g_target_heights = realloc(g_target_heights, sizeof(int64_t) * (g_render_target_count + 1));
    
    g_render_targets[g_render_target_count] = target;
    g_target_widths[g_render_target_count] = width;
    g_target_heights[g_render_target_count] = height;
    SDL_SetRenderTarget(g_renderers[renderer_id], target);
    SDL_SetRenderDrawColor(g_renderers[renderer_id], 0, 0, 0, 255);
    SDL_RenderClear(g_renderers[renderer_id]);
    
    return g_render_target_count++;
}

/* Set an existing render target as active */
void set_render_target(int64_t renderer_id, int64_t target_id) {
    if (renderer_id < 0 || renderer_id >= g_renderer_count || !g_renderers[renderer_id]) return;
    
    if (target_id == -1) {
        // Set to default (window) render target
        SDL_SetRenderTarget(g_renderers[renderer_id], NULL);
    } else if (target_id >= 0 && target_id < g_render_target_count && g_render_targets[target_id]) {
        SDL_SetRenderTarget(g_renderers[renderer_id], g_render_targets[target_id]);
    }
}

/* Destroy a render target */
void destroy_render_target(int64_t target_id) {
    if (target_id >= 0 && target_id < g_render_target_count && g_render_targets[target_id]) {
        SDL_DestroyTexture(g_render_targets[target_id]);
        g_render_targets[target_id] = NULL;
    }
}
/* --- END NEW --- */

int64_t poll_event(void) {
    return SDL_PollEvent(&g_event);
}

int64_t get_event_type(void) {
    return g_event.type;
}

int64_t get_key_code(void) {
    return g_event.key.keysym.sym;
}

int64_t get_mouse_x(void) {
    return g_event.motion.x;
}

int64_t get_mouse_y(void) {
    return g_event.motion.y;
}

int64_t get_mouse_button(void) {
    return g_event.button.button;
}

void set_draw_color(int64_t renderer_id, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (renderer_id >= 0 && renderer_id < g_renderer_count && g_renderers[renderer_id]) {
        SDL_SetRenderDrawColor(g_renderers[renderer_id], (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    }
}

void clear(int64_t renderer_id) {
    if (renderer_id >= 0 && renderer_id < g_renderer_count && g_renderers[renderer_id]) {
        SDL_RenderClear(g_renderers[renderer_id]);
    }
}

/* --- MODIFIED: Present function now takes target ID and scaling dimensions --- */
void present(int64_t renderer_id) {
    if (renderer_id >= 0 && renderer_id < g_renderer_count && g_renderers[renderer_id]) {
        SDL_RenderPresent(g_renderers[renderer_id]);
    }
}

/* NEW: Present render target with scaling */
void present_scaled(int64_t renderer_id, int64_t target_id, int64_t scale_width, int64_t scale_height) {
    if (renderer_id < 0 || renderer_id >= g_renderer_count || !g_renderers[renderer_id]) return;
    if (target_id < 0 || target_id >= g_render_target_count || !g_render_targets[target_id]) return;
    
    SDL_Renderer* renderer = g_renderers[renderer_id];
    SDL_Texture* target = g_render_targets[target_id];
    
    // Set render target to default (window)
    SDL_SetRenderTarget(renderer, NULL);
    
    // Clear the window
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    // Get window size for centering if needed
    SDL_Window* window = NULL;
    for (int i = 0; i < g_window_count; i++) {
        if (g_windows[i] && SDL_GetRenderer(g_windows[i]) == renderer) {
            window = g_windows[i];
            break;
        }
    }
    
    int window_w, window_h;
    if (window) {
        SDL_GetWindowSize(window, &window_w, &window_h);
    } else {
        window_w = (int)scale_width;
        window_h = (int)scale_height;
    }
    
    // Calculate scaling to fit window while maintaining aspect ratio
    int target_w = (int)scale_width;
    int target_h = (int)scale_height;
    
    float scale_x = (float)window_w / target_w;
    float scale_y = (float)window_h / target_h;
    float scale = (scale_x < scale_y) ? scale_x : scale_y; // Use smaller scale to fit
    
    int scaled_w = (int)(target_w * scale);
    int scaled_h = (int)(target_h * scale);
    
    // Center the scaled texture
    int x = (window_w - scaled_w) / 2;
    int y = (window_h - scaled_h) / 2;
    
    SDL_Rect dst = { x, y, scaled_w, scaled_h };
    SDL_RenderCopy(renderer, target, NULL, &dst);
    
    // Present to screen
    SDL_RenderPresent(renderer);
    
    // Restore render target back to the texture for future drawing
    SDL_SetRenderTarget(renderer, target);
}

/* Simple present scaled to exact size without aspect ratio preservation */
void present_stretched(int64_t renderer_id, int64_t target_id, int64_t dst_width, int64_t dst_height) {
    if (renderer_id < 0 || renderer_id >= g_renderer_count || !g_renderers[renderer_id]) return;
    if (target_id < 0 || target_id >= g_render_target_count || !g_render_targets[target_id]) return;
    
    SDL_Renderer* renderer = g_renderers[renderer_id];
    SDL_Texture* target = g_render_targets[target_id];
    
    // Set render target to default (window)
    SDL_SetRenderTarget(renderer, NULL);
    
    // Clear the window
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    // Stretch to exact size
    SDL_Rect dst = { 0, 0, (int)dst_width, (int)dst_height };
    SDL_RenderCopy(renderer, target, NULL, &dst);
    
    // Present to screen
    SDL_RenderPresent(renderer);
    
    // Restore render target
    SDL_SetRenderTarget(renderer, target);
}
/* --- END MODIFIED --- */

void draw_point(int64_t renderer_id, int64_t x, int64_t y) {
    if (renderer_id >= 0 && renderer_id < g_renderer_count && g_renderers[renderer_id]) {
        SDL_RenderDrawPoint(g_renderers[renderer_id], (int)x, (int)y);
    }
}

void draw_line(int64_t renderer_id, int64_t x1, int64_t y1, int64_t x2, int64_t y2) {
    if (renderer_id >= 0 && renderer_id < g_renderer_count && g_renderers[renderer_id]) {
        SDL_RenderDrawLine(g_renderers[renderer_id], (int)x1, (int)y1, (int)x2, (int)y2);
    }
}

void draw_rect(int64_t renderer_id, int64_t x, int64_t y, int64_t w, int64_t h) {
    if (renderer_id >= 0 && renderer_id < g_renderer_count && g_renderers[renderer_id]) {
        SDL_Rect rect = {(int)x, (int)y, (int)w, (int)h};
        SDL_RenderDrawRect(g_renderers[renderer_id], &rect);
    }
}

void fill_rect(int64_t renderer_id, int64_t x, int64_t y, int64_t w, int64_t h) {
    if (renderer_id >= 0 && renderer_id < g_renderer_count && g_renderers[renderer_id]) {
        SDL_Rect rect = {(int)x, (int)y, (int)w, (int)h};
        SDL_RenderFillRect(g_renderers[renderer_id], &rect);
    }
}

// Texture functions
int64_t create_texture(int64_t renderer_id, int64_t format, int64_t access, int64_t w, int64_t h) {
    if (renderer_id < 0 || renderer_id >= g_renderer_count || !g_renderers[renderer_id]) return -1;
    
    SDL_Texture* texture = SDL_CreateTexture(g_renderers[renderer_id], (Uint32)format, (int)access, (int)w, (int)h);
    if (!texture) return -1;
    
    g_textures = realloc(g_textures, sizeof(SDL_Texture*) * (g_texture_count + 1));
    g_textures[g_texture_count] = texture;
    return g_texture_count++;
}

void destroy_texture(int64_t texture_id) {
    if (texture_id >= 0 && texture_id < g_texture_count && g_textures[texture_id]) {
        SDL_DestroyTexture(g_textures[texture_id]);
        g_textures[texture_id] = NULL;
    }
}

int64_t load_texture(int64_t renderer_id, const char* file_path) {
    if (renderer_id < 0 || renderer_id >= g_renderer_count || !g_renderers[renderer_id]) return -1;
    
    SDL_Surface* surface = SDL_LoadBMP(file_path);
    if (!surface) return -2;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(g_renderers[renderer_id], surface);
    SDL_FreeSurface(surface);
    if (!texture) return -3;
    
    g_textures = realloc(g_textures, sizeof(SDL_Texture*) * (g_texture_count + 1));
    g_textures[g_texture_count] = texture;
    
    return g_texture_count++;
}

void render_texture(int64_t renderer_id, int64_t texture_id, int64_t src_x, int64_t src_y, int64_t src_w, int64_t src_h, int64_t dst_x, int64_t dst_y, int64_t dst_w, int64_t dst_h) {
    if (renderer_id >= 0 && renderer_id < g_renderer_count && g_renderers[renderer_id] &&
        texture_id >= 0 && texture_id < g_texture_count && g_textures[texture_id]) {
        
        SDL_Rect src_rect = { (int)src_x, (int)src_y, (int)src_w, (int)src_h };
        SDL_Rect dst_rect = { (int)dst_x, (int)dst_y, (int)dst_w, (int)dst_h };
        
        SDL_Rect *rc1 = &src_rect;

        if(src_x == -1 || src_y == -1 && src_w == -1 && src_h == -1) {
            rc1 = NULL;
        }

        SDL_Rect *dst_rc =    &dst_rect;
        if(dst_x == -1 && dst_y == -1 && dst_w == -1 && dst_h == -1) {
            dst_rc = NULL;
        }

        SDL_RenderCopy(g_renderers[renderer_id], g_textures[texture_id],rc1,dst_rc);
    }
}

int64_t update_texture(int64_t texture_id, const void* pixels, int64_t pitch) {
    if (texture_id >= 0 && texture_id < g_texture_count && g_textures[texture_id]) {
        return SDL_UpdateTexture(g_textures[texture_id], NULL, pixels, (int)pitch) == 0 ? 1 : 0;
    }
    return 0;
}

int64_t lock_texture(int64_t texture_id, void** pixels, int64_t* pitch) {
    if (texture_id >= 0 && texture_id < g_texture_count && g_textures[texture_id]) {
        int p;
        int result = SDL_LockTexture(g_textures[texture_id], NULL, pixels, &p);
        *pitch = p;
        return result == 0 ? 1 : 0;
    }
    return 0;
}

void unlock_texture(int64_t texture_id) {
    if (texture_id >= 0 && texture_id < g_texture_count && g_textures[texture_id]) {
        SDL_UnlockTexture(g_textures[texture_id]);
    }
}

// Timing functions
int64_t get_ticks(void) {
    return SDL_GetTicks();
}

void delay(int64_t ms) {
    SDL_Delay((Uint32)ms);
}

// Audio functions
int64_t open_audio(int64_t freq, int64_t format, int64_t channels, int64_t samples) {
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = (int)freq;
    wanted_spec.format = (SDL_AudioFormat)format;
    wanted_spec.channels = (Uint8)channels;
    wanted_spec.samples = (Uint16)samples;
    wanted_spec.callback = NULL;
    wanted_spec.userdata = NULL;
    
    return SDL_OpenAudio(&wanted_spec, NULL) == 0 ? 1 : 0;
}

void close_audio(void) {
    SDL_CloseAudio();
}

void pause_audio(int64_t pause_on) {
    SDL_PauseAudio(pause_on ? 1 : 0);
}

int64_t load_wav(const char* file_path, int64_t* audio_buf, int64_t* audio_len, int64_t* audio_spec) {
    SDL_AudioSpec spec;
    Uint8* buf;
    Uint32 len;
    
    if (SDL_LoadWAV(file_path, &spec, &buf, &len) == NULL) {
        return 0;
    }
    
    *audio_buf = (int64_t)buf;
    *audio_len = len;
    *audio_spec = (int64_t)&spec;
    return 1;
}

void free_wav(int64_t audio_buf) {
    if (audio_buf) {
        SDL_FreeWAV((Uint8*)audio_buf);
    }
}

int64_t queue_audio(const void* data, int64_t len) {
    return SDL_QueueAudio(1, data, (Uint32)len) == 0 ? 1 : 0;
}

int64_t get_queued_audio_size(void) {
    return SDL_GetQueuedAudioSize(1);
}

void clear_queued_audio(void) {
    SDL_ClearQueuedAudio(1);
}

// Mouse functions (no pointers)
int64_t get_mouse_buttons(void) {
    int x, y;
    return SDL_GetMouseState(&x, &y);
}

int64_t get_relative_mouse_x(void) {
    int x, y;
    SDL_GetRelativeMouseState(&x, &y);
    return x;
}

int64_t get_relative_mouse_y(void) {
    int x, y;
    SDL_GetRelativeMouseState(&x, &y);
    return y;
}

int64_t get_relative_mouse_buttons(void) {
    int x, y;
    return SDL_GetRelativeMouseState(&x, &y);
}

// Keyboard functions
int64_t get_keyboard_state(int64_t* numkeys) {
    int nk;
    const Uint8* state = SDL_GetKeyboardState(&nk);
    *numkeys = nk;
    return (int64_t)state;
}

int64_t is_key_pressed(int64_t scancode) {
    int numkeys;
    const Uint8* state = SDL_GetKeyboardState(&numkeys);
    if (scancode < 0 || scancode >= numkeys) return 0;
    return state[scancode] ? 1 : 0;
}

int64_t get_num_keys(void) {
    int numkeys;
    SDL_GetKeyboardState(&numkeys);
    return numkeys;
}

// Clipboard functions
void set_clipboard_text(const char* text) {
    SDL_SetClipboardText(text);
}

const char* get_clipboard_text(void) {
    return SDL_GetClipboardText();
}

// Window management additions
void set_window_title(int64_t window_id, const char* title) {
    if (window_id >= 0 && window_id < g_window_count && g_windows[window_id]) {
        SDL_SetWindowTitle(g_windows[window_id], title);
    }
}

void set_window_position(int64_t window_id, int64_t x, int64_t y) {
    if (window_id >= 0 && window_id < g_window_count && g_windows[window_id]) {
        SDL_SetWindowPosition(g_windows[window_id], (int)x, (int)y);
    }
}

void get_window_size(int64_t window_id, int64_t* w, int64_t* h) {
    if (window_id >= 0 && window_id < g_window_count && g_windows[window_id]) {
        int ww, hh;
        SDL_GetWindowSize(g_windows[window_id], &ww, &hh);
        *w = ww;
        *h = hh;
    }
}

void set_window_fullscreen(int64_t window_id, int64_t fullscreen) {
    if (window_id >= 0 && window_id < g_window_count && g_windows[window_id]) {
        SDL_SetWindowFullscreen(g_windows[window_id], fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
    }
}

void get_renderer_output_size(int64_t renderer_id, int64_t* w, int64_t* h) {
    if (renderer_id >= 0 && renderer_id < g_renderer_count && g_renderers[renderer_id]) {
        int ww, hh;
        SDL_GetRendererOutputSize(g_renderers[renderer_id], &ww, &hh);
        *w = ww;
        *h = hh;
    }
}

void show_cursor(int64_t show) {
    SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
}

int64_t init_text(void) {
    return TTF_Init() == 0 ? 1 : 0;
}

void quit_text(void) {
    if (g_fonts) {
        for (int64_t i = 0; i < g_font_count; ++i) {
            if (g_fonts[i]) TTF_CloseFont(g_fonts[i]);
        }
        free(g_fonts);
        g_fonts = NULL;
        g_font_count = 0;
    }
    TTF_Quit();
}

int64_t load_font(const char* file, int64_t ptsize) {
    TTF_Font* font = TTF_OpenFont(file, (int)ptsize);
    if (!font) return -1;
    g_fonts = realloc(g_fonts, sizeof(TTF_Font*) * (g_font_count + 1));
    g_fonts[g_font_count] = font;
    return g_font_count++;
}

void draw_text(int64_t renderer_id, int64_t font_id, const char* text, int64_t x, int64_t y, int64_t r, int64_t g, int64_t b, int64_t a) {
    if (renderer_id < 0 || renderer_id >= g_renderer_count || !g_renderers[renderer_id]) return;
    if (font_id < 0 || font_id >= g_font_count || !g_fonts[font_id]) return;

    SDL_Color color = { (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a };
    SDL_Surface* surface = TTF_RenderUTF8_Blended(g_fonts[font_id], text, color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(g_renderers[renderer_id], surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst = { (int)x, (int)y, surface->w, surface->h };
    SDL_RenderCopy(g_renderers[renderer_id], texture, NULL, &dst);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

int64_t create_rgb_surface(int64_t width, int64_t height, int64_t depth) {
    SDL_Surface* s = NULL;
    int w = (int)width, h = (int)height, d = (int)depth;

    if (d == 32) s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    else if (d == 24) s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, SDL_PIXELFORMAT_RGB24);
    else if (d == 16) s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 16, SDL_PIXELFORMAT_RGB565);
    else return 0;

    return (int64_t)s;
}

void free_surface(int64_t surf_ptr) {
    if (surf_ptr) SDL_FreeSurface((SDL_Surface*)surf_ptr);
}

int64_t blit_surface(int64_t src_ptr, int64_t dst_ptr, int64_t x, int64_t y) {
    if (!src_ptr || !dst_ptr) return 0;
    SDL_Surface* src = (SDL_Surface*)src_ptr;
    SDL_Surface* dst = (SDL_Surface*)dst_ptr;
    SDL_Rect dst_rc = { (int)x, (int)y, 0, 0 };
    return SDL_BlitSurface(src, NULL, dst, &dst_rc) == 0 ? 1 : 0;
}

int64_t get_mouse_state(int64_t* x, int64_t* y) {
    int ix, iy;
    Uint32 m = SDL_GetMouseState(&ix, &iy);
    if (x) *x = ix;
    if (y) *y = iy;
    return (int64_t)m;
}

int64_t get_relative_mouse_state(int64_t* x, int64_t* y) {
    int ix, iy;
    Uint32 m = SDL_GetRelativeMouseState(&ix, &iy);
    if (x) *x = ix;
    if (y) *y = iy;
    return (int64_t)m;
}
