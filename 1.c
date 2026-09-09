#include <stdio.h>

typedef struct {
    int dest_reg, mask, zeroing, src1_reg, src2_is_mem, src2_reg;
    int base_reg, index_reg, scale, disp, has_disp, imm8;
} Instruction;

int my_strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

int my_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void my_strncpy(char *dest, const char *src, int n) {
    for (int i = 0; i < n; i++) {
        dest[i] = src[i];
        if (src[i] == '\0') break;
    }
    dest[n] = '\0';
}

char *my_strchr(const char *s, int c) {
    while (*s) {
        if (*s == c) return (char *)s;
        s++;
    }
    return c == '\0' ? (char *)s : NULL;
}

void my_memset(void *s, int c, unsigned int n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
}

int my_isspace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
int my_isdigit(char c) { return c >= '0' && c <= '9'; }
char my_tolower(char c) { return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; }

int parse_number_exact(const char *s, int *val) {
    if (!*s) return 0;
    int res = 0;
    if (s[0] == '0' && s[1] == 'x') {
        s += 2;
        if (!*s) return 0;
        while (*s) {
            if (*s >= '0' && *s <= '9') res = res * 16 + (*s - '0');
            else if (*s >= 'a' && *s <= 'f') res = res * 16 + (*s - 'a' + 10);
            else return 0;
            s++;
        }
    } else {
        while (*s) {
            if (!my_isdigit(*s)) return 0;
            res = res * 10 + (*s - '0');
            s++;
        }
    }
    *val = res;
    return 1;
}

int parse_zmm_exact(const char *s) {
    if (s[0] == 'z' && s[1] == 'm' && s[2] == 'm' && s[3] != '\0') {
        int reg;
        if (parse_number_exact(s + 3, &reg) && reg >= 0 && reg <= 31) return reg;
    }
    return -1;
}

int parse_ymm_exact(const char *s) {
    if (s[0] == 'y' && s[1] == 'm' && s[2] == 'm' && s[3] != '\0') {
        int reg;
        if (parse_number_exact(s + 3, &reg) && reg >= 0 && reg <= 31) return reg;
    }
    return -1;
}

int parse_gpr_exact(const char* name) {
    const char *regs[] = {"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
                          "r8","r9","r10","r11","r12","r13","r14","r15"};
    for (int i = 0; i < 16; i++) {
        if (my_strcmp(name, regs[i]) == 0) return i;
    }
    return -1;
}

int my_strncmp(const char *s1, const char *s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i])
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        if (s1[i] == '\0')
            return 0;
    }
    return 0;
}

int parse_instruction(const char *input, Instruction *instr) {
    my_memset(instr, 0, sizeof(Instruction));
    instr->base_reg = -1;
    instr->index_reg = -1;
    instr->scale = 1;

    char clean[256];
    int len = 0;
    for (int i = 0; input[i] != '\0'; i++) {
        if (!my_isspace(input[i])) clean[len++] = my_tolower(input[i]);
    }
    clean[len] = '\0';

    if (my_strlen(clean) < 12 || my_strncmp(clean, "vinsertf64x4", 12) != 0) {
        printf("\nExpected mnemonic VINSERTF64X4!\n");
        return 0;
    }
    char *args = clean + 12;

    char *ops[8];
    int op_count = 0;
    ops[op_count++] = args;
    for (char *p = args; *p; p++) {
        if (*p == ',') {
            *p = '\0';
            ops[op_count++] = p + 1;
        }
    }
    if (op_count != 4) {
        printf("\nIncorrect number of operands!\n");
        return 0;
    }

    char *op1 = ops[0];
    char *brace = my_strchr(op1, '{');
    if (brace) {
        *brace = '\0'; 
        char *p = brace + 1;
        while (*p) {
            if (*p == 'k') {
                if (instr->mask != 0) { printf("\nDuplicate mask!\n"); return 0; }
                p++;
                if (*p < '1' || *p > '7') { printf("\nInvalid mask (k1-k7).\n"); return 0; }
                instr->mask = *p - '0';
                p++;
                if (*p != '}') { printf("\nExpected '}' bracket.\n"); return 0; }
            } else if (*p == 'z') {
                if (instr->zeroing != 0) { printf("\nDuplicate {z}!\n"); return 0; }
                instr->zeroing = 1;
                p++;
                if (*p != '}') { printf("\nExpected '}' bracket.\n"); return 0; }
            } else {
                printf("\nInvalid attribute in braces!\n"); return 0;
            }
            p++;
            if (*p == '{') p++;
            else if (*p != '\0') { printf("\nGarbage after mask!\n"); return 0; }
        }
        if (instr->zeroing && instr->mask == 0) { printf("\n{z} without k mask is not allowed.\n"); return 0; }
    }
    instr->dest_reg = parse_zmm_exact(op1);
    if (instr->dest_reg == -1) { printf("\nOperand 1 must be ZMM0..ZMM31.\n"); return 0; }

    instr->src1_reg = parse_zmm_exact(ops[1]);
    if (instr->src1_reg == -1) { printf("\nOperand 2 must be ZMM0..ZMM31.\n"); return 0; }

    char *op3 = ops[2];
    if (op3[0] == '[') {
        instr->src2_is_mem = 1;
        int last = my_strlen(op3) - 1;
        if (op3[last] != ']') { printf("\nMemory address not closed with ']'.\n"); return 0; }
        op3[last] = '\0';
        char *p = op3 + 1;
        if (*p == '\0') { printf("\nEmpty memory brackets are invalid.\n"); return 0; }

        int first = 1;
        while (*p) {
            int sign = 1;
            int has_sign = 0;
            if (*p == '+') { sign = 1; has_sign = 1; p++; }
            else if (*p == '-') { sign = -1; has_sign = 1; p++; }

            if (has_sign && (!*p || *p == '+' || *p == '-')) { 
                printf("\nDouble or dangling operation signs.\n"); return 0; 
            }

            char term[64];
            int tlen = 0;
            while (*p && *p != '+' && *p != '-') term[tlen++] = *p++;
            term[tlen] = '\0';

            int val;
            if (parse_number_exact(term, &val)) {
                instr->has_disp = 1;
                instr->disp += sign * val;
            } else {
                if (has_sign && first) { printf("\nRegister cannot have unary sign.\n"); return 0; }
                if (sign == -1) { printf("\nRegisters cannot be subtracted.\n"); return 0; }

                char *star = my_strchr(term, '*');
                if (star) {
                    *star = '\0';
                    int reg = parse_gpr_exact(term);
                    int scale;
                    if (reg == -1) { printf("\nUnknown index.\n"); return 0; }
                    if (!parse_number_exact(star + 1, &scale)) { printf("\nScale error.\n"); return 0; }
                    if (scale != 1 && scale != 2 && scale != 4 && scale != 8) { printf("\n[ERROR] Scale only 1, 2, 4, 8.\n"); return 0; }
                    if (instr->index_reg != -1) { printf("\nDuplicate index.\n"); return 0; }
                    instr->index_reg = reg;
                    instr->scale = scale;
                } else {
                    int reg = parse_gpr_exact(term);
                    if (reg == -1) { printf("\nUnknown register in address.\n"); return 0; }
                    if (instr->base_reg == -1) instr->base_reg = reg;
                    else if (instr->index_reg == -1) { instr->index_reg = reg; instr->scale = 1; }
                    else { printf("\nToo many registers in address.\n"); return 0; }
                }
            }
            first = 0;
        }

        if (instr->index_reg != -1 && (instr->index_reg & 7) == 4) {
            if (instr->scale == 1) {
                int tmp = instr->base_reg;
                instr->base_reg = instr->index_reg;
                instr->index_reg = tmp;
            } else { printf("\nRSP cannot have a scale.\n"); return 0; }
        }
        if (instr->index_reg != -1 && (instr->index_reg & 7) == 4) { printf("\nRSP cannot be used as index.\n"); return 0; }
        if (instr->has_disp && instr->disp == 0 && instr->base_reg != -1 && (instr->base_reg & 7) != 5) instr->has_disp = 0;

    } else {
        instr->src2_reg = parse_ymm_exact(op3);
        if (instr->src2_reg == -1) { printf("\nOperand 3 must be YMM or memory.\n"); return 0; }
    }

    char *op4 = ops[3];
    int imm_sign = 1;
    if (*op4 == '+') op4++;
    else if (*op4 == '-') { imm_sign = -1; op4++; }

    int imm_val;
    if (!parse_number_exact(op4, &imm_val)) { printf("\nOperand 4 must be a numeric constant.\n"); return 0; }
    imm_val *= imm_sign;
    if (imm_val < -128 || imm_val > 255) { printf("\nOperand 4 is out of 8-bit range.\n"); return 0; }
    instr->imm8 = imm_val & 0xFF;

    return 1;
}

void print_canonical(const Instruction *instr) {
    const char *regs[] = {"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
                          "r8","r9","r10","r11","r12","r13","r14","r15"};
    printf("2) Canonical form:  VINSERTF64X4 zmm%d", instr->dest_reg);
    if (instr->mask) printf("{k%d}", instr->mask);
    if (instr->zeroing) printf("{z}");
    printf(", zmm%d, ", instr->src1_reg);
    if (instr->src2_is_mem) {
        printf("[");
        int printed = 0;
        if (instr->base_reg != -1) { printf("%s", regs[instr->base_reg]); printed = 1; }
        if (instr->index_reg != -1) {
            if (printed) printf(" + ");
            printf("%s*%d", regs[instr->index_reg], instr->scale);
            printed = 1;
        }
        if (instr->has_disp || !printed) {
            if (!printed) {
                if (instr->disp < 0) printf("-0x%X", -instr->disp);
                else printf("0x%X", instr->disp);
            }
            else if (instr->disp >= 0) printf(" + 0x%X", instr->disp);
            else printf(" - 0x%X", -instr->disp);
        }
        printf("]");
    } else printf("ymm%d", instr->src2_reg);
    printf(", 0x%02X\n", instr->imm8);
}

void encode_and_print(const Instruction *instr) {
    unsigned char code[16];
    int code_len = 0;
    
    int mod = 0, rm = 0, disp_size = 0, encoded_disp = 0;
    int use_sib = 0, sib_byte = 0;

    int EVEX_R = ~(instr->dest_reg >> 3) & 1;
    int EVEX_R_prime = ~(instr->dest_reg >> 4) & 1;
    int EVEX_V_prime = ~(instr->src1_reg >> 4) & 1;
    int EVEX_v = ~(instr->src1_reg) & 0xF;
    int EVEX_B = 0, EVEX_X = 1;

    if (!instr->src2_is_mem) {
        mod = 3; rm = instr->src2_reg & 7;
        EVEX_B = ~(instr->src2_reg >> 3) & 1;
        EVEX_X = ~(instr->src2_reg >> 4) & 1;
    } else {
        int eff_base = (instr->base_reg == -1) ? 5 : (instr->base_reg & 7);
        int eff_index = (instr->index_reg == -1) ? 4 : (instr->index_reg & 7);
        
        EVEX_B = (instr->base_reg == -1) ? 1 : (~(instr->base_reg >> 3) & 1);
        EVEX_X = (instr->index_reg == -1) ? 1 : (~(instr->index_reg >> 3) & 1);

        use_sib = (instr->base_reg == -1 || instr->index_reg != -1 || eff_base == 4);

        if (use_sib) {
            int ss = (instr->scale == 8) ? 3 : (instr->scale == 4) ? 2 : (instr->scale == 2) ? 1 : 0;
            sib_byte = (ss << 6) | (eff_index << 3) | eff_base;
        }

        if (instr->base_reg == -1) {
            mod = 0; rm = 4; disp_size = 4; encoded_disp = instr->disp;
        } else {
            if (instr->has_disp == 0) {
                if (eff_base == 5) { mod = 1; rm = use_sib ? 4 : eff_base; disp_size = 1; encoded_disp = 0; }
                else { mod = 0; rm = use_sib ? 4 : eff_base; disp_size = 0; }
            } else {
                if (instr->disp % 32 == 0 && instr->disp >= -4096 && instr->disp <= 4064) {
                    mod = 1; rm = use_sib ? 4 : eff_base; disp_size = 1; encoded_disp = instr->disp / 32;
                } else {
                    mod = 2; rm = use_sib ? 4 : eff_base; disp_size = 4; encoded_disp = instr->disp;
                }
            }
        }
    }

    code[code_len++] = 0x62;
    code[code_len++] = (EVEX_R << 7) | (EVEX_X << 6) | (EVEX_B << 5) | (EVEX_R_prime << 4) | 0x03;
    code[code_len++] = (1 << 7) | (EVEX_v << 3) | (1 << 2) | 0x01;
    code[code_len++] = (instr->zeroing << 7) | (2 << 5) | (0 << 4) | (EVEX_V_prime << 3) | instr->mask;
    code[code_len++] = 0x1A;
    code[code_len++] = (mod << 6) | ((instr->dest_reg & 7) << 3) | rm;
    if (use_sib) code[code_len++] = sib_byte;

    if (disp_size == 1) code[code_len++] = encoded_disp & 0xFF;
    else if (disp_size == 4) {
        code[code_len++] = encoded_disp & 0xFF; code[code_len++] = (encoded_disp >> 8) & 0xFF;
        code[code_len++] = (encoded_disp >> 16) & 0xFF; code[code_len++] = (encoded_disp >> 24) & 0xFF;
    }
    code[code_len++] = instr->imm8;

    printf("3) Machine code:        ");
    for (int i = 0; i < code_len; i++) printf("%02X ", code[i]);
    printf("\n");
}

int main() {
    char input[256], original_input[256];

    printf("Enter AVX-512 command:\n> ");
    if (!fgets(input, sizeof(input), stdin)) return 1;

    int len = my_strlen(input);
    if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';
    
    my_strncpy(original_input, input, len);

    Instruction instr;
    if (!parse_instruction(input, &instr)) return 1;

    printf("1) Original command:    %s\n", original_input);
    print_canonical(&instr);
    encode_and_print(&instr);

    return 0;
}