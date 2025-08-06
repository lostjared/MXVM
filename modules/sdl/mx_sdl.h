
#ifndef __SDL2__H_
#define __SDL2__H_

#include"SDL.h"
#include <stdint.h>
/*#define SDL_INIT_VIDEO 0x00000020
#define SDL_INIT_AUDIO 0x00000010
#define SDL_WINDOWPOS_UNDEFINED 0x1FFF0000
#define SDL_WINDOW_SHOWN 0x00000004
#define SDL_RENDERER_ACCELERATED 0x00000002
#define SDL_QUIT 0x100
#define SDL_KEYDOWN 0x300
#define SDL_KEYUP 0x301
#define SDL_MOUSEBUTTONDOWN 0x401
#define SDL_MOUSEBUTTONUP 0x402
#define SDL_MOUSEMOTION 0x400*/

int64_t init(void);
void quit(void);
int64_t create_window(const char* title, int64_t x, int64_t y, int64_t w, int64_t h, int64_t flags);
void destroy_window(int64_t window_id);
int64_t create_renderer(int64_t window_id, int64_t index, int64_t flags);
void destroy_renderer(int64_t renderer_id);

int64_t poll_event(void);
int64_t get_event_type(void);
int64_t get_key_code(void);
int64_t get_mouse_x(void);
int64_t get_mouse_y(void);
int64_t get_mouse_button(void);

void set_draw_color(int64_t renderer_id, int64_t r, int64_t g, int64_t b, int64_t a);
void clear(int64_t renderer_id);
void present(int64_t renderer_id);
void draw_point(int64_t renderer_id, int64_t x, int64_t y);
void draw_line(int64_t renderer_id, int64_t x1, int64_t y1, int64_t x2, int64_t y2);
void draw_rect(int64_t renderer_id, int64_t x, int64_t y, int64_t w, int64_t h);
void fill_rect(int64_t renderer_id, int64_t x, int64_t y, int64_t w, int64_t h);

int64_t create_texture(int64_t renderer_id, int64_t format, int64_t access, int64_t w, int64_t h);
void destroy_texture(int64_t texture_id);
int64_t load_texture(int64_t renderer_id, const char* file_path);
void render_texture(int64_t renderer_id, int64_t texture_id, int64_t src_x, int64_t src_y, int64_t src_w, int64_t src_h, int64_t dst_x, int64_t dst_y, int64_t dst_w, int64_t dst_h);

int64_t open_audio(int64_t frequency, int64_t format, int64_t channels, int64_t samples);
void close_audio(void);
void pause_audio(int64_t pause_on);

int64_t get_ticks(void);
void delay(int64_t ms);

#endif