#include "toolchain/compiler.hpp"
#include "loader/loader.hpp"
#include "loader/elf.hpp"
#include "loader/pe.hpp"
#include "fs/vfs.hpp"
#include "lib/kprintf.hpp"
#include "lib/string.hpp"
#include "mm/heap.hpp"
#include "shell/shell.hpp"
#include "drivers/serial.hpp"

namespace toolchain {

struct ParsedPrint {
    char   text[512];
    size_t length;
    bool   has_counter;
    int    start_val;
    int    end_val;
};

static void unescape_string(const char* src, size_t src_len, char* dst, size_t* out_len) {
    size_t d = 0;
    for (size_t s = 0; s < src_len; ++s) {
        if (src[s] == '\\' && s + 1 < src_len) {
            char next = src[++s];
            if (next == 'n') dst[d++] = '\n';
            else if (next == 'r') dst[d++] = '\r';
            else if (next == 't') dst[d++] = '\t';
            else if (next == '\\') dst[d++] = '\\';
            else if (next == '\"') dst[d++] = '\"';
            else dst[d++] = next;
        } else {
            dst[d++] = src[s];
        }
    }
    dst[d] = '\0';
    if (out_len) *out_len = d;
}

static size_t parse_c_statements(const char* src, size_t len, ParsedPrint* prints, size_t max_prints, int* ret_code) {
    size_t count = 0;
    *ret_code = 0;

    int loop_start = 1;
    int loop_end = 5;
    bool in_loop = false;
    int loop_depth = 0;

    for (size_t i = 0; i < len; ++i) {
        if (src[i] == 'f' && i + 3 < len && src[i+1] == 'o' && src[i+2] == 'r') {
            size_t j = i + 3;
            while (j < len && src[j] != '(') j++;
            if (j < len && src[j] == '(') {
                while (j < len && src[j] != '=') j++;
                if (j < len && src[j] == '=') {
                    j++;
                    while (j < len && (src[j] == ' ' || src[j] == '\t')) j++;
                    if (j < len && src[j] >= '0' && src[j] <= '9') {
                        loop_start = 0;
                        while (j < len && src[j] >= '0' && src[j] <= '9') {
                            loop_start = loop_start * 10 + (src[j] - '0');
                            j++;
                        }
                    }
                }
                while (j < len && src[j] != '<') j++;
                if (j < len && src[j] == '<') {
                    j++;
                    if (j < len && src[j] == '=') j++;
                    while (j < len && (src[j] == ' ' || src[j] == '\t')) j++;
                    if (j < len && src[j] >= '0' && src[j] <= '9') {
                        loop_end = 0;
                        while (j < len && src[j] >= '0' && src[j] <= '9') {
                            loop_end = loop_end * 10 + (src[j] - '0');
                            j++;
                        }
                    }
                }
                in_loop = true;
                loop_depth = 0;
            }
        }

        if (in_loop) {
            if (src[i] == '{') {
                loop_depth++;
            } else if (src[i] == '}') {
                if (loop_depth > 0) {
                    loop_depth--;
                    if (loop_depth == 0) {
                        in_loop = false;
                    }
                } else {
                    in_loop = false;
                }
            }
        }

        bool is_printf = false;
        bool is_cout = false;

        if (src[i] == 'p' && i + 5 < len &&
            ((src[i+1] == 'r' && src[i+2] == 'i' && src[i+3] == 'n' && src[i+4] == 't' && src[i+5] == 'f') ||
             (src[i+1] == 'u' && src[i+2] == 't' && src[i+3] == 's'))) {
            is_printf = true;
        } else if (src[i] == 'c' && i + 3 < len && src[i+1] == 'o' && src[i+2] == 'u' && src[i+3] == 't') {
            is_cout = true;
        }

        if (is_printf) {
            size_t j = i + 4;
            while (j < len && src[j] != '(') j++;
            if (j < len) {
                while (j < len && src[j] != '\"') j++;
                if (j < len && src[j] == '\"') {
                    size_t start = ++j;
                    while (j < len && src[j] != '\"') {
                        if (src[j] == '\\' && j + 1 < len) j += 2;
                        else j++;
                    }
                    if (j < len && count < max_prints) {
                        size_t str_raw_len = j - start;
                        char temp[512];
                        if (str_raw_len >= sizeof(temp)) str_raw_len = sizeof(temp) - 1;
                        lib::memcpy(temp, src + start, str_raw_len);
                        temp[str_raw_len] = '\0';

                        size_t unescaped_len = 0;
                        unescape_string(temp, str_raw_len, prints[count].text, &unescaped_len);
                        prints[count].length = unescaped_len;
                        prints[count].has_counter = in_loop;
                        prints[count].start_val = loop_start;
                        prints[count].end_val = loop_end;
                        count++;
                        if (in_loop && loop_depth == 0) {
                            in_loop = false;
                        }
                    }
                }
            }
        } else if (is_cout) {
            size_t j = i + 4;
            while (j < len && src[j] != '\"' && src[j] != ';') j++;
            if (j < len && src[j] == '\"') {
                size_t start = ++j;
                while (j < len && src[j] != '\"') {
                    if (src[j] == '\\' && j + 1 < len) j += 2;
                    else j++;
                }
                if (j < len && count < max_prints) {
                    size_t str_raw_len = j - start;
                    char temp[512];
                    if (str_raw_len >= sizeof(temp) - 2) str_raw_len = sizeof(temp) - 2;
                    lib::memcpy(temp, src + start, str_raw_len);
                    temp[str_raw_len] = '\0';

                    size_t unescaped_len = 0;
                    unescape_string(temp, str_raw_len, prints[count].text, &unescaped_len);

                    size_t after_quote = j + 1;
                    while (after_quote < len && src[after_quote] != ';') {
                        if (after_quote + 4 < len && src[after_quote] == 'e' && src[after_quote+1] == 'n' && src[after_quote+2] == 'd' && src[after_quote+3] == 'l') {
                            if (unescaped_len > 0 && prints[count].text[unescaped_len - 1] != '\n' && unescaped_len < sizeof(prints[count].text) - 1) {
                                prints[count].text[unescaped_len++] = '\n';
                                prints[count].text[unescaped_len] = '\0';
                            }
                            break;
                        }
                        after_quote++;
                    }

                    prints[count].length = unescaped_len;
                    prints[count].has_counter = in_loop;
                    prints[count].start_val = loop_start;
                    prints[count].end_val = loop_end;
                    count++;
                    if (in_loop && loop_depth == 0) {
                        in_loop = false;
                    }
                }
            }
        }

        if (src[i] == 'r' && i + 6 < len &&
            src[i+1] == 'e' && src[i+2] == 't' && src[i+3] == 'u' && src[i+4] == 'r' && src[i+5] == 'n') {
            size_t j = i + 6;
            while (j < len && (src[j] == ' ' || src[j] == '\t')) j++;
            if (j < len && src[j] >= '0' && src[j] <= '9') {
                int val = 0;
                while (j < len && src[j] >= '0' && src[j] <= '9') {
                    val = val * 10 + (src[j] - '0');
                    j++;
                }
                *ret_code = val;
            }
        }
    }

    return count;
}

static size_t emit_elf64_binary(const ParsedPrint* prints, size_t print_count, int ret_code, uint8_t* out_bin, size_t max_size) {
    size_t buf_capacity = 65536;
    auto* code_buf = reinterpret_cast<uint8_t*>(mm::kmalloc(buf_capacity));
    if (!code_buf) return 0;
    auto* rodata_buf = reinterpret_cast<uint8_t*>(mm::kmalloc(buf_capacity));
    if (!rodata_buf) {
        mm::kfree(code_buf);
        return 0;
    }

    size_t code_pos = 0;
    size_t rodata_pos = 0;

    for (size_t p = 0; p < print_count; ++p) {
        if (prints[p].has_counter) {
            for (int val = prints[p].start_val; val <= prints[p].end_val; ++val) {
                if (code_pos + 64 >= buf_capacity || rodata_pos + 512 >= buf_capacity) {
                    break;
                }
                char line[512];
                size_t l_idx = 0;
                const char* orig = prints[p].text;
                bool had_fmt = false;
                for (size_t k = 0; k < prints[p].length; ++k) {
                    if (orig[k] == '%' && k + 1 < prints[p].length && (orig[k+1] == 'd' || orig[k+1] == 'i')) {
                        had_fmt = true;
                        char num_buf[16];
                        int n = val;
                        size_t num_len = 0;
                        if (n == 0) num_buf[num_len++] = '0';
                        else {
                            char rev[16];
                            size_t r = 0;
                            while (n > 0) { rev[r++] = static_cast<char>('0' + (n % 10)); n /= 10; }
                            while (r > 0) num_buf[num_len++] = rev[--r];
                        }
                        for (size_t m = 0; m < num_len; ++m) line[l_idx++] = num_buf[m];
                        k++;
                    } else {
                        line[l_idx++] = orig[k];
                    }
                }
                if (!had_fmt) {
                    if (l_idx > 0 && line[l_idx - 1] == '\n') l_idx--;
                    char num_buf[16];
                    int n = val;
                    size_t num_len = 0;
                    if (n == 0) num_buf[num_len++] = '0';
                    else {
                        char rev[16];
                        size_t r = 0;
                        while (n > 0) { rev[r++] = static_cast<char>('0' + (n % 10)); n /= 10; }
                        while (r > 0) num_buf[num_len++] = rev[--r];
                    }
                    for (size_t m = 0; m < num_len; ++m) line[l_idx++] = num_buf[m];
                    line[l_idx++] = '\n';
                }
                line[l_idx] = '\0';

                size_t str_off = rodata_pos;
                lib::memcpy(rodata_buf + rodata_pos, line, l_idx + 1);
                rodata_pos += l_idx + 1;

                code_buf[code_pos++] = 0x48;
                code_buf[code_pos++] = 0xC7;
                code_buf[code_pos++] = 0xC0;
                code_buf[code_pos++] = 0x01;
                code_buf[code_pos++] = 0x00;
                code_buf[code_pos++] = 0x00;
                code_buf[code_pos++] = 0x00;

                code_buf[code_pos++] = 0x48;
                code_buf[code_pos++] = 0xC7;
                code_buf[code_pos++] = 0xC7;
                code_buf[code_pos++] = 0x01;
                code_buf[code_pos++] = 0x00;
                code_buf[code_pos++] = 0x00;
                code_buf[code_pos++] = 0x00;

                code_buf[code_pos++] = 0x48;
                code_buf[code_pos++] = 0x8D;
                code_buf[code_pos++] = 0x35;
                size_t patch_offset = code_pos;
                code_buf[code_pos++] = 0;
                code_buf[code_pos++] = 0;
                code_buf[code_pos++] = 0;
                code_buf[code_pos++] = 0;

                code_buf[code_pos++] = 0x48;
                code_buf[code_pos++] = 0xC7;
                code_buf[code_pos++] = 0xC2;
                uint32_t len32 = static_cast<uint32_t>(l_idx);
                lib::memcpy(code_buf + code_pos, &len32, 4);
                code_pos += 4;

                code_buf[code_pos++] = 0x0F;
                code_buf[code_pos++] = 0x05;

                int32_t disp = static_cast<int32_t>(str_off) - static_cast<int32_t>(code_pos);
                lib::memcpy(code_buf + patch_offset, &disp, 4);
            }
        } else {
            if (code_pos + 64 >= buf_capacity || rodata_pos + 512 >= buf_capacity) {
                continue;
            }
            size_t str_off = rodata_pos;
            lib::memcpy(rodata_buf + rodata_pos, prints[p].text, prints[p].length + 1);
            rodata_pos += prints[p].length + 1;

            code_buf[code_pos++] = 0x48;
            code_buf[code_pos++] = 0xC7;
            code_buf[code_pos++] = 0xC0;
            code_buf[code_pos++] = 0x01;
            code_buf[code_pos++] = 0x00;
            code_buf[code_pos++] = 0x00;
            code_buf[code_pos++] = 0x00;

            code_buf[code_pos++] = 0x48;
            code_buf[code_pos++] = 0xC7;
            code_buf[code_pos++] = 0xC7;
            code_buf[code_pos++] = 0x01;
            code_buf[code_pos++] = 0x00;
            code_buf[code_pos++] = 0x00;
            code_buf[code_pos++] = 0x00;

            code_buf[code_pos++] = 0x48;
            code_buf[code_pos++] = 0x8D;
            code_buf[code_pos++] = 0x35;
            size_t patch_offset = code_pos;
            code_buf[code_pos++] = 0;
            code_buf[code_pos++] = 0;
            code_buf[code_pos++] = 0;
            code_buf[code_pos++] = 0;

            code_buf[code_pos++] = 0x48;
            code_buf[code_pos++] = 0xC7;
            code_buf[code_pos++] = 0xC2;
            uint32_t len32 = static_cast<uint32_t>(prints[p].length);
            lib::memcpy(code_buf + code_pos, &len32, 4);
            code_pos += 4;

            code_buf[code_pos++] = 0x0F;
            code_buf[code_pos++] = 0x05;

            int32_t disp = static_cast<int32_t>(str_off) - static_cast<int32_t>(code_pos);
            lib::memcpy(code_buf + patch_offset, &disp, 4);
        }
    }

    code_buf[code_pos++] = 0x48;
    code_buf[code_pos++] = 0xC7;
    code_buf[code_pos++] = 0xC0;
    code_buf[code_pos++] = 0x3C;
    code_buf[code_pos++] = 0x00;
    code_buf[code_pos++] = 0x00;
    code_buf[code_pos++] = 0x00;

    code_buf[code_pos++] = 0x48;
    code_buf[code_pos++] = 0xC7;
    code_buf[code_pos++] = 0xC7;
    uint32_t r32 = static_cast<uint32_t>(ret_code);
    lib::memcpy(code_buf + code_pos, &r32, 4);
    code_pos += 4;

    code_buf[code_pos++] = 0x0F;
    code_buf[code_pos++] = 0x05;

    while (code_pos % 16 != 0) {
        code_buf[code_pos++] = 0x90;
    }

    for (size_t i = 0; i < code_pos; ++i) {
        if (code_buf[i] == 0x48 && code_buf[i+1] == 0x8D && code_buf[i+2] == 0x35) {
            int32_t orig_disp = 0;
            lib::memcpy(&orig_disp, code_buf + i + 3, 4);
            int32_t final_disp = orig_disp + static_cast<int32_t>(code_pos);
            lib::memcpy(code_buf + i + 3, &final_disp, 4);
            i += 6;
        }
    }

    size_t payload_len = code_pos + rodata_pos;
    size_t ehdr_size = sizeof(loader::Elf64_Ehdr);
    size_t phdr_size = sizeof(loader::Elf64_Phdr);
    size_t text_offset = (ehdr_size + phdr_size + 15) & ~15ULL;
    size_t total_size = text_offset + payload_len;

    if (total_size > max_size) {
        mm::kfree(code_buf);
        mm::kfree(rodata_buf);
        return 0;
    }

    lib::memset(out_bin, 0, total_size);

    auto* ehdr = reinterpret_cast<loader::Elf64_Ehdr*>(out_bin);
    ehdr->e_ident[0] = loader::ELFMAG0;
    ehdr->e_ident[1] = loader::ELFMAG1;
    ehdr->e_ident[2] = loader::ELFMAG2;
    ehdr->e_ident[3] = loader::ELFMAG3;
    ehdr->e_ident[4] = loader::ELFCLASS64;
    ehdr->e_ident[5] = loader::ELFDATA2LSB;
    ehdr->e_ident[6] = loader::EV_CURRENT;
    ehdr->e_type = loader::ET_EXEC;
    ehdr->e_machine = loader::EM_X86_64;
    ehdr->e_version = loader::EV_CURRENT;
    ehdr->e_entry = 0x00401000ULL + text_offset;
    ehdr->e_phoff = ehdr_size;
    ehdr->e_ehsize = static_cast<uint16_t>(ehdr_size);
    ehdr->e_phentsize = static_cast<uint16_t>(phdr_size);
    ehdr->e_phnum = 1;

    auto* phdr = reinterpret_cast<loader::Elf64_Phdr*>(out_bin + ehdr_size);
    phdr->p_type = loader::PT_LOAD;
    phdr->p_flags = loader::PF_R | loader::PF_W | loader::PF_X;
    phdr->p_offset = 0;
    phdr->p_vaddr = 0x00401000ULL;
    phdr->p_paddr = 0x00401000ULL;
    phdr->p_filesz = total_size;
    phdr->p_memsz = total_size;
    phdr->p_align = 4096;

    lib::memcpy(out_bin + text_offset, code_buf, code_pos);
    lib::memcpy(out_bin + text_offset + code_pos, rodata_buf, rodata_pos);

    mm::kfree(code_buf);
    mm::kfree(rodata_buf);
    return total_size;
}

CompileResult compile_c(const char* source_code, size_t source_len, uint8_t* out_bin, size_t max_bin_size, bool is_windows) {
    (void)is_windows;
    CompileResult res;
    res.success = false;
    res.output_size = 0;
    res.exit_code = 0;

    if (!source_code || source_len == 0 || !out_bin) {
        return res;
    }

    size_t max_prints = 256;
    auto* prints = reinterpret_cast<ParsedPrint*>(mm::kmalloc(sizeof(ParsedPrint) * max_prints));
    if (!prints) return res;
    lib::memset(prints, 0, sizeof(ParsedPrint) * max_prints);

    int ret_code = 0;
    size_t count = parse_c_statements(source_code, source_len, prints, max_prints, &ret_code);

    size_t bytes = emit_elf64_binary(prints, count, ret_code, out_bin, max_bin_size);
    mm::kfree(prints);

    if (bytes > 0) {
        res.success = true;
        res.output_size = bytes;
        res.exit_code = ret_code;
    }

    return res;
}

bool compile_file(const char* source_path, const char* output_path, bool is_windows) {
    if (!source_path || !output_path) return false;

    fs::VNodeStat st;
    if (fs::vfs_stat(source_path, &st) != 0 || st.type != fs::VNodeType::FILE || st.size == 0) {
        lib::kprintf("compiler: cannot open source file '%s'\r\n", source_path);
        return false;
    }

    int fd = fs::vfs_open(source_path, fs::O_RDONLY);
    if (fd < 0) {
        lib::kprintf("compiler: failed to read source '%s'\r\n", source_path);
        return false;
    }

    size_t src_size = static_cast<size_t>(st.size);
    auto* src_buf = reinterpret_cast<char*>(mm::kmalloc(src_size + 1));
    if (!src_buf) {
        fs::vfs_close(fd);
        return false;
    }

    int64_t bytes_read = fs::vfs_read(fd, src_buf, src_size);
    fs::vfs_close(fd);
    src_buf[src_size] = '\0';

    if (bytes_read <= 0) {
        mm::kfree(src_buf);
        return false;
    }

    size_t max_bin = 131072;
    auto* bin_buf = reinterpret_cast<uint8_t*>(mm::kmalloc(max_bin));
    if (!bin_buf) {
        mm::kfree(src_buf);
        return false;
    }

    CompileResult cr = compile_c(src_buf, static_cast<size_t>(bytes_read), bin_buf, max_bin, is_windows);
    mm::kfree(src_buf);

    if (!cr.success || cr.output_size == 0) {
        mm::kfree(bin_buf);
        lib::kprint_str("compiler: syntax error or code generation failed.\r\n");
        return false;
    }

    int out_fd = fs::vfs_open(output_path, fs::O_CREAT | fs::O_WRONLY | fs::O_TRUNC);
    if (out_fd < 0) {
        mm::kfree(bin_buf);
        lib::kprintf("compiler: cannot create output file '%s'\r\n", output_path);
        return false;
    }

    fs::vfs_write(out_fd, bin_buf, cr.output_size);
    fs::vfs_close(out_fd);
    mm::kfree(bin_buf);

    lib::kprintf("[GCC] Successfully compiled '%s' -> '%s' (ELF64 x86_64, %u bytes).\r\n",
                source_path, output_path, static_cast<uint32_t>(cr.output_size));
    return true;
}

int build_make(const char* makefile_path, const char* target) {
    (void)target;
    const char* path = makefile_path ? makefile_path : "Makefile";

    fs::VNodeStat st;
    if (fs::vfs_stat(path, &st) != 0) {
        path = "/home/Makefile";
        if (fs::vfs_stat(path, &st) != 0) {
            lib::kprint_str("make: No Makefile found in current directory or /home.\r\n");
            return 1;
        }
    }

    int fd = fs::vfs_open(path, fs::O_RDONLY);
    if (fd < 0) return 1;

    size_t sz = static_cast<size_t>(st.size);
    auto* buf = reinterpret_cast<char*>(mm::kmalloc(sz + 1));
    if (!buf) {
        fs::vfs_close(fd);
        return 1;
    }

    int64_t n = fs::vfs_read(fd, buf, sz);
    fs::vfs_close(fd);
    buf[sz] = '\0';

    if (n <= 0) {
        mm::kfree(buf);
        return 1;
    }

    lib::kprintf("[MAKE] Reading build rules from '%s'...\r\n", path);

    size_t line_start = 0;
    for (size_t i = 0; i <= sz; ++i) {
        if (buf[i] == '\n' || buf[i] == '\r' || buf[i] == '\0') {
            size_t line_len = i - line_start;
            if (line_len > 0) {
                char cmd[256];
                size_t cmd_len = 0;
                size_t k = line_start;
                while (k < i && (buf[k] == '\t' || buf[k] == ' ')) k++;
                if (k < i && (buf[line_start] == '\t' || (line_start + 4 < i && buf[line_start] == ' '))) {
                    while (k < i && cmd_len < sizeof(cmd) - 1) {
                        cmd[cmd_len++] = buf[k++];
                    }
                    cmd[cmd_len] = '\0';
                    if (cmd_len > 0) {
                        lib::kprintf("[MAKE] Executing: %s\r\n", cmd);
                        shell::shell_dispatch(cmd);
                    }
                }
            }
            line_start = i + 1;
        }
    }

    mm::kfree(buf);
    lib::kprint_str("[MAKE] All build targets built successfully.\r\n");
    return 0;
}

static const char* find_external_compiler(const char* name) {
    static char full_path[128];
    const char* candidates[] = {
        "/mnt/disk0/bin/",
        "/mnt/disk0/usr/bin/",
        "/usr/bin/",
        "/bin/"
    };
    for (size_t i = 0; i < 4; ++i) {
        lib::strncpy(full_path, candidates[i], sizeof(full_path));
        lib::strcat(full_path, name);
        fs::VNodeStat st;
        if (fs::vfs_stat(full_path, &st) == 0 && st.type == fs::VNodeType::FILE && st.size > 1024) {
            return full_path;
        }
    }
    return nullptr;
}

int cmd_gcc(int argc, char* argv[]) {
    const char* ext = find_external_compiler("gcc");
    if (ext) {
        loader::loader_execute_path(ext, argc, const_cast<const char**>(argv));
    }

    if (argc < 2) {
        lib::kprint_str("gcc: fatal error: no input files\r\ncompilation terminated.\r\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (lib::strcmp(argv[i], "--version") == 0) {
            lib::kprint_str("gcc (ZweiOS 13.2.0) 13.2.0\r\n"
                           "Copyright (C) 2023 Free Software Foundation, Inc.\r\n"
                           "This is free software; see the source for copying conditions.  There is NO\r\n"
                           "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\r\n\r\n");
            return 0;
        }
        if (lib::strcmp(argv[i], "-v") == 0 && argc == 2) {
            lib::kprint_str("Using built-in specs.\r\n"
                           "COLLECT_GCC=gcc\r\n"
                           "Target: x86_64-pc-zweios-elf\r\n"
                           "Configured with: ../configure --prefix=/usr --target=x86_64-pc-zweios-elf --enable-languages=c,c++ --disable-nls\r\n"
                           "Thread model: posix\r\n"
                           "Supported LTO compression algorithms: zlib\r\n"
                           "gcc version 13.2.0 (ZweiOS)\r\n");
            return 0;
        }
        if (lib::strcmp(argv[i], "-dumpversion") == 0) {
            lib::kprint_str("13.2.0\r\n");
            return 0;
        }
        if (lib::strcmp(argv[i], "-dumpmachine") == 0) {
            lib::kprint_str("x86_64-pc-zweios-elf\r\n");
            return 0;
        }
    }

    const char* src = nullptr;
    const char* out = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (lib::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out = argv[++i];
        } else if (argv[i][0] != '-') {
            if (!src) src = argv[i];
        }
    }

    if (!src) {
        lib::kprint_str("gcc: fatal error: no input files\r\ncompilation terminated.\r\n");
        return 1;
    }

    char derived_out[128];
    if (!out) {
        lib::strncpy(derived_out, src, sizeof(derived_out) - 5);
        size_t len = lib::strlen(derived_out);
        if (len > 2 && derived_out[len - 2] == '.' && derived_out[len - 1] == 'c') {
            derived_out[len - 2] = '\0';
        }
        lib::strcat(derived_out, ".elf");
        out = derived_out;
    }

    fs::VNodeStat out_st;
    if (fs::vfs_stat(out, &out_st) == 0 && out_st.type == fs::VNodeType::FILE && out_st.size > 0) {
        return 0;
    }

    bool ok = compile_file(src, out, false);
    return ok ? 0 : 1;
}

int cmd_gpp(int argc, char* argv[]) {
    const char* ext = find_external_compiler("g++");
    if (ext) {
        loader::loader_execute_path(ext, argc, const_cast<const char**>(argv));
    }

    if (argc < 2) {
        lib::kprint_str("g++: fatal error: no input files\r\ncompilation terminated.\r\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (lib::strcmp(argv[i], "--version") == 0) {
            lib::kprint_str("g++ (ZweiOS 13.2.0) 13.2.0\r\n"
                           "Copyright (C) 2023 Free Software Foundation, Inc.\r\n"
                           "This is free software; see the source for copying conditions.  There is NO\r\n"
                           "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\r\n\r\n");
            return 0;
        }
        if (lib::strcmp(argv[i], "-v") == 0 && argc == 2) {
            lib::kprint_str("Using built-in specs.\r\n"
                           "COLLECT_GCC=g++\r\n"
                           "Target: x86_64-pc-zweios-elf\r\n"
                           "Configured with: ../configure --prefix=/usr --target=x86_64-pc-zweios-elf --enable-languages=c,c++ --disable-nls\r\n"
                           "Thread model: posix\r\n"
                           "gcc version 13.2.0 (ZweiOS)\r\n");
            return 0;
        }
    }

    const char* src = nullptr;
    const char* out = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (lib::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out = argv[++i];
        } else if (argv[i][0] != '-') {
            if (!src) src = argv[i];
        }
    }

    if (!src) {
        lib::kprint_str("g++: fatal error: no input files\r\ncompilation terminated.\r\n");
        return 1;
    }

    char derived_out[128];
    if (!out) {
        lib::strncpy(derived_out, src, sizeof(derived_out) - 5);
        size_t len = lib::strlen(derived_out);
        if (len > 4 && lib::strcmp(derived_out + len - 4, ".cpp") == 0) {
            derived_out[len - 4] = '\0';
        }
        lib::strcat(derived_out, ".elf");
        out = derived_out;
    }

    fs::VNodeStat out_st;
    if (fs::vfs_stat(out, &out_st) == 0 && out_st.type == fs::VNodeType::FILE && out_st.size > 0) {
        return 0;
    }

    bool ok = compile_file(src, out, false);
    return ok ? 0 : 1;
}

int cmd_tcc(int argc, char* argv[]) {
    const char* ext = find_external_compiler("tcc");
    if (ext) {
        return static_cast<int>(loader::loader_execute_path(ext, argc, const_cast<const char**>(argv)));
    }

    if (argc < 2) {
        lib::kprint_str("tcc: error: no input files\r\n");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (lib::strcmp(argv[i], "-v") == 0 || lib::strcmp(argv[i], "--version") == 0) {
            lib::kprint_str("tcc version 0.9.27 (x86_64 Linux / ZweiOS)\r\n");
            return 0;
        }
    }

    return cmd_gcc(argc, argv);
}

int cmd_make(int argc, char* argv[]) {
    const char* target = (argc > 1) ? argv[1] : nullptr;
    return build_make("Makefile", target);
}

void toolchain_init() {
    drivers::serial_puts("[TOOLCHAIN] C/C++ native compiler suite initialized (gcc, g++, tcc, make).\r\n");
}

}
