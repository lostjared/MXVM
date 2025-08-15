#ifndef __SDL2__H_
#define __SDL2__H_

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Core SDL functions
int64_t init(void);
void quit(void);

// Window management
int64_t create_window(const char* title, int64_t x, int64_t y, int64_t w, int64_t h, int64_t flags);
void destroy_window(int64_t window_id);
void set_window_title(int64_t window_id, const char* title);
void set_window_position(int64_t window_id, int64_t x, int64_t y);
void get_window_size(int64_t window_id, int64_t* w, int64_t* h);
void set_window_fullscreen(int64_t window_id, int64_t fullscreen);

// Renderer management
int64_t create_renderer(int64_t window_id, int64_t index, int64_t flags);
void destroy_renderer(int64_t renderer_id);
void set_draw_color(int64_t renderer_id, int64_t r, int64_t g, int64_t b, int64_t a);
void clear(int64_t renderer_id);
void present(int64_t renderer_id);
void get_renderer_output_size(int64_t renderer_id, int64_t* w, int64_t* h);

// Drawing functions
void draw_point(int64_t renderer_id, int64_t x, int64_t y);
void draw_line(int64_t renderer_id, int64_t x1, int64_t y1, int64_t x2, int64_t y2);
void draw_rect(int64_t renderer_id, int64_t x, int64_t y, int64_t w, int64_t h);
void fill_rect(int64_t renderer_id, int64_t x, int64_t y, int64_t w, int64_t h);

// Event handling
int64_t poll_event(void);
int64_t get_event_type(void);
int64_t get_key_code(void);
int64_t get_mouse_x(void);
int64_t get_mouse_y(void);
int64_t get_mouse_button(void);

// Surface functions
int64_t create_rgb_surface(int64_t width, int64_t height, int64_t depth);
void free_surface(int64_t surf_ptr);
int64_t blit_surface(int64_t src_ptr, int64_t dst_ptr, int64_t x, int64_t y);

// Mouse state functions (pointer and pointerless)
int64_t get_mouse_state(int64_t* x, int64_t* y);
int64_t get_relative_mouse_state(int64_t* x, int64_t* y);
int64_t get_mouse_buttons(void);
int64_t get_relative_mouse_x(void);
int64_t get_relative_mouse_y(void);
int64_t get_relative_mouse_buttons(void);

// Keyboard state functions
int64_t get_keyboard_state(int64_t* numkeys);
int64_t is_key_pressed(int64_t scancode);
int64_t get_num_keys(void);

// Clipboard functions
void set_clipboard_text(const char* text);
const char* get_clipboard_text(void);

// Cursor functions
void show_cursor(int64_t show);

int64_t create_texture(int64_t renderer_id, int64_t format, int64_t access, int64_t w, int64_t h);
void destroy_texture(int64_t texture_id);
int64_t load_texture(int64_t renderer_id, const char* file_path);
void render_texture(int64_t renderer_id, int64_t texture_id, int64_t src_x, int64_t src_y, int64_t src_w, int64_t src_h, int64_t dst_x, int64_t dst_y, int64_t dst_w, int64_t dst_h);

// Timing functions
int64_t get_ticks(void);
void delay(int64_t ms);

/* Audio functions */
int64_t open_audio(int64_t freq, int64_t format, int64_t channels, int64_t samples);
void close_audio(void);
void pause_audio(int64_t pause_on);
int64_t load_wav(const char* file_path, int64_t* audio_buf, int64_t* audio_len, int64_t* audio_spec);
void free_wav(int64_t audio_buf);
int64_t queue_audio(const void* data, int64_t len);
int64_t get_queued_audio_size(void);
void clear_queued_audio(void);

int64_t update_texture(int64_t texture_id, const void* pixels, int64_t pitch);
int64_t lock_texture(int64_t texture_id, void** pixels, int64_t* pitch);
void unlock_texture(int64_t texture_id);

int64_t init_text(void);
void quit_text(void);
int64_t load_font(const char* file, int64_t ptsize);
void draw_text(int64_t renderer_id, int64_t font_id, const char* text, int64_t x, int64_t y, int64_t r, int64_t g, int64_t b, int64_t a);


int64_t create_render_target(int64_t renderer_id, int64_t width, int64_t height);
void set_render_target(int64_t renderer_id, int64_t target_id);    
void destroy_render_target(int64_t target_id);
void present_scaled(int64_t renderer_id, int64_t target_id, int64_t scale_width, int64_t scale_height);
void present_stretched(int64_t renderer_id, int64_t target_id, int64_t dst_width, int64_t dst_height);

#ifdef __cplusplus
}
#endif


#endif
