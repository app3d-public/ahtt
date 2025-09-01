#include "linker.hpp"
#include "translator.hpp"

int main(int argc, char *argv[])
{
    acul::init_simd_module();
    if (acul::get_simd_flags() == acul::simd_flag_bits::initialized) printf("Failed to load simd module\n");

    acul::io::path file_path{"test_page.at"};
    acul::io::path views_path{"views"};
    ahtt::Parser p;
    ahtt::load_template(views_path / file_path, p);
    ahtt::Linker l(p);
    l.link(views_path);
    ahtt::Translator tr(p);
    tr.parse_tokens();

    // l.dump();

    // Main parse
    // auto toks_main = lex_with_indents(file_content);
    // Parser p_main{toks_main};
    // p_main.parse();
    // // dump_ast(p_main.ast);
    // printf("template parsed (pass 0). Extends: %p. Blocks: %u\n", p_main.extends, p_main.block_map.size());

    // Layout parse
    // auto toks_layout = lex_with_indents(layout_content);
    // Parser p_layout{toks_layout};
    // p_layout.parse();
    // // dump_ast(p_layout.ast);
    // printf("layout parsed (pass 0). Extends: %p. Blocks: %u\n", p_layout.extends, p_layout.block_map.size());

    // if (p_main.extends)
    // {
    //     Linker linker{p_main, p_layout};
    //     linker.link();
    //     dump_ast(linker.ast);
    // }

    return 0;
}