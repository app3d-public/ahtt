#pragma once

#include <acul/hash/hashmap.hpp>
#include <acul/io/path.hpp>
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

    struct FileInfo
    {
        acul::io::path path;
        size_t file_size;
    };

    using IOInfo = acul::vector<FileInfo>;
} // namespace ahtt