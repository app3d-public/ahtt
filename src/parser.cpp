#include "parser.hpp"
#include <acul/io/path.hpp>

namespace ahtt
{
    acul::vector<Tok> lex_with_indents(const acul::string_pool<char> &pool)
    {
        acul::vector<Tok> out;
        out.reserve(pool.size());
        acul::vector<int> stack{0};
        int line_no = 1;

        for (const char *ln : pool)
        {
            int sp = 0;
            const char *p = ln;
            while (*p == ' ')
            {
                ++sp;
                ++p;
            }

            const char *q = p;
            while (*q) ++q;
            size_t content_len = static_cast<size_t>(q - p);
            bool blank = (content_len == 0);

            if (blank)
                out.push_back({Tok::blank, {}, {line_no, 1}, (int)stack.size() - 1});
            else
            {
                while (sp < stack.back())
                {
                    stack.pop_back();
                    out.push_back({Tok::dedent, {}, {line_no, 1}, (int)stack.size() - 1});
                }

                if (sp > stack.back())
                {
                    stack.push_back(sp);
                    out.push_back({Tok::indent, {}, {line_no, 1}, (int)stack.size() - 1});
                }

                acul::string_view content(p, content_len);
                out.push_back({Tok::line, content, {line_no, sp + 1}, (int)stack.size() - 1});
            }

            ++line_no;
        }

        while (stack.size() > 1)
        {
            stack.pop_back();
            out.push_back({Tok::dedent, {}, {line_no, 1}, (int)stack.size() - 1});
        }

        out.push_back({Tok::eof, {}, {line_no, 1}, 0});
        return out;
    }

    acul::unique_ptr<TextGroupNode> Parser::collect_text_nodes()
    {
        auto group = acul::make_unique<TextGroupNode>();
        if (at(Tok::line) || at(Tok::blank)) group->pos = cur().pos;

        while (at(Tok::line) || at(Tok::blank))
        {
            auto tn = acul::make_unique<TextNode>();
            if (at(Tok::line))
            {
                const Tok &lt = cur();
                tn->text = acul::string(lt.sv);
                tn->pos = lt.pos;
            }
            else
            {
                // пустая строка
                tn->text = acul::string();
                tn->pos = cur().pos;
            }
            group->text_nodes.push_back(std::move(tn));
            next();
        }
        return group;
    }

    NodeUP Parser::parse_line(INode *parent, size_t parent_next_index)
    {
        const Tok &t = cur();
        acul::string s = acul::trim_start(t.sv);

        // directives
        if (acul::starts_with(s, "extends "))
        {
            auto p = acul::make_unique<ExtendsNode>();
            extends = p.get();
            p->path = trim(s.substr(8));
            p->pos = t.pos;
            next();
            return p;
        }

        if (acul::starts_with(s, "block "))
        {
            BlockNode::Mode mode = BlockNode::replace;
            acul::string rest = trim(s.substr(6));
            acul::string name;
            if (acul::starts_with(rest, "append "))
            {
                mode = BlockNode::append;
                name = trim(rest.substr(7));
            }
            else if (acul::starts_with(rest, "prepend "))
            {
                mode = BlockNode::prepend;
                name = trim(rest.substr(8));
            }
            else
                name = rest;

            next();
            auto b = acul::make_unique<BlockNode>();
            b->mode = mode;
            b->name = std::move(name);
            b->pos = t.pos;
            replace_map.emplace(b->name, b.get(), parent, parent_next_index);

            if (at(Tok::indent))
            {
                next();
                while (at(Tok::line) || at(Tok::blank))
                {
                    if (at(Tok::line))
                        b->children.push_back(parse_line(b.get(), b->children.size()));
                    else
                        next();
                }
                if (!at(Tok::dedent)) throw acul::runtime_error("expected DEDENT after block body");
                next();
            }
            return b;
        }

        if (acul::starts_with(s, "append ") || acul::starts_with(s, "prepend "))
        {
            bool is_append = acul::starts_with(s, "append ");
            auto block = acul::make_unique<BlockNode>();
            block->mode = is_append ? BlockNode::append : BlockNode::prepend;
            block->name = trim(s.substr(is_append ? 7 : 8));
            block->pos = t.pos;
            replace_map.emplace(block->name, block.get(), parent, parent_next_index);
            next();

            if (at(Tok::indent))
            {
                next();
                while (at(Tok::line) || at(Tok::blank))
                {
                    if (at(Tok::line))
                        block->children.push_back(parse_line(block.get(), block->children.size()));
                    else
                        next();
                }
                if (!at(Tok::dedent)) throw acul::runtime_error("expected DEDENT after append/prepend");
                next();
            }
            return block;
        }

        if (acul::starts_with(s, "mixin "))
        {
            auto m = acul::make_unique<MixinDecl>();
            size_t p = 6;
            while (p < s.size() && std::isspace((unsigned char)s[p])) ++p;
            size_t q = p;
            if (q >= s.size() || !(std::isalpha((unsigned char)s[q]) || s[q] == '_'))
                throw acul::runtime_error("mixin name expected");
            ++q;
            while (q < s.size() && (std::isalnum((unsigned char)s[q]) || s[q] == '_' || s[q] == '-')) ++q;
            m->name = acul::string(s.substr(p, q - p));
            m->pos = t.pos;
            next();

            if (at(Tok::indent))
            {
                next();
                while (at(Tok::line) || at(Tok::blank))
                {
                    if (at(Tok::line))
                        m->body.push_back(parse_line(m.get(), m->body.size()));
                    else
                        next();
                }
                if (!at(Tok::dedent)) throw acul::runtime_error("expected DEDENT after mixin body");
                next();
            }
            return m;
        }

        if (!s.empty() && s[0] == '+')
        {
            auto m = acul::make_unique<MixinCall>();
            size_t p = 1, q = p;
            if (q >= s.size() || !(std::isalpha((unsigned char)s[q]) || s[q] == '_'))
                throw acul::runtime_error("mixin call name expected");
            ++q;
            while (q < s.size() && (std::isalnum((unsigned char)s[q]) || s[q] == '_' || s[q] == '-')) ++q;
            m->name = acul::string(s.substr(p, q - p));
            m->pos = t.pos;
            next();

            if (at(Tok::indent))
            {
                next();
                while (at(Tok::line) || at(Tok::blank))
                {
                    if (at(Tok::line))
                        m->body.push_back(parse_line(m.get(), m->body.size()));
                    else
                        next(); // ПРОПУСТИТЬ blank
                }
                if (!at(Tok::dedent)) throw acul::runtime_error("expected DEDENT after mixin call body");
                next();
            }
            return m;
        }

        // code / expr / text
        if (acul::starts_with(s, "- "))
        {
            auto c = acul::make_unique<CodeNode>();
            c->code = acul::string(s.substr(2));
            c->pos = t.pos;
            next();

            if (at(Tok::indent))
            {
                next();
                while (at(Tok::line) || at(Tok::blank))
                {
                    if (at(Tok::line))
                        c->children.push_back(parse_line(c.get(), c->children.size()));
                    else
                        next();
                }
                if (!at(Tok::dedent)) throw acul::runtime_error("expected DEDENT after code block");
                next();
            }
            return c;
        }

        if (acul::starts_with(s, "= "))
        {
            auto e = acul::make_unique<ExprNode>();
            e->expr = acul::string(s.substr(2));
            e->pos = t.pos;
            next();
            return e;
        }

        if (acul::starts_with(s, "|"))
        {
            auto tnode = acul::make_unique<TextNode>();
            tnode->text = trim(s.substr(1));
            tnode->pos = t.pos;
            next();
            return tnode;
        }

        if (s == ".")
        {
            next();
            if (!at(Tok::indent)) throw acul::runtime_error("expected INDENT after .");

            next();
            auto group = collect_text_nodes();
            if (!at(Tok::dedent)) throw acul::runtime_error("expected DEDENT after text block");
            next();

            group->pos = t.pos;
            return group;
        }

        if (acul::starts_with(s, "include "))
        {
            auto include_node = acul::make_unique<IncludeNode>();
            include_node->path = s.substr(8);
            include_node->pos = t.pos;
            auto ext = acul::io::get_extension(include_node->path);
            include_node->mode = ext == ".at" ? IncludeNode::Mode::at : IncludeNode::Mode::plain;
            replace_map.emplace(include_node->path, include_node.get(), parent, parent_next_index);
            next();
            return include_node;
        }

        if (acul::starts_with(s, "external"))
        {
            auto ext = acul::make_unique<ExternalNode>();
            ext->pos = t.pos;
            next();

            if (at(Tok::indent))
            {
                next();
                while (at(Tok::line) || at(Tok::blank))
                {
                    if (at(Tok::line))
                        ext->children.push_back(parse_line(ext.get(), ext->children.size()));
                    else
                        next();
                }
                if (!at(Tok::dedent)) throw acul::runtime_error("expected DEDENT after external");
                next();
            }
            return ext;
        }

        auto el = acul::make_unique<HTMLNode>();
        acul::string trimmed = acul::trim_end(s);
        bool is_dot_at_end = !trimmed.empty() && trimmed.back() == '.';
        el->head = s.substr(0, is_dot_at_end ? s.size() - 1 : s.size());
        el->pos = t.pos;
        next();

        if (at(Tok::indent))
        {
            next();
            if (is_dot_at_end)
            {
                auto group = collect_text_nodes();
                el->children.push_back(std::move(group));
                if (!at(Tok::dedent)) throw acul::runtime_error("expected DEDENT after text block");
                next();
            }

            else
            {
                while (at(Tok::line) || at(Tok::blank))
                {
                    if (at(Tok::line))
                        el->children.push_back(parse_line(el.get(), el->children.size()));
                    else
                        next();
                }
                if (!at(Tok::dedent)) throw acul::runtime_error("expected DEDENT after element body");
                next();
            }
        }
        return el;
    }

    void Parser::parse()
    {
        while (!at(Tok::eof))
        {
            if (at(Tok::blank))
            {
                next();
                continue;
            }

            if (at(Tok::indent) || at(Tok::dedent))
            {
                const Tok &t = cur();
                throw acul::runtime_error(acul::format("Unexpected %s at line %d, col %d",
                                                       at(Tok::indent) ? "indent" : "dedent", t.pos.line, t.pos.col));
            }

            if (at(Tok::line))
            {
                const Tok &t = cur();
                if (t.level != 0)
                    throw acul::runtime_error(
                        acul::format("Leading indentation before first content is not allowed at line %d, col %d",
                                     t.pos.line, t.pos.col));
                ast.push_back(parse_line(nullptr, ast.size()));
                continue;
            }
            break;
        }
    }

} // namespace ahtt