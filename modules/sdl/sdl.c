
#include"mx_sdl.h"


static SDL_Event g_event;
static SDL_Window** g_windows = NULL;
static SDL_Renderer** g_renderers = NULL;
static SDL_Texture** g_textures = NULL;
static int64_t g_window_count = 0;
static int64_t g_renderer_count = 0;
static int64_t g_texture_count = 0;

// Implementation
int64_t init(void) {
    return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0 ? 1 : 0;
}

void quit(void) {
    if (g_windows) free(g_windows);
    if (g_renderers) free(g_renderers);
    if (g_textures) free(g_textures);
    SDL_Quit();
}

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

void present(int64_t renderer_id) {
    if (renderer_id >= 0 && renderer_id < g_renderer_count && g_renderers[renderer_id]) {
        SDL_RenderPresent(g_renderers[renderer_id]);
    }
}

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

int64_t get_ticks(void) {
    return SDL_GetTicks();
}

void delay(int64_t ms) {
    SDL_Delay((Uint32)ms);
}