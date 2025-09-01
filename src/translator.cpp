#include "translator.hpp"
#include <acul/string/sstream.hpp>

namespace ahtt
{

    acul::string build_external(ExternalNode *node, acul::set<acul::string> &includes)
    {
        acul::stringstream ss;
        ss << "struct External\n{\n";
        for (const auto &child : node->children)
        {
            if (child->kind() != INode::Kind::code) continue;
            auto *code_node = static_cast<CodeNode *>(child.get());
            auto str = acul::trim_start(code_node->code);
            if (acul::starts_with(str, "#include "))
                includes.emplace(str);
            else
                ss << '\t' << str << '\n';
        }
        ss << "};\n";
        return ss.str();
    }

 
    void build_html(HTMLNode *node)
    {
        if (acul::starts_with(node->head, "doctype"))
        {
            // acul::string arg = acul::trim_start(node->head.substr(7));
            // if (arg.empty())
            //     printf("<!DOCTYPE html>");
            // else
            //     printf("<!DOCTYPE %s>", arg.c_str());
        }
        else
        {
            auto head = parse_html_head(node->head);
            printf("tag: %s\n", head.tag.data());
        }
        for (const auto &child : node->children)
        {
            if (child->kind() == INode::Kind::html) build_html(static_cast<HTMLNode *>(child.get()));
        }
    }

    void Translator::parse_tokens()
    {
        for (const auto &node : _p.ast)
        {
            switch (node->kind())
            {
                case INode::Kind::external:
                    _external_decl = build_external(static_cast<ExternalNode *>(node.get()), _includes_map);
                    // printf("%s\n", _external_decl.c_str());
                    break;
                case INode::Kind::html:
                    build_html(static_cast<HTMLNode *>(node.get()));
                    break;
                default:
                    break;
            }
        }
    }
} // namespace ahtt