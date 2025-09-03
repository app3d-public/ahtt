#pragma once

#include <acul/io/file.hpp>
#include <acul/io/path.hpp>
#include <acul/log.hpp>
#include "parser.hpp"

namespace ahtt
{
    inline void load_template(const acul::io::path &path, Parser &p)
    {
        LOG_INFO("Loading template file: %s", path.str().c_str());
        acul::vector<char> file_buffer;
        if (acul::io::file::read_binary(path.str(), file_buffer) != acul::io::file::op_state::success)
            throw acul::runtime_error(acul::format("Failed to read template file: %s", path.str().c_str()));

        acul::string_pool<char> pool(file_buffer.size());
        acul::io::file::fill_line_buffer(file_buffer.data(), file_buffer.size(), pool);

        p.ts = ahtt::lex_with_indents(pool);
        p.parse();
    }

    class Linker
    {
    public:
        Linker(Parser &p) : _template(p) {}

        void link(const acul::io::path &base_path);

    private:
        Parser &_template;
    };
} // namespace ahtt
