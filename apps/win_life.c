

#define STD_OUTPUT_HANDLE ((unsigned long)-11)
#define GRID_WIDTH  40
#define GRID_HEIGHT 15

__declspec(dllimport) void*         __stdcall GetStdHandle(unsigned long nStdHandle);
__declspec(dllimport) int           __stdcall WriteFile(void* hFile, const void* lpBuffer, unsigned long nNumberOfBytesToWrite, unsigned long* lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) void          __stdcall ExitProcess(unsigned int uExitCode);

static void print_str(void* hOut, const char* str) {
    if (!str || !hOut) return;
    unsigned long len = 0;
    while (str[len] != '\0') len++;
    unsigned long written = 0;
    WriteFile(hOut, str, len, &written, (void*)0);
}

static void print_u32(void* hOut, unsigned int val) {
    if (val == 0) {
        print_str(hOut, "0");
        return;
    }
    char buf[16];
    int idx = 14;
    buf[15] = '\0';
    while (val > 0 && idx >= 0) {
        buf[idx--] = (char)('0' + (val % 10));
        val /= 10;
    }
    print_str(hOut, &buf[idx + 1]);
}

static unsigned char grid_curr[GRID_HEIGHT][GRID_WIDTH];
static unsigned char grid_next[GRID_HEIGHT][GRID_WIDTH];

static int count_neighbors(int y, int x) {
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dy == 0 && dx == 0) continue;
            int ny = (y + dy + GRID_HEIGHT) % GRID_HEIGHT;
            int nx = (x + dx + GRID_WIDTH) % GRID_WIDTH;
            count += grid_curr[ny][nx];
        }
    }
    return count;
}

static int evolve(void) {
    int live_cells = 0;
    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            int neighbors = count_neighbors(y, x);
            unsigned char state = grid_curr[y][x];

            if (state == 1) {


                grid_next[y][x] = (neighbors == 2 || neighbors == 3) ? 1 : 0;
            } else {

                grid_next[y][x] = (neighbors == 3) ? 1 : 0;
            }

            if (grid_next[y][x]) {
                live_cells++;
            }
        }
    }


    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            grid_curr[y][x] = grid_next[y][x];
        }
    }

    return live_cells;
}

static void render_grid(void* hOut, int gen, int pop) {
    print_str(hOut, "--- [Generation ");
    print_u32(hOut, gen);
    print_str(hOut, " | Alive: ");
    print_u32(hOut, pop);
    print_str(hOut, "] -----------------------\r\n");

    char line_buf[GRID_WIDTH + 3];
    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            line_buf[x] = grid_curr[y][x] ? '#' : '.';
        }
        line_buf[GRID_WIDTH] = '\r';
        line_buf[GRID_WIDTH + 1] = '\n';
        line_buf[GRID_WIDTH + 2] = '\0';
        print_str(hOut, line_buf);
    }
}

void mainCRTStartup(void) {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    print_str(hOut, "================================================================\r\n");
    print_str(hOut, "  [WIN32 LIFE] Conway's Game of Life Cellular Simulation        \r\n");
    print_str(hOut, "================================================================\r\n");


    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            grid_curr[y][x] = 0;
        }
    }


    grid_curr[1][2] = 1;
    grid_curr[2][3] = 1;
    grid_curr[3][1] = 1;
    grid_curr[3][2] = 1;
    grid_curr[3][3] = 1;


    grid_curr[8][5] = 1;
    grid_curr[8][6] = 1;
    grid_curr[8][7] = 1;


    grid_curr[5][25] = 1;
    grid_curr[5][26] = 1;
    grid_curr[6][25] = 1;
    grid_curr[7][28] = 1;
    grid_curr[8][27] = 1;
    grid_curr[8][28] = 1;

    int initial_pop = 5 + 3 + 6;


    render_grid(hOut, 0, initial_pop);


    int final_pop = initial_pop;
    for (int gen = 1; gen <= 10; ++gen) {
        final_pop = evolve();

        if (gen == 5 || gen == 10) {
            render_grid(hOut, gen, final_pop);
        }
    }

    print_str(hOut, "================================================================\r\n");
    print_str(hOut, "[WIN32 LIFE] 10 Generations Complete. Final Population: ");
    print_u32(hOut, final_pop);
    print_str(hOut, "\r\n[WIN32 LIFE] Exiting cleanly with status code 7...\r\n");
    print_str(hOut, "================================================================\r\n");

    ExitProcess(7);
}
