program Asteroids;
const
  SDL_WINDOW_SHOWN = 4;
  SDL_RENDERER_ACCELLERATED = 2;
  SDL_QUIT = 256;
  SDL_KEYDOWN = 768;
  SDL_KEYUP = 769;
  SDLK_ESCAPE = 27;

  WIDTH = 1280;
  HEIGHT = 720;

  title = 'Asteroids';
  filename = 'font.ttf';
  EXIT_FAILURE = 1;

  SCALE_W = 640;
  SCALE_H = 360;

  SDL_SCANCODE_RIGHT = 79;
  SDL_SCANCODE_LEFT = 80;
  SDL_SCANCODE_DOWN = 81;
  SDL_SCANCODE_UP = 82;

  MAX_AST = 8;

type
  RPoint = record
    x: real;
    y: real;
  end;

  Asteroid = record
    posx: real;
    posy: real;
    vx: real;
    vy: real;
    angle: real;
    va: real;
    radius: real;
    nvert: integer;
    seed: integer;
  end;

var
  running: boolean;
  event_type_result: integer;
  window_id, renderer, target_id: integer;
  font_id: integer;
  key: integer;

  ship_pos: RPoint;
  ship_vel: RPoint;
  ship_angle: real;
  thrust_on: integer;

  ast: array[0..MAX_AST-1] of Asteroid;
  lives: integer;
  score: integer;

procedure wrap_point(var p: RPoint);
begin
  if p.x < 0 then p.x := p.x + SCALE_W;
  if p.x >= SCALE_W then p.x := p.x - SCALE_W;
  if p.y < 0 then p.y := p.y + SCALE_H;
  if p.y >= SCALE_H then p.y := p.y - SCALE_H;
end;

function frand(a, b: real): real;
begin
  frand := a + (b - a) * (rand() mod 1000) / 1000.0;
end;

function mix(seed, idx: integer): integer;
var v: integer;
begin
  v := (seed * 1103515245 + 12345 + idx * 196314165) mod 2147483647;
  if v < 0 then v := -v;
  mix := v;
end;

function jagged_scale(seed, idx: integer): real;
var h: integer; t: real;
begin
  h := mix(seed, idx);
  t := (h mod 1000) / 1000.0;
  jagged_scale := 0.65 + t * (1.35 - 0.65);
end;

procedure make_asteroid(i: integer);
begin
  ast[i].posx := frand(0.0, SCALE_W);
  ast[i].posy := frand(0.0, SCALE_H);
  ast[i].vx := frand(-0.8, 0.8);
  ast[i].vy := frand(-0.8, 0.8);
  ast[i].angle := frand(0.0, 6.28318);
  ast[i].va := frand(-0.02, 0.02);
  ast[i].radius := frand(14.0, 34.0);
  ast[i].nvert := 8 + (rand() mod 5);
  ast[i].seed := rand();
end;

procedure reset_game;
var
  i: integer;
begin
  ship_pos.x := SCALE_W / 2.0;
  ship_pos.y := SCALE_H / 2.0;
  ship_vel.x := 0.0;
  ship_vel.y := 0.0;
  ship_angle := -1.570796;
  thrust_on := 0;
  for i := 0 to MAX_AST-1 do make_asteroid(i);
  lives := 3;
  score := 0;
end;

procedure draw_ship;
var
  p0, p1, p2: RPoint;
  c, s: real;
  x0, y0, x1, y1, x2, y2: integer;
begin
  c := cos(ship_angle);
  s := sin(ship_angle);
  p0.x := ship_pos.x + (0.0 * c - (-10.0) * s);
  p0.y := ship_pos.y + (0.0 * s + (-10.0) * c);
  p1.x := ship_pos.x + (6.0 * c - 8.0 * s);
  p1.y := ship_pos.y + (6.0 * s + 8.0 * c);
  p2.x := ship_pos.x + (-6.0 * c - 8.0 * s);
  p2.y := ship_pos.y + (-6.0 * s + 8.0 * c);
  x0 := trunc(p0.x); y0 := trunc(p0.y);
  x1 := trunc(p1.x); y1 := trunc(p1.y);
  x2 := trunc(p2.x); y2 := trunc(p2.y);
  sdl_draw_line(renderer, x0, y0, x1, y1);
  sdl_draw_line(renderer, x1, y1, x2, y2);
  sdl_draw_line(renderer, x2, y2, x0, y0);
end;

procedure draw_asteroid(i: integer);
var
  n, k: integer;
  a0, a1, step: real;
  r0, r1: real;
  x0, y0, x1, y1: integer;
begin
  n := ast[i].nvert;
  if n < 3 then n := 3;
  step := 6.28318 / n;
  for k := 0 to n-1 do
  begin
    a0 := ast[i].angle + step * k;
    a1 := ast[i].angle + step * ((k + 1) mod n);
    r0 := ast[i].radius * jagged_scale(ast[i].seed, k);
    r1 := ast[i].radius * jagged_scale(ast[i].seed, (k + 1) mod n);
    x0 := trunc(ast[i].posx + cos(a0) * r0);
    y0 := trunc(ast[i].posy + sin(a0) * r0);
    x1 := trunc(ast[i].posx + cos(a1) * r1);
    y1 := trunc(ast[i].posy + sin(a1) * r1);
    sdl_draw_line(renderer, x0, y0, x1, y1);
  end;
end;

function dist2(ax, ay, bx, by: real): real;
var dx, dy: real;
begin
  dx := ax - bx;
  dy := ay - by;
  dist2 := dx*dx + dy*dy;
end;

procedure update_input;
begin
  if sdl_is_key_pressed(SDL_SCANCODE_LEFT) <> 0 then ship_angle := ship_angle - 0.06;
  if sdl_is_key_pressed(SDL_SCANCODE_RIGHT) <> 0 then ship_angle := ship_angle + 0.06;
  thrust_on := sdl_is_key_pressed(SDL_SCANCODE_UP);
end;

procedure update_ship;
var
  accel: real;
begin
  accel := 0.12;
  if thrust_on <> 0 then
  begin
    ship_vel.x := ship_vel.x + cos(ship_angle) * accel;
    ship_vel.y := ship_vel.y + sin(ship_angle) * accel;
  end;
  ship_vel.x := ship_vel.x * 0.995;
  ship_vel.y := ship_vel.y * 0.995;
  ship_pos.x := ship_pos.x + ship_vel.x;
  ship_pos.y := ship_pos.y + ship_vel.y;
  wrap_point(ship_pos);
end;

procedure update_asteroids;
var
  i: integer;
begin
  for i := 0 to MAX_AST-1 do
  begin
    ast[i].posx := ast[i].posx + ast[i].vx;
    ast[i].posy := ast[i].posy + ast[i].vy;
    ast[i].angle := ast[i].angle + ast[i].va;
    if ast[i].angle > 6.28318 then ast[i].angle := ast[i].angle - 6.28318;
    if ast[i].angle < 0.0 then ast[i].angle := ast[i].angle + 6.28318;
    if ast[i].posx < 0 then ast[i].posx := ast[i].posx + SCALE_W;
    if ast[i].posx >= SCALE_W then ast[i].posx := ast[i].posx - SCALE_W;
    if ast[i].posy < 0 then ast[i].posy := ast[i].posy + SCALE_H;
    if ast[i].posy >= SCALE_H then ast[i].posy := ast[i].posy - SCALE_H;
  end;
end;

procedure check_collisions;
var
  i: integer;
begin
  for i := 0 to MAX_AST-1 do
  begin
    if dist2(ship_pos.x, ship_pos.y, ast[i].posx, ast[i].posy) < (ast[i].radius*ast[i].radius) then
    begin
      lives := lives - 1;
      ship_pos.x := SCALE_W / 2.0;
      ship_pos.y := SCALE_H / 2.0;
      ship_vel.x := 0.0;
      ship_vel.y := 0.0;
      ship_angle := -1.570796;
      exit;
    end;
  end;
end;

procedure render;
var
  i: integer;
begin
  sdl_set_render_target(renderer, target_id);
  sdl_set_draw_color(renderer, 0, 0, 0, 255);
  sdl_clear(renderer);
  sdl_set_draw_color(renderer, 255, 255, 255, 255);
  draw_ship;
  for i := 0 to MAX_AST-1 do draw_asteroid(i);
  sdl_draw_text(renderer, font_id, 'Lives: ' + inttostr(lives), 8, 8, 255, 255, 255, 255);
  sdl_draw_text(renderer, font_id, 'Score: ' + inttostr(score), SCALE_W - 120, 8, 255, 255, 255, 255);
end;

procedure init(title: string; xval, yval, wval, hval: integer);
var
  result: integer;
begin
  result := sdl_init();
  if result <> 0 then
  begin
    writeln('failed to init SDL');
    halt(EXIT_FAILURE);
  end;
  window_id := sdl_create_window(title, xval, yval, wval, hval, SDL_WINDOW_SHOWN);
  if window_id = -1 then
  begin
    writeln('Could not create window');
    sdl_quit();
    halt(EXIT_FAILURE);
  end;
  renderer := sdl_create_renderer(window_id, -1, SDL_RENDERER_ACCELLERATED);
  if renderer = -1 then
  begin
    writeln('could not create renderer');
    sdl_destroy_window(window_id);
    sdl_quit();
    halt(EXIT_FAILURE);
  end;
  target_id := sdl_create_render_target(renderer, SCALE_W, SCALE_H);
  if target_id = -1 then
  begin
    writeln('could not create render target');
    sdl_destroy_renderer(renderer);
    sdl_destroy_window(window_id);
    sdl_quit();
    halt(EXIT_FAILURE);
  end;
  if sdl_init_text() <> 1 then
  begin
    writeln('Font subsystem failed to load');
    halt(EXIT_FAILURE);
  end;
  font_id := sdl_load_font(filename, 16);
  if font_id = -1 then
  begin
    writeln('failed to open: ', filename);
    halt(EXIT_FAILURE);
  end;
  reset_game;
end;

procedure cleanup;
begin
  sdl_destroy_render_target(target_id);
  sdl_destroy_renderer(renderer);
  sdl_destroy_window(window_id);
  sdl_quit_text();
  sdl_quit();
end;

begin
  init(title, 100, 100, WIDTH, HEIGHT);
  running := true;
  while running = true do
  begin
    while sdl_poll_event() <> 0 do
    begin
      event_type_result := sdl_get_event_type();
      if event_type_result = SDL_QUIT then running := false;
      if event_type_result = SDL_KEYDOWN then
      begin
        key := sdl_get_key_code();
        if key = SDLK_ESCAPE then running := false;
      end;
    end;
    if lives <= 0 then reset_game;
    update_input;
    update_ship;
    update_asteroids;
    check_collisions;
    render;
    sdl_present_scaled(renderer, target_id, WIDTH, HEIGHT);
    sdl_delay(16);
  end;
  cleanup;
end.
