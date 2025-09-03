#pragma once

#include <acul/hash/hashset.hpp>
#include <acul/string/sstream.hpp>
#include "parser.hpp"

#define AHTT_PARSE_DEFAULT     0x0
#define AHTT_PARSE_BLOCK_ADDED 0x1

namespace ahtt
{
    class Translator
    {
    public:
        Translator(Parser &parser) : _p(parser) {}

        void parse_tokens()
        {
            NodeList ast;
            parse_tokens(_p.ast, ast);
            dump_ast(ast);
            _ast = std::move(ast);
        }

        void write_to_stream(acul::stringstream &ss, const acul::string &template_name);

    private:
        Parser &_p;
        acul::hashset<acul::string> _includes_map;
        acul::hashmap<acul::string, acul::unique_ptr<MixinDecl>> _mixins_map;
        acul::unique_ptr<ExternalNode> _external;
        HTMLNode *_doctype = nullptr;
        NodeList _ast;

        int build_html(NodeList &ast, HTMLNode *node);
        void build_external_node(ExternalNode *current);
        int parse_node(INode *node, NodeList &ast);

        inline int parse_tokens(NodeList &elements, NodeList &ast)
        {
            int flags = AHTT_PARSE_DEFAULT;
            for (auto &node : elements) flags |= parse_node(node.get(), ast);
            return flags;
        }

        inline void parse_mixin(MixinDecl *origin, MixinDecl *m)
        {
            m->name = origin->name;
            m->args = std::move(origin->args);
            m->pos = origin->pos;
            int flags = parse_tokens(origin->children, m->children);
            m->has_block = flags & AHTT_PARSE_BLOCK_ADDED;
        }

        acul::stringstream &write_node_list(acul::stringstream &ss, const NodeList &nodes, const char *ss_out,
                                            const char *indent);
    };
} // namespace ahtt