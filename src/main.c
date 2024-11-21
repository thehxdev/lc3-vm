/**
 * LC3-VM
 * Author: Hossein Khosravi (@thehxdev)
 * Description: A virtual machine for LC3 architecture
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

#include "platform.h"

#ifdef LC3_PLAT_UNIX
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <sys/termios.h>
    #include <sys/mman.h>
#else
    #include <Windows.h>
    #include <conio.h>
#endif // platform specific includes


#define MEMORY_MAX (1<<16)

// Registers
enum {
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT,
};

// Flags
enum {
    FL_POS = (1<<0), // P
    FL_ZRO = (1<<1), // Z
    FL_NEG = (1<<2), // N
};

// Opcodes
enum {
    OP_BR = 0,  // Branch
    OP_ADD,     // add
    OP_LD,      // load
    OP_ST,      // store
    OP_JSR,     // jump register
    OP_AND,     // bitwise and
    OP_LDR,     // load register
    OP_STR,     // store register
    OP_RTI,     // unused
    OP_NOT,     // bitwise not
    OP_LDI,     // load indirect
    OP_STI,     // store indirect
    OP_JMP,     // jump
    OP_RES,     // reserved (unused)
    OP_LEA,     // load effective address
    OP_TRAP,    // execute trap
};

// Trap codes
enum {
    TRAP_GETC  = 0x20,  // get character from keyboard, NOT echoed onto the terminal
    TRAP_OUT   = 0x21,  // output a character
    TRAP_PUTS  = 0x22,  // output a word string
    TRAP_IN    = 0x23,  // get character from keyboard, echoed onto the terminal
    TRAP_PUTSP = 0x24,  // output a byte string
    TRAP_HALT  = 0x25,  // halt the program
};

enum {
    MR_KBSR = 0xfe00,   // keyboard status
    MR_KBDR = 0xfe02,   // keyboard data
};

uint16_t memory[MEMORY_MAX];
uint16_t reg[R_COUNT];

#ifdef LC3_PLAT_UNIX
struct termios original_tio;
#else
HANDLE hStdin = INVALID_HANDLE_VALUE;
DWORD fdwMode, fdwOldMode;
#endif

static void disable_input_buffering(void) {
#ifdef LC3_PLAT_UNIX
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
#else
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOldMode); // save old mode
    fdwMode = fdwOldMode
            ^ ENABLE_ECHO_INPUT  // no input echo
            ^ ENABLE_LINE_INPUT; // return when one or more characters are available
    SetConsoleMode(hStdin, fdwMode); // set new mode
    FlushConsoleInputBuffer(hStdin); // clear buffer
#endif
}

static void restore_input_buffering(void) {
#ifdef LC3_PLAT_UNIX
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
#else
    SetConsoleMode(hStdin, fdwOldMode);
#endif
}

static uint16_t check_key(void) {
#ifdef LC3_PLAT_UNIX
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
#else
    return WaitForSingleObject(hStdin, 1000) == WAIT_OBJECT_0 && _kbhit();
#endif
}

static uint16_t mem_read(uint16_t addr) {
    if (addr == MR_KBSR) {
        if (check_key()) {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        } else {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[addr];
}

static void mem_write(uint16_t addr, uint16_t value) {
    memory[addr] = value;
}

static uint16_t sign_extend(uint16_t num, int bit_count) {
    if ((num >> (bit_count-1)) & 0x1) {
        num |= (0xffff << bit_count);
    }
    return num;
}

static void update_flags(uint16_t r) {
    if (reg[r] == 0)
        reg[R_COND] = FL_ZRO;
    else if (reg[r] >> 15)
        reg[R_COND] = FL_NEG;
    else
        reg[R_COND] = FL_POS;
}

static uint16_t swap16(uint16_t x) {
    return ((x << 8) | (x >> 8));
}

static void read_image_file(FILE *fp) {
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, fp);
    origin = swap16(origin);

    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t *p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, fp);

    // convert to little endian
    while (read-- > 0) {
        *p = swap16(*p);
        ++p;
    }
}

static int read_image(const char path[]) {
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;
    read_image_file(fp);
    fclose(fp);
    return 1;
}

static void shutdown(void) {
    restore_input_buffering();
}

static void handle_interrupt(int signal) {
    (void)signal;

    shutdown();
    printf("\n");
    exit(-2);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("%s [image-file1] ...\n", argv[0]);
        exit(2);
    }

    for (int j = 0; j < argc; j++) {
        if (!read_image(argv[j])) {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }

    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    // set Z flag
    reg[R_COND] = FL_ZRO;

    // set PC to starting position
    // 0x3000 is the default
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    int running = 1;
    while (running) {
        // Fetch
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = (instr >> 12);

        switch (op) {
            case OP_ADD:
                {
                    uint16_t dr  = (instr >> 9) & 0x7;
                    uint16_t sr1 = (instr >> 6) & 0x7;
                    uint16_t imm_flag = (instr >> 5) & 0x1;

                    if (imm_flag) {
                        uint16_t imm5 = sign_extend(instr & 0x1f, 5);
                        reg[dr] = reg[sr1] + imm5;
                    } else {
                        uint16_t sr2 = instr & 0x7;
                        reg[dr] = reg[sr1] + reg[sr2];
                    }

                    update_flags(dr);
                }
                break;

            case OP_AND:
                {
                    uint16_t dr  = (instr >> 9) & 0x7;
                    uint16_t sr1 = (instr >> 6) & 0x7;
                    uint16_t imm_flag = (instr >> 5) & 0x1;

                    if (imm_flag) {
                        uint16_t imm5 = sign_extend(instr & 0x1f, 5);
                        reg[dr] = reg[sr1] & imm5;
                    } else {
                        uint16_t sr2 = instr & 0x7;
                        reg[dr] = reg[sr1] & reg[sr2];
                    }

                    update_flags(dr);
                }
                break;

            case OP_NOT:
                {
                    uint16_t dr = (instr >> 9) & 0x7;
                    uint16_t sr = (instr >> 6) & 0x7;
                    reg[dr] = ~reg[sr];
                    update_flags(dr);
                }
                break;

            case OP_BR:
                {
                    uint8_t flags = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1ff, 9);
                    if (flags & reg[R_COND])
                        reg[R_PC] += pc_offset;
                }
                break;

            case OP_JMP:
                reg[R_PC] = reg[(instr >> 6) & 0x7];
                break;

            case OP_JSR:
                {
                    uint8_t flag = (instr >> 11) & 0x1;
                    reg[R_R7] = reg[R_PC];
                    if (flag)
                        reg[R_PC] += sign_extend(instr & 0x7ff, 11);
                    else
                        reg[R_PC] = reg[(instr >> 6) & 0x7];
                }
                break;

            case OP_LD:
                {
                    uint16_t dr = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1ff, 9);
                    reg[dr] = mem_read(reg[R_PC] + pc_offset);
                    update_flags(dr);
                }
                break;

            case OP_LDI:
                {
                    uint16_t dr = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend((instr & 0x1ff), 9);
                    reg[dr] = mem_read(mem_read(reg[R_PC] + pc_offset));
                    update_flags(dr);
                }
                break;

            case OP_LDR:
                {
                    uint16_t offset = sign_extend(instr & 0x3f, 6);
                    uint16_t baseR  = (instr >> 6) & 0x7;
                    uint16_t dr = (instr >> 9) & 0x7;
                    reg[dr] = mem_read(reg[baseR] + offset);
                    update_flags(dr);
                }
                break;

            case OP_LEA:
                {
                    uint16_t offset = sign_extend(instr & 0x1ff, 9);
                    uint16_t dr = (instr >> 9) & 0x7;
                    reg[dr] = reg[R_PC] + offset;
                    update_flags(dr);
                }
                break;

            case OP_ST:
                {
                    uint16_t pc_offset = sign_extend(instr & 0x1ff, 9);
                    uint16_t sr = (instr >> 9) & 0x7;
                    mem_write(reg[R_PC] + pc_offset, reg[sr]);
                }
                break;

            case OP_STI:
                {
                    uint16_t pc_offset = sign_extend(instr & 0x1ff, 9);
                    uint16_t sr = (instr >> 9) & 0x7;
                    mem_write(mem_read(reg[R_PC] + pc_offset), reg[sr]);
                }
                break;

            case OP_STR:
                {
                    uint16_t offset = sign_extend(instr & 0x3f, 6);
                    uint16_t baseR = (instr >> 6) & 0x7;
                    uint16_t sr = (instr >> 9) & 0x7;
                    mem_write(reg[baseR] + offset, reg[sr]);
                }
                break;

            case OP_TRAP:
                {
                    reg[R_R7] = reg[R_PC];
                    switch (instr & 0xff) {
                        case TRAP_GETC:
                            reg[R_R0] = (uint16_t)getchar();
                            update_flags(R_R0);
                            break;

                        case TRAP_OUT:
                            fputc((char)reg[R_R0], stdout);
                            fflush(stdout);
                            break;

                        case TRAP_PUTS:
                            {
                                uint16_t *c = memory + reg[R_R0];
                                while (*c) {
                                    fputc((char)*c, stdout);
                                    ++c;
                                }
                                fflush(stdout);
                            }
                            break;

                        case TRAP_IN:
                            {
                                printf("Enter a character: ");
                                char c = getchar();
                                reg[R_R0] = (uint16_t)c;
                                fputc(c, stdout);
                                update_flags(R_R0);
                            }
                            break;

                        case TRAP_PUTSP:
                            {
                                uint16_t *c = memory + reg[R_R0];
                                while (*c) {
                                    fputc((*c) & 0xff, stdout);
                                    char c2 = (*c) >> 8;
                                    if (c2)
                                        fputc(c2, stdout);
                                    ++c;
                                }
                                fflush(stdout);
                            }
                            break;

                        case TRAP_HALT:
                            {
                                fputs("HALT", stdout);
                                running = 0;
                            }
                            break;
                    }
                }
                break;

            case OP_RES:
            case OP_RTI:
            default:
                abort();
                break;
        } // End switch(op)
    } // End while(running)

    shutdown();
}
