#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <termios.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

int clamp(int v, int min, int max) {
    if(v < min) return min;
    if(v >= max) return max;
    return v;
}

void pchar(char c) {
    write(STDOUT_FILENO, &c, 1);
}

int writeTerm(char *thing) {
    write(STDOUT_FILENO, thing, strlen(thing));
}

void setCursor(int row, int col) {
    char buf[20] = "\e[";
    char *ptr = buf + strlen(buf);
    snprintf(ptr, 6, "%d;", row+1);
    ptr = buf + strlen(buf);
    snprintf(ptr, 6, "%dH", col+1);
    writeTerm(buf);
}

int getScreenWidth() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

int getScreenHeight() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row;
}

void renderText(char *text, size_t size, int topOffset, int leftOffset, int cx, int cy) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int rows = w.ws_row;
    int cols = w.ws_col;

    writeTerm("\e[?25l");
    size_t current = 0;

    for(int i = 0; i < topOffset; i++) {
        while(current < size && *text != '\n') {
            text++;
            current++;
        }
        text++;
        current++;
    }

    for(int row = 0; row < rows; row++) {
        bool finishedLine = false;

        for(int i = 0; i < leftOffset; i++) {
            if(current >= size || *text == '\n') continue;
            current++;
            text++;
        }

        for(int col = 0; col < cols; col++) {
            setCursor(row, col);
            if(finishedLine) { pchar(' '); continue; }
            if(current >= size || *text == '\n') { pchar(' '); finishedLine = true; continue; }

            pchar(*text);
            text++;
            current++;
        }

        while(current < size && *text != '\n') {
            text++;
            current++;
        }
        text++;
        current++;
    }

    writeTerm("\e[?25h");
    setCursor(cy, cx);
}

int main(int argc, char **argv) {
    int result = 0;
    if(argc != 2) {
        printf("Please provide a file name\n");
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if(!file) {
        printf("Couldn't open specified file\n");
        return 1;
    }

    int fd = fileno(file);
    if(fd == -1) {
        printf("Couldn't get file's descriptor\n");
        return 1;
    }

    struct stat fileStats = {0};
    result = fstat(fd, &fileStats);
    if(result) {
        printf("Couldn't get file's stats\n");
        return 1;
    }

    uint32_t size = fileStats.st_size;
    char *buffer = calloc(1, size+1);
    result = fread(buffer, sizeof(char), size, file);

    fclose(file);
    fd = -1;
    // fileStats = {0};

    struct termios term, restore;
    tcgetattr(STDIN_FILENO, &term);
    tcgetattr(STDIN_FILENO, &restore);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    writeTerm("\e[?1049h");


    int rf = 0, cf = 0;
    int rcx = 0, rcy = 0;

    renderText(buffer, size, rf, cf, rcx, rcy);

    char input;
    while(true) { 
        read(STDIN_FILENO, &input, 1);
        int w = getScreenWidth();
        int h = getScreenHeight();

        if(input == 'q') break;
        if(input == 'h') {
            if(--rcx < 0) {
                rcx = 0;
                cf = clamp(cf - 1, 0, 100000);
            }
        }
        if(input == 'j') {
            if(++rcy >= h) {
                rcy = h - 1;
                rf = clamp(rf + 1, 0, 100000);
            }
        }
        if(input == 'k') {
            if(--rcy < 0) {
                rcy = 0;
                rf = clamp(rf - 1, 0, 100000);
            }
        }
        if(input == 'l') {
            if(++rcx >= w) {
                rcx = w - 1;
                cf = clamp(cf + 1, 0, 100000);
            }
        }

        setCursor(rcy, rcx);
        renderText(buffer, size, rf, cf, rcx, rcy);
    }

    writeTerm("\e[?1049l");
    tcsetattr(STDIN_FILENO, TCSANOW, &restore);
    return 0;
}
