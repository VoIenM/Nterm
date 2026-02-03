#ifndef MAIN_H
#define MAIN_H

#define ESC_KEY_CODE 27
#define millis 1000
#define nanos  1000000000
#define MAX(A,B) (A > B) ? (A) : (B)
#define MIN(A,B) (A < B) ? (A) : (B)
#define ABS(A) ((A) > 0 ? A : (A * -1))

#define TILE_SIZE 5   // measured in rows per digit tile 
#define TILES_COUNT 3 // amount of tiles
#define TILE_WIDTH 3  // cols per tile
#define TILE_OFFSET TILE_SIZE * TILE_WIDTH

static char charmap[][TILE_WIDTH] = 
{
	"***",
	"* *", 
	"* *",
	"* *",
	"***",
	
	" * ",
	" * ", 
	" * ",
	" * ",
	" * ",
	
	"***",
	"  *", 
	"***",
	"*  ",
	"***",
	
	"***",
	"  *",
	"***",
	"  *",
	"***",
	
	"* *",
	"* *",
	"***",
	"  *",
	"  *",
	
	"***",
	"*  ",
	"***",
	"  *",
	"***",
	
	"***",
	"*  ",
	"***",
	"* *",
	"***",
	
	"***",
	"  *",
	" * ",
	" * ",
	" * ",
	
	"***",
	"* *",
	"***",
	"* *",
	"***",
	
	"***",
	"* *",
	"***",
	"  *",
	"***",
};

struct coords
{
	int x;
	int y;
};

struct string
{
	char *str;
	unsigned int size;
};

struct fcoords
{
	float x;
	float y;
};

struct window
{
	int x;
	int y;
	unsigned int width;
	unsigned int height;
};

struct game_state
{
	struct window player;
};

struct tail_block
{
	char ingested_state;
	int x;
	int y;
};

struct snake_gstate
{
	struct coords player;
	struct coords tail;
	struct coords apple;
	struct tail_block *tail_blocks;
	int tail_size;
	struct fcoords pvel;
	struct fcoords tvel;
	int fps;
	float frame_timer;
	char *chartile;
};

enum
{
	KEY_w,
	KEY_a,
	KEY_s,
	KEY_d,
	KEY_p,
	KEY_P,
	KEY_Left,
	KEY_Right,
	KEY_Up,
	KEY_Down,
	KEY_SPACE,
	
	KEY_COUNT
};

enum
{
	TESTBED   = 'T',
	SNAKE 	= 'S',
	ASTEROIDS = 'A',
	MENU	  = 'M',
	
	GAME_MODES_COUNT
};

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef struct coords point;

#endif //MAIN_H
