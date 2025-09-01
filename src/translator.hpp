#pragma once

#include <acul/set.hpp>
#include "parser.hpp"

namespace ahtt
{

    class Translator
    {
    public:
        Translator(Parser &parser) : _p(parser) {}

        void parse_tokens();

    private:
        Parser &_p;
        acul::set<acul::string> _includes_map;
        acul::string _mixins_decl;
        acul::string _external_decl;
        NodeList _ast;
    };
} // namespace ahtt