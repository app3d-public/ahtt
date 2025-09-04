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
                tn->text = acul::string();
                tn->pos = cur().pos;
            }
            group->text_nodes.push_back(std::move(tn));
            next();
        }
        return group;
    }

    void Parser::parse_children(ParentNode *node, bool is_anonymous_allowed)
    {
        next();
        while (at(Tok::line) || at(Tok::blank))
        {
            if (at(Tok::line))
                node->children.push_back(parse_line(node, node->children.size(), is_anonymous_allowed));
            else
                next();
        }
        if (!at(Tok::dedent))
            throw acul::runtime_error(
                acul::format("expected DEDENT after block at line: %d, col: %d", cur().pos.line, cur().pos.col));
        next();
    }

    void parse_mixin(MixinDecl *m, const acul::string_view &name, const Pos &pos)
    {
        auto start = name.find('(');
        if (start == std::string::npos)
            throw acul::runtime_error(
                acul::format("Expected '(' after mixin declaration. Line %d, pos %d", pos.line, pos.col));
        auto end = name.rfind(')');
        if (end == std::string::npos)
            throw acul::runtime_error(
                acul::format("Expected ')' after mixin declaration. Line %d, pos %d", pos.line, pos.col));

        acul::string args_raw{name.data() + start + 1, end - start - 1};

        m->name = acul::trim(acul::string(name.data(), start));
        m->args = acul::split(args_raw, ',');
        m->pos = pos;
    }

    static int paren_balance(acul::string_view s)
    {
        bool in_s = false, in_d = false, esc = false;
        int bal = 0;
        for (char c : s)
        {
            if (esc)
            {
                esc = false;
                continue;
            }
            if (c == '\\')
            {
                esc = true;
                continue;
            }
            if (!in_d && c == '\'')
                in_s = !in_s;
            else if (!in_s && c == '"')
                in_d = !in_d;
            else if (!in_s && !in_d)
            {
                if (c == '(')
                    ++bal;
                else if (c == ')')
                    --bal;
            }
        }
        return bal;
    }

    NodeUP Parser::parse_html_node(const acul::string &s, const Tok &t, bool is_anonymous_allowed)
    {
        auto el = acul::make_unique<HTMLNode>();
        acul::string trimmed = acul::trim_end(s);
        bool is_dot_at_end = !trimmed.empty() && trimmed.back() == '.';
        el->head = s.substr(0, is_dot_at_end ? s.size() - 1 : s.size());
        el->pos = t.pos;
        next();

        size_t sp = el->head.find(' ');
        acul::string_view html_head{el->head.c_str(), sp != acul::string::npos ? sp : el->head.size()};
        bool is_ob_found = html_head.find('(') != acul::string::npos;
        if (is_ob_found)
        {
            int bal = paren_balance(el->head);
            if (bal > 0)
            {
                int borrowed_indents = 0;

                while (bal > 0)
                {
                    if (at(Tok::indent))
                    {
                        ++borrowed_indents;
                        next();
                        continue;
                    }

                    if (at(Tok::blank))
                    {
                        next();
                        continue;
                    }

                    if (!at(Tok::line)) break;

                    const Tok &lt2 = cur();
                    el->head += ' ';
                    el->head += acul::trim_start(lt2.sv);
                    bal += paren_balance(acul::string_view(lt2.sv));
                    next();
                }

                while (borrowed_indents > 0 && at(Tok::dedent))
                {
                    --borrowed_indents;
                    next();
                }
                if (bal > 0)
                    throw acul::runtime_error(acul::format(
                        "Expected ')' to close tag attributes started at line %d, col %d", t.pos.line, t.pos.col));
            }
        }

        if (at(Tok::indent))
        {
            if (is_dot_at_end)
            {
                next();
                auto group = collect_text_nodes();
                el->children.push_back(std::move(group));
                if (!at(Tok::dedent))
                    throw acul::runtime_error(acul::format("expected DEDENT after text block at line: %d, col: %d",
                                                           cur().pos.line, cur().pos.col));
                next();
            }
            else
                parse_children(el.get(), is_anonymous_allowed);
        }
        return el;
    }

    NodeUP Parser::parse_line(INode *parent, size_t parent_next_index, bool is_anonymous_allowed)
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

        if (acul::starts_with(s, "block"))
        {
            BlockNode::Mode mode = BlockNode::replace;
            bool is_anonymous = s.size() == 5;
            acul::string rest;
            if (!is_anonymous)
            {
                rest = trim(s.substr(6));
                is_anonymous = rest.empty();
            }

            auto b = acul::make_unique<BlockNode>();
            if (!is_anonymous)
            {
                if (acul::starts_with(rest, "append "))
                {
                    mode = BlockNode::append;
                    b->name = trim(rest.substr(7));
                }
                else if (acul::starts_with(rest, "prepend "))
                {
                    mode = BlockNode::prepend;
                    b->name = trim(rest.substr(8));
                }
                else
                    b->name = rest;
                replace_map.emplace(b->name, b.get(), parent, parent_next_index);
            }
            else if (!is_anonymous_allowed)
                throw acul::runtime_error(
                    acul::format("anonymous block is not allowed at: line %d, pos %d", t.pos.line, t.pos.col));
            b->mode = mode;
            b->pos = t.pos;

            next();
            if (at(Tok::indent)) parse_children(b.get(), is_anonymous_allowed);
            return b;
        }

        bool is_append_starts = acul::starts_with(s, "append ");
        if (is_append_starts || acul::starts_with(s, "prepend "))
        {
            auto block = acul::make_unique<BlockNode>();
            block->mode = is_append_starts ? BlockNode::append : BlockNode::prepend;
            block->name = trim(s.substr(is_append_starts ? 7 : 8));
            block->pos = t.pos;
            replace_map.emplace(block->name, block.get(), parent, parent_next_index);

            next();
            if (at(Tok::indent)) parse_children(block.get(), is_anonymous_allowed);
            return block;
        }

        if (acul::starts_with(s, "mixin "))
        {
            auto m = acul::make_unique<MixinDecl>();
            acul::string_view name{s.data() + 6, s.size() - 6};
            parse_mixin(m.get(), name, t.pos);

            next();
            if (at(Tok::indent)) parse_children(m.get(), true);
            return m;
        }

        if (!s.empty() && s[0] == '+')
        {
            auto m = acul::make_unique<MixinCall>();
            acul::string_view name{s.data() + 1, s.size() - 1};
            parse_mixin(m.get(), name, t.pos);

            next();
            if (at(Tok::indent)) parse_children(m.get(), is_anonymous_allowed);
            return m;
        }

        // code / expr / text
        if (acul::starts_with(s, "- "))
        {
            auto c = acul::make_unique<CodeNode>();
            c->code = acul::string(s.substr(2));
            c->pos = t.pos;

            next();
            if (at(Tok::indent)) parse_children(c.get(), is_anonymous_allowed);
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
            auto ext_type_def = acul::trim(s.substr(8));
            ext->is_struct = ext_type_def == "struct";

            next();
            if (at(Tok::indent)) parse_children(ext.get(), is_anonymous_allowed);
            return ext;
        }

        return parse_html_node(s, t, is_anonymous_allowed);
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