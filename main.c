// NOTE: printw functions modify the cursor position which is row/col based
// NOTE: Ncurses doesn't draw the last row (we have 31 rows but last visible row is #30)
// NOTE: Last visible line = row - 1
// NOTE: (0,0) is in the top left (inf, inf) is in the bottom right
// NOTE: Currently every frame takes a minimum of 16.6 ms

// TODO: Colors i.e attron(color_pair_id ?) text-to-be-displayed-in-said-color attroff(color_pair_id)
// TODO: Game modes
// TODO: Showcase Endgame
// TODO: Framerate independent state update for every game mode
// TODO: Fix the tail_parts malloc() usage. Have a clear memory allocated only for the frame buffer, read from that when drawing characters in the terminal

#include <ncurses.h>
#include <string.h>
#include <time.h>
#include <immintrin.h> //__rdtsc()

#include "main.h"

#define UPDATE_TIMER 60.0f
#define STRINGS_COUNT 5

// globals
int run_loop = 1;
static int game_mode = MENU;
static char draw_debug_info;
static struct window frame_buffer;
static struct tail_block *tail_parts;
unsigned int block_buffer_size;

void DrawCharTile(int x, int y, char *chartile)
{
	for(int th = 0; th < TILE_SIZE; th++)
	{
		for(int tc = 0; tc < TILE_WIDTH; tc++)
		{
			if(chartile[tc] != ' ')
			{
				mvaddch(y + th, x + tc, chartile[tc]);
			}
		}
		chartile += 3;
	}
}

static void DrawStringData(int x, int y, unsigned int w, unsigned int h, char *data, int dsize)
{
	int size = 1;
	char *sym = data;
	for(int tr = 0; tr < 5; tr++)
	{
		for(int tc = 0; tc < 3; tc++)
		{
			if(*sym)
			{
				mvaddch(y * tc + size, x * tr + size, *sym);
				sym++;
			}
		}
	}
}

static void DrawRectangle(int sx, int sy, unsigned int w, unsigned int h, int symbol)
{
	for(int r = sy; r < h; r++)
	{
		for(int c = sx; c < w; c++)
		{
			mvaddch(r, c, symbol);
		}
	}
	move(0, 0); // reset cursor to 0
}

static void DrawDebugInfo(int x, int y,
						  struct string *lines,
						  float delta_time, 
						  long long delta_cycles,
						  int wall_clock_time,
						  unsigned char *user_input,
						  struct window frame_buffer)
{
	unsigned int longest_string = lines[0].size;

	for(int i = 0; i < STRINGS_COUNT; i++)
	{
		if(longest_string < lines[i].size)
		{
			longest_string = lines[i].size;
		}
	}
	
	unsigned int border_width = longest_string + 1;
	unsigned int border_height = STRINGS_COUNT + 2;
	
	attron(COLOR_PAIR(1));
	DrawRectangle(x, y - border_height, border_width, y, ' ');

	mvprintw(y - 5, x + 1, lines[0].str, wall_clock_time);
	mvprintw(y - 4, x + 1, lines[1].str, frame_buffer.height + 1, frame_buffer.width);
	mvprintw(y - 3, x + 1, lines[2].str, delta_time);
	mvprintw(y - 2, x + 1, lines[3].str, delta_cycles, delta_cycles / 1000000);
	mvprintw(y - 1, x + 1, lines[4].str, millis / delta_time);	
	
	for(int i = 0; i < KEY_COUNT; i++)
	{
		mvprintw(y - 6, x + 1 + i, "%d", user_input[i]);
	}
	
	mvaddch(y, x, ACS_LLCORNER);
	
	for(int row = y - 1; row > y - border_height; row--)
	{
		mvaddch(row, x, ACS_VLINE);
		mvaddch(row, x + border_width, ACS_VLINE);
	}
	
	mvaddch(y, x + border_width, ACS_LRCORNER);
	mvaddch(y - border_height, x, ACS_ULCORNER);
	
	for(int col = x + 1; col < x + border_width; col++)
	{
		mvaddch(y - border_height, col, ACS_HLINE);
		mvaddch(y, col, ACS_HLINE);
	}
	
	mvaddch(y - border_height, x + border_width, ACS_URCORNER);

	attroff(COLOR_PAIR(1));
}

static void ProcessUserInput(unsigned char *user_input, int y, int x)
{
	// NOTE(melvin): How can I implement 2 keys being pressed at once
	move(y,x);
	
	int ch = getch(); // IMPORTANT: getch locks waiting for input if we dont call nodelay()
	switch(ch)
	{
		case KEY_LEFT:
		{
			user_input[KEY_Left] = 1;
		}break;
		case KEY_UP:
		{
			user_input[KEY_Up] = 1;
		}break;
		case KEY_RIGHT:
		{
			user_input[KEY_Right] = 1;
		}break;
		case KEY_DOWN:
		{
			user_input[KEY_Down] = 1;
		}break;
		case 'a':
		{
			user_input[KEY_a] = 1;
		}break;
		case 'w':
		{
			user_input[KEY_w] = 1;
		}break;
		case 'd':
		{
			user_input[KEY_d] = 1;
		}break;
		case 's':
		{
			user_input[KEY_s] = 1;
		}break;
		case 'P':
		{
			user_input[KEY_P] = 1;
			draw_debug_info = draw_debug_info ? 0 : 1; // on/off
		}break;
		case KEY_BACKSPACE:
		{
			printw("Exiting program");
			attron(A_BOLD);
			printw("%c", ch);
			attroff(A_BOLD);
			run_loop = 0;
		}break;
		case KEY_RESIZE:
		{
			clear();
			frame_buffer.width = COLS;
			frame_buffer.height = LINES;
			block_buffer_size = sizeof(struct tail_block) * frame_buffer.width * frame_buffer.height;
			
			free(tail_parts);
			tail_parts = (struct tail_block *)malloc(block_buffer_size);
			
			if(tail_parts == 0)
			{
				exit(0);
			}
		}break;
		case 'T':
		{
			game_mode = TESTBED;
		}break;
		case 'S':
		{
			game_mode = SNAKE;
		}break;
		case 'A': 
		{
			game_mode = ASTEROIDS;
		}break;

		case ESC_KEY_CODE:
		{
			game_mode = MENU;
		}break;
		
		default:break;
	}
}

void DrawLineBresenham(point a, point b, int symbol)
{
	int dx = ABS((b.x - a.x));
	int dy = ABS((b.y - a.y));
	char d1 = 0xFF;
	char d2 = 0xFF;
	float slope = dy / dx; // rise / run
	int idx = dx;
	int x = a.x;
	int y = a.y;
	
	while(idx--)
	{
		mvaddch(y, x, symbol);
		
#if 0
		d1 = next_y - y;
		d2 = 
#endif
		
		if(d2 < d1)
		{
			y++;
		}
		
		x++;
	}
}

void DrawLineSokolov(point a, point b, int symbol)
{
	float dx = b.x - a.x;
	float dy = b.y - a.y;
	
	for(float t = 0.0f; t < 1.0f; t += 0.02f) 
	{
		int x = a.x + dx * t;  // parametric function
		int y = a.y + dy * t;
		mvaddch(y, x, symbol);
	}
}

void DrawLineDDA(point a, point b, int symbol)
{
	float dx = ABS((b.x - a.x));
	float dy = ABS((b.y - a.y));
	float slope = dy / dx;
	float x = a.x;
	float y = a.y;
	int idx = dx;
	
	while(idx--)
	{
		mvaddch(y, x, symbol);
	
		if(x > b.x)
		{
			x--;			
		}
		else
		{
			x++;
		}
		
		if(a.y > b.y)
		{
			y = y - slope;
		}
		else
		{
			y = y + slope;
		}
	}
}

static void ClearWindow(struct window window)
{
	DrawRectangle(window.x, window.y, window.width, window.height, ' ');
}

static void Asteroids(unsigned char *user_input, 
					  struct game_state *gstate, 
					  struct window frame_buffer)
{
	DrawRectangle(frame_buffer.x, frame_buffer.y, frame_buffer.width, frame_buffer.height, '*');
}

// TODO: Pass the Map dimensions as an arg to Snake(...)
// NOTE: Currently speed is frame rate dependent. 
// To make us frame rate independent, speed needs to remain the same regardless of the time to compute frame
static void Snake(unsigned char *user_input, 
				  struct snake_gstate *gstate,
				  int X, int Y,
				  unsigned int width, 
				  unsigned int height, float delta_time)
{
	static float multiplier = 0.5;
	
	if(tail_parts == 0)
	{
		game_mode = TESTBED;
		return;
	}
	
	gstate->tail_blocks = tail_parts;
	
	int border_y = Y + 10;
	int border_height = height - 10;
	struct coords old_head_pos = gstate->player;
	static struct coords old_apple_pos;
	
	if(user_input[KEY_Up])
	{
		if(gstate->tail.y != gstate->player.y - 1)
		{
			gstate->pvel.y = -1;
			gstate->pvel.x = 0;
		}
	}
	else if(user_input[KEY_Down])
	{
		if(gstate->tail.y != gstate->player.y + 1)
		{
			gstate->pvel.y = 1;
			gstate->pvel.x = 0;
		}
	}
	else if(user_input[KEY_Left])
	{
		if(gstate->tail.x != gstate->player.x - 1)
		{
			gstate->pvel.y = 0;
			gstate->pvel.x = -1;
		}
	}
	else if(user_input[KEY_Right])
	{
		if(gstate->tail.x != gstate->player.x + 1)
		{
			gstate->pvel.y = 0;
			gstate->pvel.x = 1;
		}
	}
	
	if(gstate->frame_timer-- < 0)
	{
		gstate->tail_blocks[0].x = gstate->tail.x;
		gstate->tail_blocks[0].y = gstate->tail.y;
		gstate->tail = gstate->player;
		gstate->player.y += gstate->pvel.y;
		gstate->player.x += gstate->pvel.x;
		
		// Clamp player to map bounds
		if(gstate->player.x > width - 3)
		{
			gstate->player.x = X + 2;
		}
		if(gstate->player.x < 2)
		{
			gstate->player.x = width - 3;
		}
		if(gstate->player.y > border_height - 1)
		{
			gstate->player.y = border_y + 1;
		}
		if(gstate->player.y < border_y + 1)
		{
			gstate->player.y = border_height - 1;
		}
		
		if(gstate->player.x != old_head_pos.x || 
		   gstate->player.y != old_head_pos.y)
		{
			if(gstate->player.x == gstate->apple.x && 
			   gstate->player.y == gstate->apple.y)
			{
				old_apple_pos = gstate->apple;
				gstate->apple.y = random() % (border_height - 1); // NOTE: apple should always be within map limits
				gstate->apple.x = (random() % (width - 2)) + 2;
				
				if(gstate->tail_size < ((border_height - 1) * (width - 4)))
				{
					gstate->tail_size += 5;
					if(multiplier > 0)
					{
						multiplier -= 0.25f;
					}
				}
			}		
		}
		
		for(int tid = gstate->tail_size; tid; tid--)
		{
			if(gstate->player.x == gstate->tail_blocks[tid].x && 
			   gstate->player.y == gstate->tail_blocks[tid].y)
			{
				gstate->player.x = X / 2;
				gstate->player.y = border_height / 2;
				gstate->tail_size = 1;
				game_mode = MENU;
				
				memset(gstate->tail_blocks, 0, block_buffer_size);
			}
			
			gstate->tail_blocks[tid] = gstate->tail_blocks[tid - 1];	
			
			if(gstate->tail_blocks[tid].x == old_apple_pos.x &&
			   gstate->tail_blocks[tid].y == old_apple_pos.y)
			{
				gstate->tail_blocks[tid].ingested_state = 1;
			}
		}
		
		gstate->frame_timer = delta_time * multiplier;
	}
	
	user_input[KEY_s] = 1;
	
	// Draw calls
	// Player bounds (map)
	{
		DrawRectangle(0, border_y, width, border_height, ':'); 
		
		for(int row = border_y; row < border_height; row++)
		{
			mvprintw(row, 0, "||");
			mvprintw(row, width - 2, "||");
		}
		for(int col = 0; col < width; col++)
		{
			mvprintw(border_y, col, "=");
			mvprintw(border_height, col, "=");
		}		
	}
	
	// Draw Digits 0-9 represented as tiles of ascii characters
	for(int y_coord = 10, tile_id = 0; y_coord <= 100; y_coord += 10, tile_id++)
	{
		DrawCharTile(y_coord, height - 5, gstate->chartile + tile_id * TILE_OFFSET);	
	}
	
	mvprintw(gstate->apple.y, gstate->apple.x, "$");
	mvprintw(gstate->player.y, gstate->player.x, "X");
	
	for(int tid = gstate->tail_size; tid; tid--)
	{
		mvprintw(gstate->tail_blocks[tid].y, gstate->tail_blocks[tid].x, "#"); // Draw tail block
		mvprintw(gstate->tail.y, gstate->tail.x, "#"); // first tail block
		
		if(gstate->tail_blocks[tid].ingested_state)
		{
			mvprintw(gstate->tail_blocks[tid].y, gstate->tail_blocks[tid].x, "0");
		}
	}
	
	// Debug info
	mvprintw(3, 0, "head velocity: x = %f, y = %f\n", gstate->pvel.x, gstate->pvel.y);
	mvprintw(4, 0, "head coords: x = %d, y = %d\n", gstate->player.x, gstate->player.y);
	mvprintw(5, 0, "tail coords: x = %d, y = %d\n", gstate->tail.x, gstate->tail.y);
	mvprintw(6, 0, "tail size: %d", gstate->tail_size);	
	mvprintw(7, 0, "frame timer: %f", gstate->frame_timer);
}

static void TestBed(unsigned char *user_input, 
					struct game_state *gstate, 
					int X, int Y, 
					unsigned int term_width, 
					unsigned int term_height, 
					int symbol)
{
	if(user_input[KEY_Left])
	{
		gstate->player.x--;
	}
	
	if(user_input[KEY_Right])
	{
		gstate->player.x++;
	}
	if(user_input[KEY_Up])
	{
		gstate->player.y--;
	}
	
	if(user_input[KEY_Down])
	{
		gstate->player.y++;
	}
	
	gstate->player.height = gstate->player.y + 10;
	gstate->player.width = gstate->player.x + 10;
	
	// Clamp coords to the frame buffer
	{
		if(gstate->player.x < 0)
		{
			gstate->player.x = 0;
		}
		if(gstate->player.y < frame_buffer.y + 1)
		{
			gstate->player.y = frame_buffer.y + 1;
		}
		if(gstate->player.width > frame_buffer.width)
		{
			gstate->player.width = frame_buffer.width;
			gstate->player.x = frame_buffer.width - 10;
		}
	}
	
	// Draw Calls
	DrawRectangle(X, Y, term_width, term_height, ACS_CKBOARD);
	
	// Triangle
	int cx = frame_buffer.width / 2;
	int cy = frame_buffer.y + frame_buffer.height / 2;
	int dx = frame_buffer.x + 25;
	int dy = frame_buffer.y + 30;
		
	mvaddch(cy, cx, 'A'); // start point
	mvaddch(dy, dx, 'B'); // end point
		
	point a = {cx, cy};
	point b = {gstate->player.x, gstate->player.y};
		
	DrawLineSokolov(a, b, 'x');//DrawPLineDDA(b, a, 'a');
	DrawLineDDA(a, (point){dx, dy}, 'b');
	DrawLineDDA((point){dx, dy}, b, 'c');
	//DrawLineBresenham((point){dx, dy}, b, 'c');
	
	DrawRectangle(gstate->player.x, gstate->player.y, 
				  gstate->player.width, gstate->player.height, 
				  NCURSES_ACS('?'));
	
	mvprintw(0, 0, "x = %d, y = %d", gstate->player.x,gstate->player.y);
	mvprintw(1, 0, "player width %d", gstate->player.width);
}

int main()
{
	initscr(); // Start curses mode. Initializ a g_var of type WINDOW(exposed to user(programmer) as stdscr)
	cbreak();
	keypad(stdscr, TRUE); // Enable arrow and function keys
	noecho();
	curs_set(0);
	init_color(COLOR_RED, 700, 0, 0);
	nodelay(stdscr, true); // set getch() to be non-blocking
	
	// Clock Timers
	float target_frame_rate = 1/60.f * millis;
	float delta_time = 0.0f;
	struct timespec start_of_prog = {0};
	struct timespec start_t = {0};
	struct timespec end_t = {0};
	__int64_t start = __rdtsc();
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &start_t);
	start_of_prog = start_t;

	frame_buffer.width = COLS;
	frame_buffer.height = LINES;
	
	struct game_state gstate = {0};
	gstate.player.y = frame_buffer.y + 15;
	gstate.player.x = frame_buffer.x + 5;
	
	block_buffer_size = sizeof(struct tail_block) * frame_buffer.width * frame_buffer.height;
	tail_parts = (struct tail_block *)malloc(block_buffer_size);

	if(tail_parts == 0)
	{
		return 0;
	}
	
	struct snake_gstate snake_state = {0};
	snake_state.player.x = frame_buffer.width / 2;
	snake_state.player.y = frame_buffer.height / 2;
	snake_state.tail.x = snake_state.player.x;
	snake_state.tail.y = snake_state.player.y - 1;
	snake_state.tail_size = 1;
	snake_state.apple = (struct coords){snake_state.player.x, snake_state.player.y - 5};
	snake_state.tail_blocks = tail_parts;
	snake_state.pvel.y = 1.0f;
	snake_state.frame_timer = UPDATE_TIMER;
	snake_state.chartile = charmap[0];

	float elapsed = 0;
	unsigned char user_input[KEY_COUNT] = {0};
	
	// Ncurses color
	start_color();
	init_pair(1, COLOR_RED, COLOR_BLACK);
	
	attron(COLOR_PAIR(1));
	mvprintw(1, 0, "Viola !!! In color ...");
	attroff(COLOR_PAIR(1));
	
	//------------------------------------MAIN LOOP----------------------------------------
	while(run_loop)
	{
		ProcessUserInput(user_input, 2, COLS / 2);
		ClearWindow(frame_buffer);

		switch(game_mode)
		{
			case TESTBED:
			{
				TestBed(user_input, &gstate, 
						frame_buffer.x, frame_buffer.y, 
						frame_buffer.width, frame_buffer.height, NCURSES_ACS('q'));
			}break;
			case SNAKE:
			{
				Snake(user_input, &snake_state,
					  frame_buffer.x, frame_buffer.y, 
					  frame_buffer.width, frame_buffer.height, 
					  delta_time);
			}break;
			case ASTEROIDS:
			{
				Asteroids(user_input, &gstate, frame_buffer);
			}break;
			case MENU:
			{
				attron(A_REVERSE | A_BLINK);
				mvprintw(4, 5, "1.Test Bed");
				mvprintw(5, 5, "2.Snake");
				mvprintw(6, 5, "3.Asteroids");
				mvprintw(7, 5, "4.Tetris");
				attroff(A_REVERSE | A_BLINK);
			}break;
			default: break;
		}
		
		// Timers
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_t);

		__int64_t end = __rdtsc();
		long long delta_cycles = end - start;
		start = end;
		
		double deltas[3] = {0};
		deltas[0] = (end_t.tv_sec - start_t.tv_sec); // s
		deltas[1] = (end_t.tv_nsec - start_t.tv_nsec); // ns
		deltas[2] = (deltas[0] * millis) + (deltas[1] / 1000000.0f); // ms
		delta_time = deltas[2];
		
		if((end_t.tv_sec - start_of_prog.tv_sec) > 1)
		{
			elapsed++;
			start_of_prog = end_t;
		}
		
		// Cap frame rate
		while(delta_time < target_frame_rate)
		{
			clock_gettime(CLOCK_MONOTONIC_RAW, &end_t);
			delta_time = (end_t.tv_sec - start_t.tv_sec) * millis + (end_t.tv_nsec - start_t.tv_nsec) / 1000000.0f;
		}
		
		struct string debug_strings[STRINGS_COUNT] = {0};
		debug_strings[0].str  = "Time passed since start of program: %dTs";
		debug_strings[0].size = strlen(debug_strings[0].str);
		debug_strings[1].str  = "This screen has %d rows and %d columns";;
		debug_strings[1].size = strlen(debug_strings[1].str);
		debug_strings[2].str  = "Frame Time: %f ms";;
		debug_strings[2].size = strlen(debug_strings[2].str);
		debug_strings[3].str  = "Frame Cycles: %lld,(%lld)Mhz";
		debug_strings[3].size = strlen(debug_strings[3].str);
		debug_strings[4].str  = "FPS: %f";
		debug_strings[4].size = strlen(debug_strings[4].str);
		
		if(draw_debug_info)
		{
			DrawDebugInfo(0, frame_buffer.height - 2, 
						  debug_strings, delta_time, 
						  delta_cycles, 
						  elapsed,user_input,
						  frame_buffer);
		}
		
		start_t = end_t;
		
		memset(user_input, 0, sizeof(user_input));

		refresh(); // ONLY updates stdscr i.e terminal window (excludes other WINDOW * windows). Call this to actually display data on screen
		if(run_loop == 0)break;
	}
	
	endwin(); // need to call this or the terminal will be broken after program exits
	return 0;
}