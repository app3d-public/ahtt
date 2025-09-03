#pragma once

#include <acul/hash/hashmap.hpp>
#include <acul/memory/smart_ptr.hpp>
#include <acul/string/string_pool.hpp>
#include <acul/string/utils.hpp>
#include <acul/vector.hpp>

namespace ahtt
{
    // -------------------- Pos --------------------
    struct Pos
    {
        int line = 1, col = 1;
    };

    // -------------------- Tokens --------------------
    struct Tok
    {
        enum Kind
        {
            line,
            indent,
            dedent,
            eof,
            blank
        } kind;
        acul::string_view sv; // only for Line
        Pos pos{};
        int level = 0;
    };

    struct INode
    {
        enum class Kind
        {
            text,
            text_group,
            code,
            expr,
            html,
            extends,
            include,
            block,
            mixin_decl,
            mixin_call,
            external,
        };

        Pos pos;

        virtual ~INode() = default;
        virtual Kind kind() const = 0;
        virtual acul::unique_ptr<INode> clone() const = 0;
    };

    using NodeUP = acul::unique_ptr<INode>;
    using NodeList = acul::vector<NodeUP>;

    struct ParentNode : INode
    {
        NodeList children;
    };

    struct HTMLNode : ParentNode
    {
        acul::string head;
        Kind kind() const override { return Kind::html; }

        acul::unique_ptr<INode> clone() const override
        {
            auto p = acul::make_unique<HTMLNode>();
            p->head = head;
            p->pos = pos;
            for (auto &ch : children) p->children.push_back(ch->clone());
            return p;
        }
    };

    struct CodeNode : ParentNode
    {
        acul::string code;

        Kind kind() const override { return Kind::code; }

        acul::unique_ptr<INode> clone() const override
        {
            auto p = acul::make_unique<CodeNode>();
            p->code = code;
            p->pos = pos;
            for (auto &ch : children) p->children.push_back(ch->clone());
            return p;
        }
    };

    struct BlockNode : ParentNode
    {
        enum Mode
        {
            replace,
            append,
            prepend
        } mode = replace;
        acul::string name;

        Kind kind() const override { return Kind::block; }

        acul::unique_ptr<INode> clone() const override
        {
            auto p = acul::make_unique<BlockNode>();
            p->mode = mode;
            p->name = name;
            p->pos = pos;
            for (auto &ch : children) p->children.push_back(ch->clone());
            return p;
        }
    };

    struct IncludeNode : INode
    {
        acul::string path;
        enum class Mode
        {
            at,
            plain
        } mode;

        virtual Kind kind() const override { return Kind::include; }

        virtual acul::unique_ptr<INode> clone() const override { return acul::make_unique<IncludeNode>(*this); }
    };

    struct MixinDecl : ParentNode
    {
        acul::string name;
        acul::vector<acul::string> args;
        bool has_block = false;

        Kind kind() const override { return Kind::mixin_decl; }

        acul::unique_ptr<INode> clone() const override
        {
            auto p = acul::make_unique<MixinDecl>();
            p->name = name;
            p->pos = pos;
            for (auto &ch : children) p->children.push_back(ch->clone());
            return p;
        }
    };

    struct MixinCall : MixinDecl
    {
        virtual Kind kind() const override { return Kind::mixin_call; }
    };

    struct TextNode : INode
    {
        acul::string text;
        Kind kind() const override { return Kind::text; }
        acul::unique_ptr<INode> clone() const override { return acul::make_unique<TextNode>(*this); }
    };

    struct TextGroupNode : INode
    {
        acul::vector<acul::unique_ptr<TextNode>> text_nodes;
        Kind kind() const override { return Kind::text_group; }

        acul::unique_ptr<INode> clone() const override
        {
            auto p = acul::make_unique<TextGroupNode>();
            p->pos = pos;
            for (const auto &ch : text_nodes) p->text_nodes.push_back(acul::make_unique<TextNode>(*ch));
            return p;
        }
    };

    struct ExprNode : INode
    {
        acul::string expr;
        Kind kind() const override { return Kind::expr; }
        acul::unique_ptr<INode> clone() const override { return acul::make_unique<ExprNode>(*this); }
    };

    struct ExtendsNode : INode
    {
        acul::string path;
        Kind kind() const override { return Kind::extends; }
        acul::unique_ptr<INode> clone() const override { return acul::make_unique<ExtendsNode>(*this); }
    };

    struct ExternalNode : ParentNode
    {
        bool is_struct;

        virtual Kind kind() const override { return Kind::external; }

        virtual acul::unique_ptr<INode> clone() const override
        {
            auto p = acul::make_unique<ExternalNode>();
            p->pos = pos;
            for (auto &ch : children) p->children.push_back(ch->clone());
            return p;
        }
    };

    struct ReplaceSlot
    {
        INode *node;
        INode *parent = nullptr;
        size_t offset = 0;
    };

    // -------------------- Parser --------------------
    struct Parser
    {
        ExtendsNode *extends = nullptr;
        acul::hashmap<acul::string, ReplaceSlot> replace_map;
        NodeList ast;
        acul::vector<Tok> ts;

        void parse();

    private:
        size_t _pos = 0;
        bool at(Tok::Kind k) const { return ts[_pos].kind == k; }
        const Tok &cur() const { return ts[_pos]; }
        const Tok &next() { return ts[_pos++]; }

        acul::unique_ptr<TextGroupNode> collect_text_nodes();
        NodeUP parse_line(INode *parent, size_t parent_next_index, bool is_anonymous_allowed = false);

        void parse_children(ParentNode *node, bool is_anonymous_allowed);
    };

    acul::vector<Tok> lex_with_indents(const acul::string_pool<char> &pool);

    // -------------------- Dumper --------------------
    inline void dump_node(const INode *n, int depth);

    inline void indent(int n)
    {
        for (int i = 0; i < n; i++) std::printf("  ");
    }

    inline void dump_children(const acul::unique_ptr<INode> *xs, size_t size, int depth)
    {
        for (size_t i = 0; i < size; i++) dump_node(xs[i].get(), depth);
    }

    inline void dump_node(const INode *n, int depth)
    {
        printf("%p ", n);
        using K = INode::Kind;
        switch (n->kind())
        {
            case K::text:
            {
                auto *x = static_cast<const TextNode *>(n);
                indent(depth);
                std::printf("Text: %s\n", x->text.c_str());
                break;
            }
            case K::text_group:
            {
                auto *x = static_cast<const TextGroupNode *>(n);
                indent(depth);
                std::printf("TextGroup:\n");
                dump_children((acul::unique_ptr<INode> *)x->text_nodes.data(), x->text_nodes.size(), depth + 1);
                break;
            }
            case K::expr:
            {
                auto *x = static_cast<const ExprNode *>(n);
                indent(depth);
                std::printf("Expr: %s\n", x->expr.c_str());
                break;
            }
            case K::code:
            {
                auto *x = static_cast<const CodeNode *>(n);
                indent(depth);
                std::printf("Code: %s\n", x->code.c_str());
                dump_children(x->children.data(), x->children.size(), depth + 1);
                break;
            }
            case K::html:
            {
                auto *e = static_cast<const HTMLNode *>(n);
                indent(depth);
                std::printf("Element: %s\n", e->head.c_str());
                dump_children(e->children.data(), e->children.size(), depth + 1);
                break;
            }
            case K::block:
            {
                auto *b = static_cast<const BlockNode *>(n);
                indent(depth);
                std::printf("Block(%s): %s\n",
                            b->mode == BlockNode::replace  ? "replace"
                            : b->mode == BlockNode::append ? "append"
                                                           : "prepend",
                            b->name.c_str());
                dump_children(b->children.data(), b->children.size(), depth + 1);
                break;
            }
            case K::include:
            {
                auto *x = static_cast<const IncludeNode *>(n);
                indent(depth);
                std::printf("Include (%s): %s\n", x->mode == IncludeNode::Mode::at ? "template" : "plain-text",
                            x->path.c_str());
                break;
            }
            case K::extends:
            {
                auto *x = static_cast<const ExtendsNode *>(n);
                indent(depth);
                std::printf("Extends: %s\n", x->path.c_str());
                break;
            }
            case K::mixin_decl:
            {
                auto *m = static_cast<const MixinDecl *>(n);
                indent(depth);
                printf("MixinDecl: %s(", m->name.c_str());
                for (int i = 0; i < m->args.size(); i++)
                {
                    printf("%s", m->args[i].c_str());
                    if (i + 1 < m->args.size()) printf(", ");
                }
                printf(")\n");
                dump_children(m->children.data(), m->children.size(), depth + 1);
                break;
            }
            case K::mixin_call:
            {
                auto *m = static_cast<const MixinCall *>(n);
                indent(depth);
                std::printf("MixinCall: +%s\n", m->name.c_str());
                dump_children(m->children.data(), m->children.size(), depth + 1);
                break;
            }
            case K::external:
            {
                auto *x = static_cast<const ExternalNode *>(n);
                indent(depth);
                if (x->is_struct)
                    printf("External(struct)\n");
                else
                    std::printf("External\n");
                dump_children(x->children.data(), x->children.size(), depth + 1);
                break;
            }
        }
    }

    inline void dump_ast(const NodeList &ast)
    {
        std::printf("=== AST ===\n");
        dump_children(ast.data(), ast.size(), 0);
    }
} // namespace ahtt