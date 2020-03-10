/*
 * (C) 2020, level0noob
 *	chip8.cpp - Simple Chip 8 Emulator
 *	Support for Chip 48 CPUs is enabled by doing -mcpu=chip48
 *  Flags := -fshow-asm -ffontset=file= -fwidth -fheight -fscale
 *  Datasheets taken:
 *	http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
 */

#include <initializer_list>
#include <string>
#include <cstring>
#include <cmath>
#include <ctime>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

using namespace std;
static unsigned char SCRWIDTH  {64};
static unsigned char SCRHEIGHT {32};
static unsigned char SC {8};
static unsigned char flags = 0x0;

enum {
	CHIP8_DISASSEMBLER = 1 << 0,
	CHIP48_CPU = 1 << 1,
	CHIP8_FONTSET_FILE = 1 << 2,
	SET_WIDTH = 1 << 3,
	SET_HEIGHT = 1 << 4,
	SET_SCALE = 1 << 5,
	FOUND_FILE = 1 << 6,
	CHIP8_ASSEMBLER = 1 << 7
};

enum {
	CHIP8_NORMAL_SOUND_HIT = 0,
	CHIP8_FATAL_SOUND_HIT = 1
};

class STDIO {
public:
	STDIO operator<<(const char *str) { printf("%s", str); return *this; }
	STDIO operator<<(const unsigned long x) { printf("0x%lX", x); return *this; }
} STDIO;

class chip8_disasm {
private:
	FILE *p;
	Mix_Chunk *chk[2] {0, 0};
	int chks = 0;
	unsigned char *data;
	unsigned char newdata[0x1000]; /* Fixed size */
	unsigned short int size;
	unsigned short I {0};
	unsigned char V[0x10];
	/* Addressing registers are here */
	unsigned short nnn;
	unsigned char n : 4;
	unsigned char x : 4, y : 4;
	unsigned char kk {0};
	unsigned short PC = 0x200;
	unsigned char DT {0}, ST {0};
	unsigned short stack[0x10];
	unsigned char sp {0};
	unsigned char display[32][64];
	unsigned char fontset[80];
	unsigned char key[0x10];
	bool error = false;
public:
	size_t cycles = 0;
	bool quit = false;
	explicit chip8_disasm(const char *str) {
		p = fopen(str, "rb");
		n = 0; x = 0; y = 0;
		if (!p) {
			STDIO << ("Enter a valid file\n");
			exit(2);
		}
		fseek(p, 0, SEEK_END);
		size = ftell(p);
		if (size > 4096) { STDIO << "Expected file < 4096\n"; exit(1); }
		rewind(p);
		cycles = 3;
		data = new unsigned char[size];
		fread(data, size, 1, p);
		#define v(s) memset(s, 0, sizeof s);
		v(stack)v(display)v(fontset)v(newdata)
		v(fontset)v(newdata)v(V)v(key)
		#undef v
		for (int i = 0x200; i < 0x1000; i++)
			newdata[i] = data[i - 0x200];
	}
	void load_sound(int id, const char *file) {
		if (id >= 2)
			return;
		chk[id] = Mix_LoadWAV(file);
		if (!chk[id]) {
			STDIO << ("Invalid sound file\n");
			return;
		}
	}
	void destroy_chunk(int id) { if (id >= 2) return; if (chk[id]) Mix_FreeChunk(chk[id]); }
	void rst() { PC = 0x200, memset(display, 0, sizeof display), memset(key, 0, sizeof key); ST = 14; }
	#define T(c, v)	\
			case c: v break;
	void get_events() {
		static SDL_Event event;
		static SDL_Joystick *joystick = NULL;
		if (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_QUIT: quit = 1; break;
				/* Mainly i set-up this for my controller to work remap this to your own liking */
				case SDL_JOYAXISMOTION:
					if (!event.jaxis.axis) {
						if (event.jaxis.value>0) key[6]=1;
						else if (event.jaxis.value==0) key[6]=0, key[4] = 0;
						else if (event.jaxis.value<0) key[4]=1;
					}
					if (!!event.jaxis.axis) {
						if (event.jaxis.value>0) key[8]=1;
						else if (event.jaxis.value == 0) key[2]=0, key[8] = 0;
						else if (event.jaxis.value<0) key[2]=1;
					}
					break;
				case SDL_JOYBUTTONUP:
					switch (event.jbutton.button) case 0: key[15]=0; break;
					break;
				case SDL_JOYBUTTONDOWN:
					switch (event.jbutton.button)	{	case 0: key[15]=1;break;
														case 2: rst();  break;
														case 8:
														case 9:
														case 10: quit=1; break;
													}
													break;
				case SDL_JOYDEVICEADDED:
					STDIO << ("Added joystick controller input mapper game device set to 1\n");
					joystick = SDL_JoystickOpen(0);
					break;
				case SDL_JOYDEVICEREMOVED:
					STDIO << ("Removed joystick\n");
					if (joystick) SDL_JoystickClose(joystick);
					break;
				case SDL_KEYUP:
					switch (event.key.keysym.sym)	{	case SDLK_LEFT: key[4]=0;break;	
														case SDLK_DOWN: key[8]=0;break;
														case SDLK_UP: key[2]=0;break;
														case SDLK_RIGHT: key[6]=0;break;
														case '0': key[0]=0; break;
														case '1': key[1]=0; break;
														case '2': key[2]=0; break;
														case '3': key[3]=0; break;
														case '4': key[4]=0; break;
														case '5': key[5]=0; break;
														case '6': key[6]=0; break;
														case '7': key[7]=0; break;
														case '8': key[8]=0; break;
														case '9': key[9]=0; break;
														case 'a': key[10]=0;break;
														case 'b': key[11]=0;break;
														case 'c': key[12]=0;break;
														case 'd': key[13]=0;break;
														case 'e': key[14]=0;break;
														case 'f': key[15]=0;break;
													}
													break;
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym)	{	case SDLK_ESCAPE: quit=1; break;
														case SDLK_DOWN: key[8]=1;break;
														case SDLK_UP: key[2]=1;break;
														case SDLK_RIGHT: key[6]=1;break;
														case SDLK_LEFT: key[4]=1;break;
														case 'q': quit=1;break;
														case '0': key[0]=1;break;
														case '1': key[1]=1;break;
														case '2': key[2]=1;break; // Translate down
														case '3': key[3]=1;break;
														case '4': key[4]=1;break; // Translate left
														case '5': key[5]=1;break;
														case '6': key[6]=1;break; // Translate right
														case '7': key[7]=1;break;
														case '8': key[8]=1;break; // Translate Up
														case '9': key[9]=1;break;
														case 'a': key[10]=1;break;
														case 'b': key[11]=1;break;
														case 'c': key[12]=1;break;
														case 'd': key[13]=1;break;
														case 'e': key[14]=1;break;
														case 'f': key[15]=1;break;  // This starts the game for similar games
														case 'r': rst(); break;
													}
													break;
			}
		}
	}
	unsigned char rd8(unsigned short addr) { return newdata[addr]; }
	void load_fontset(const char *T = NULL) { /* Chip 8 sprite possible size of 8x15 */
			unsigned char *str {newdata};
			unsigned int n {0};
			if (T == NULL) { for (auto& i : {0xF0, 0x90, 0x90, 0x90, 0xF0, 0x20, 0x60, 0x20, 0x20, 0x70,
											 0xF0, 0x10, 0xF0, 0x80, 0xF0, 0xF0, 0x10, 0xF0, 0x10, 0xF0,
											 0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0, 0x80, 0xF0, 0x10, 0xF0,
											 0xF0, 0x80, 0xF0, 0x90, 0xF0, 0xF0, 0x10, 0x20, 0x40, 0x40,
											 0xF0, 0x90, 0xF0, 0x90, 0xF0, 0xF0, 0x90, 0xF0, 0x10, 0xF0,
											 0xF0, 0x90, 0xF0, 0x90, 0x90, 0xE0, 0x90, 0xE0, 0x90, 0xE0,
											 0xF0, 0x80, 0x80, 0x80, 0xF0, 0xE0, 0x90, 0x90, 0x90, 0xE0,
											 0xF0, 0x80, 0xF0, 0x80, 0xF0, 0xF0, 0x80, 0xF0, 0x80, 0x80}) *str++ = i, n++; }
			if (!(n % 0x50) && n == 0x50) STDIO << ("Font set test pass\n");
			else STDIO << ("Invalid fontset must be from range 0 ... 0x50\n");
	}
	void draw(SDL_Renderer *render) {
		SDL_Rect r;
		memset(&r,0,sizeof r);
		r.w = SCRWIDTH*SC;
		r.h = SCRHEIGHT*SC;
		for (int y = 0; y < 32; y++)
			for (int x = 0; x < 64; x++) {
				r.x=x*SC;
				r.y=y*SC;
				if (display[y][x]) { SDL_SetRenderDrawColor(render, 0xFF, 0xFF, 0xFF, 0xFF), SDL_RenderFillRect(render, &r); }
				else { SDL_SetRenderDrawColor(render, 0x0, 0x0, 0x0, 0x0), SDL_RenderFillRect(render, &r); }
			}
	}
	inline void drw() {
		// Read the n bytes from memory starting at address stored in I
		V[0xF] = 0;
		register int n, sprite, b;
		for (n = 0; n<this->n; n++)
			for (sprite = 0, b = newdata[n+I]; sprite<8; sprite++) {
				int bt = (b >> sprite)&0x1;
				unsigned char *p = &display[(V[y]+n)%SCRHEIGHT][(V[x]+(7-sprite))%SCRWIDTH];
				if (*p==1&&bt==1) V[0xF]=1;
				*p^=bt;
			}
	}
	inline bool fault() { return error; }
	// This executes one instruction at a time
	void exec() {
		unsigned short opcode=rd8(PC&0xFFF) << 8 | rd8((PC&0xFFF) + 1);
		nnn=opcode%0x1000;
		n=opcode%16;
		kk=opcode%256;
		x=(opcode&0x0F00)>>8;
		y=((opcode&0x00F0)>>4)&0xF;
		PC+=2;
		if (flags & CHIP8_DISASSEMBLER)
			printf("Current #%04X, I #%03X, NNN #%03X, KK #%02X, N #%01X, X #%01X, Y #%01X, VX %02X, VY #%02X, SP #%02X, PC #%04X, CYCLES %ld\n",
			opcode, I, nnn, kk, n, x,
			y, V[x], V[y], sp, PC, cycles);
		if (ST>0) { if (ST-- == 14) { if (chk[CHIP8_FATAL_SOUND_HIT]) Mix_PlayChannel(0, chk[CHIP8_FATAL_SOUND_HIT], 0), ST = 0; }
					else if (ST == 4) if (chk[CHIP8_NORMAL_SOUND_HIT]) Mix_PlayChannel(0, chk[CHIP8_NORMAL_SOUND_HIT], 0), ST=0; }
		if (DT>0) DT--;
		// The normal Chip 8 Opcodes are in here
		// val = byte
		switch(opcode & 0xF000) {
			case 0x0000:
				switch (opcode & 0xFF && (opcode >> 8) == 0) 	{	T(0, PC = nnn; cycles += 6; ) // SYS addr
																	T(1, switch (opcode) 	{	T(0xE0, memset(display, 0, sizeof display);cycles+=2; ) // CLS
																								T(0xEE, PC=stack[--sp%0x10];cycles+=2; )	// RET
																								default: goto err;
																							})
																}
																break;
			case 0x1000: PC=nnn;cycles+=3; break; // JP addr
			case 0x2000: stack[sp++%0x10]=  PC;PC=nnn;cycles+=2; break; // CALL addr
			case 0x3000: PC=V[x]==kk?PC+2:  PC;cycles+=2; break; // SE Vx == byte skip
			case 0x4000: PC=V[x]!=kk?PC+2:  PC;cycles+=2; break;	// SNE to this
			case 0x5000: PC=V[x]==V[y]?PC+2:PC;cycles+=2; break; // SE Vx == Vy skip
			case 0x6000: V[x]=kk; cycles+=1; break; // LD Vx, byte
			case 0x7000: V[x]+=kk;cycles+=2; break; // ADD Vx, byte
			case 0x8000: //  RCA's original COSMAC VIP manual
									cycles+=2;
									switch (opcode & 0xF) 	{	case 0: V[x]=V[y]; break; // LD Vx, Vy
																case 1: V[x]|=V[y];break; // OR Vx, Vy
																case 2: V[x]&=V[y];break; // AND Vx, Vy
																case 3: V[x]^=V[y];break; // XOR Vx, Vy
																case 4: V[x]=(V[x]+V[y]);V[0xF]=V[x]>255;V[x]&=0xF0;break; // ADD Vx, Vy
																case 5: V[x]-=V[y];V[0xF]=!(V[x]>255);break; // SUB Vx, Vy
																case 6:	V[0xF]=V[x]&1;V[x]>>=1;break; // SHR Vx, val
																case 7: V[x]=V[y]-V[x];V[0xF]=!(V[x]>255);break; // SUBN Vx, Vy
																case 0xE:V[0xF]=(V[x]&0x80)!=0;V[x]<<=1;break; // SHL Vx, val
																default: goto err;
															}
															break;
			case 0x9000: PC=V[x]!=V[y]?PC+2:PC;cycles+=2;break; // SNE Vx, Vy
			case 0xA000: I=nnn; cycles+=3;	  break;	// LD I, nnn
			case 0xB000: PC=nnn+V[0];cycles+=4;break;	// JP V0, nnn
			case 0xC000: V[x]=(rand()%4);cycles+=2;break; // RND Vx, val
			case 0xD000: drw(); cycles+=3;	  break; // DRW Vx, Vy, n
			case 0xE000: switch (opcode&0xFF)	{	T(0x9E, PC=!!key[V[x]]?PC+2:PC;cycles+=1;) // SKP Vx
																T(0xA1,  PC=!key[V[x]]?PC+2:PC;cycles+=1;) // SKNP Vx
															}
															break;
			case 0xF000:
									switch (opcode&0xFF)	{	T(0x7,	V[x]=DT;cycles+=3;) // LD Vx, DT
																T(0xA,	while (1) { // LD Vx, K
																			get_events();
																			for (int i = 0; i < 16; i++) {
																				if (key[i]) { V[x] = i; goto done; }
																			}
																		}
																		done:
																		cycles+=3;)
																T(0x15, DT=V[x];cycles+=3;) // LD DT, Vx
																T(0x18, ST=V[x];cycles+=3;) // LD ST, Vx
																T(0x1E, I+=V[x];cycles+=3;) // ADD I, Vx
																T(0x29, I=5*V[x];cycles+=3;) // LD F, Vx
																T(0x33, // LD B, Vx
																	newdata[I]	=(V[x]%1000)/100;
																	newdata[I+1]=(V[x]%100)/10;
																	newdata[I+2]=(V[x]%10);
																	cycles+=3;)
																T(0x55, // LD [I], Vx
																	for (int i = 0; i<0x10; i++) {
																		newdata[i+I]=V[i];
																	}
																	I += x + 1;
																	cycles+=3;)
																T(0x65, // LD Vx, [I]
																	for (int i = 0; i<0x10; i++) {
																		V[i]=newdata[i+I];
																	}
																	I += x + 1;
																	cycles+=3;)
															}
															break;
			default: err:; STDIO << "Undefined opcode " << opcode << "\n", error = true;
		}
		// The Chip 48 Flags are inside of here
		if (flags & CHIP48_CPU) {
			switch (opcode) {

			}
		}
	}
	#undef T
	~chip8_disasm() { delete data; }
};

static const char *fl = NULL;
class {
private:
public:
	void set_cpu_to_chip8() { flags |= CHIP48_CPU; }
	void parse_file(const char *token) { flags |= FOUND_FILE; fl = token; }
	const char *get_file() { return fl; }
	void parse_width(const char *str) { flags |= SET_WIDTH; }
	void parse_height(const char *str) { flags |= SET_HEIGHT; }
	void parse_scale(const char *str) { flags |= SET_SCALE; }
	void parse_fontset(const char *fontsetfile) { flags |= CHIP8_FONTSET_FILE; }
	void enable_disassembler() { flags |= CHIP8_DISASSEMBLER; }
} chip8_disassembler_parser;

// No texture rendering is made implement one if you want to
int main(int argc, char *argv[] = 0) {
	if (argc < 2) { STDIO << argv[0] + 2 << ": use --help for more chip 8 details\n"; return 2; }
	#define u(name, info) #name "\t" #info "\n"
	string type = "CHIP 8 ";
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--help")) {
			printf("%s: [args] ...\n"
			u(--help, Shows the info for ch8emu help)
			u(ASSEMBLER, )
			u(--assembler-help, Shows the assembler help)
			u(DISASSEMBLER and PARSER, )
			u(-mcpu=chip48\t, Sets the CPU style to a normal Chip-48 Instruction set)
			u(-show-disassembler, Shows the view of the disassembler but not the view entirely complete)
			u(-file=\t\t, Loads the current file if valid)
			u(-scale=\t\t, Sets the scale based on the user preference increasing time to render slowly Preferred x10)
			u(-width=\t\t, Sets the width based on the user preference default: 64)
			u(-height=\t, Sets the height based on the user preference default: 32)
			u(-fontset=\t, Loads the current parsed file of the fontset used by font.ch8fontset tool provided in CH8FONTPARSE)
			u(NOTES, ...)
			u(- The sfx folder has 2 files one for the CHIP8_NORMAL_SOUND_HIT as to sfx/normal.wav CHIP8_FATAL_SOUND_HIT as to sfx/fatal.wav, )
			u(- Rendering process is kind of a problem here and there is CPU instructions that still is a problem, ), argv[0] + 2);
			return 2;
		} else if (!memcmp(argv[i], "-", 1)) {
			argv[i]+=1;
			if (!memcmp(argv[i], "file=", 5)) { argv[i]+=5; chip8_disassembler_parser.parse_file(argv[i]); continue; }
			if (!memcmp(argv[i], "scale=", 6)) { argv[i]+=6; SC = strtol(argv[i], 0, 10) ? : 0; continue; }
			if (!memcmp(argv[i], "width=", 6)) { argv[i]+=6; chip8_disassembler_parser.parse_width(argv[i]); continue; }
			if (!memcmp(argv[i], "height=", 8)) { argv[i]+=8; chip8_disassembler_parser.parse_width(argv[i]); continue; }
			if (!memcmp(argv[i], "fontset=", 8)) { }
			else { printf("Unknown type\n"); return 1; }
		}
	}
	#undef u
	type += " Emulator";
	/* Doesn't relate to the disassembler at all */
	if (!(flags&FOUND_FILE)) { printf("Expected the -file= flag\n"); return 2; }
	if (flags & CHIP8_ASSEMBLER) { return 0; }
	srand(time(0));
	SDL_Window *window;
	SDL_Renderer *render;
	SDL_Texture *tex;
	int w=SCRWIDTH*SC, h=SCRHEIGHT*SC;
	chip8_disasm chip(chip8_disassembler_parser.get_file());
	chip.load_fontset();
	SDL_Init(SDL_INIT_EVERYTHING);
	Mix_Init(MIX_INIT_MP3|MIX_INIT_MID|MIX_INIT_OGG);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	Mix_OpenAudio(22050, AUDIO_S16SYS, 1, 2048);
	chip.load_sound(CHIP8_NORMAL_SOUND_HIT, "sfx/normal.wav");
	chip.load_sound(CHIP8_FATAL_SOUND_HIT, "sfx/fatal.wav");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	SDL_SetHint(SDL_HINT_IDLE_TIMER_DISABLED, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	window = SDL_CreateWindow(type.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_SHOWN);
	render = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	while (!chip.quit&&!chip.fault()&&chip.cycles--) { chip.get_events(), chip.exec(), chip.draw(render), SDL_RenderPresent(render); }
	chip.destroy_chunk(CHIP8_NORMAL_SOUND_HIT);
	chip.destroy_chunk(CHIP8_FATAL_SOUND_HIT);
	SDL_DestroyRenderer(render);
	SDL_DestroyWindow(window);
	Mix_Quit();
	SDL_Quit();
	return 0;
}
