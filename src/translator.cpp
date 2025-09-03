#include "translator.hpp"
#include <acul/log.hpp>
#include "html_ir.hpp"

#define INDENT4  "    "
#define INDENT8  INDENT4 INDENT4
#define INDENT12 INDENT8 INDENT4
#define INDENT16 INDENT8 INDENT8

namespace ahtt
{
    static inline void push_expr(NodeList &ast, const Pos &pos, acul::string_view sv)
    {
        if (sv.empty()) return;
        auto en = acul::make_unique<ExprNode>();
        en->pos = pos;
        en->expr.assign(sv.data(), sv.size());
        ast.push_back(std::move(en));
    }

    void push_text_to_ast(NodeList &ast, HtmlIR &ir, HTMLNode *node)
    {
        for (auto &seg : ir.content.segs)
        {
            if (seg.kind == HtmlSegment::Literal)
            {
                if (!seg.sv.empty())
                {
                    auto tn = acul::make_unique<TextNode>();
                    tn->pos = node->pos;
                    tn->text = acul::string(seg.sv);
                    ast.push_back(std::move(tn));
                }
            }
            else if (seg.kind == HtmlSegment::Expr)
                if (!seg.sv.empty()) push_expr(ast, node->pos, seg.sv);
        }
    }

    void push_doctype_to_ast(NodeList &ast, HtmlIR &ir, HTMLNode *node)
    {
        static acul::hashmap<acul::string_view, const char *> builtin{
            {"html", "<!DOCTYPE html>"},
            {"xml", "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"},
            {"transitional", "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
                             "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">"},
            {"strict", "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
                       "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">"},
            {"frameset", "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Frameset//EN\" "
                         "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd\">"},
            {"1.1",
             "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">"},
            {"basic", "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML Basic 1.1//EN\" "
                      "\"http://www.w3.org/TR/xhtml-basic/xhtml-basic11.dtd\">"},
            {"mobile", "<!DOCTYPE html PUBLIC \"-//WAPFORUM//DTD XHTML Mobile 1.2//EN\" "
                       "\"http://www.openmobilealliance.org/tech/DTD/xhtml-mobile12.dtd\">"},
            {"plist", "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" "
                      "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"}};
        auto trimmed = acul::trim(ir.content.segs.front().sv);
        auto it = builtin.find(trimmed.data());
        if (it != builtin.end())
        {
            auto tn = acul::make_unique<TextNode>();
            tn->pos = node->pos;
            tn->text = it->second;
            ast.push_back(std::move(tn));
        }
        else
        {
            ir.tag = "!DOCTYPE";
            push_text_to_ast(ast, ir, node);
        }
    }

    static inline void flush_text(NodeList &ast, acul::stringstream &ss, const Pos &pos, bool &has_buf)
    {
        if (!has_buf) return;
        auto tn = acul::make_unique<TextNode>();
        tn->pos = pos;
        tn->text = ss.str();
        ast.push_back(std::move(tn));
        ss.clear();
        has_buf = false;
    }

    static inline void put_sv(acul::stringstream &ss, acul::string_view sv, bool &has_buf)
    {
        if (!sv.empty())
        {
            ss.write(sv.data(), (std::streamsize)sv.size());
            has_buf = true;
        }
    }

    static void emit_value(NodeList &ast, acul::stringstream &ss, HTMLNode *node, bool &has_buf, const HtmlValue &v)
    {
        for (const auto &seg : v.segs)
        {
            if (seg.kind == HtmlSegment::Literal) { put_sv(ss, seg.sv, has_buf); }
            else
            {
                flush_text(ast, ss, node->pos, has_buf);
                if (!seg.sv.empty()) push_expr(ast, node->pos, seg.sv);
            }
        }
    }

    static void emit_open_tag(NodeList &ast, HtmlIR &ir, HTMLNode *node)
    {
        acul::stringstream ss;
        bool has_buf = false;

        ss << '<';
        put_sv(ss, ir.tag, has_buf);

        if (!ir.id.empty())
        {
            ss << " id=\"";
            has_buf = true;
            emit_value(ast, ss, node, has_buf, ir.id);
            ss << '"';
            has_buf = true;
        }

        if (!ir.classes.empty())
        {
            ss << " class=\"";
            has_buf = true;
            for (size_t i = 0; i < ir.classes.size(); ++i)
            {
                emit_value(ast, ss, node, has_buf, ir.classes[i]);
                if (i + 1 < ir.classes.size())
                {
                    ss << ' ';
                    has_buf = true;
                }
            }
            ss << '"';
            has_buf = true;
        }

        for (const auto &attr : ir.attrs)
        {
            ss << ' ';
            has_buf = true;
            emit_value(ast, ss, node, has_buf, attr.name);
            if (!attr.value.empty())
            {
                ss << '=';
                has_buf = true;
                emit_value(ast, ss, node, has_buf, attr.value);
            }
        }

        ss << '>';
        has_buf = true;
        flush_text(ast, ss, node->pos, has_buf);
    }

    static void emit_ir_chain(NodeList &ast, HtmlIR &ir, HTMLNode *node)
    {
        emit_open_tag(ast, ir, node);
        if (ir.next)
            emit_ir_chain(ast, *ir.next, node);
        else if (!ir.content.empty())
            push_text_to_ast(ast, ir, node);
    }

    static inline bool is_void_tag(acul::string_view t)
    {
        if (t.empty()) return false;
        switch (t[0])
        {
            case 'a':
                return t == "area";
            case 'b':
                return t == "base" || t == "br";
            case 'c':
                return t == "col" || t == "command";
            case 'e':
                return t == "embed";
            case 'h':
                return t == "hr";
            case 'i':
                return t == "img" || t == "input";
            case 'k':
                return t == "keygen";
            case 'l':
                return t == "link";
            case 'm':
                return t == "meta";
            case 'p':
                return t == "param" || t == "portal";
            case 's':
                return t == "source";
            case 't':
                return t == "track";
            case 'w':
                return t == "wbr";
            default:
                return false;
        }
    }

    static void collect_chain_tags(const HtmlIR &ir, acul::vector<acul::string_view> &out)
    {
        const HtmlIR *p = &ir;
        while (p)
        {
            out.push_back(p->tag);
            p = p->next.get();
        }
    }

    static inline void emit_close_tag(NodeList &ast, HTMLNode *node, acul::string_view tag)
    {
        auto tn = acul::make_unique<TextNode>();
        tn->pos = node->pos;
        tn->text = acul::format("</%.*s>", (int)tag.size(), tag.data());
        ast.push_back(std::move(tn));
    }

    static void push_plain_text(NodeList &ast, const Pos &pos, acul::string_view raw)
    {
        HtmlValue v = parse_segments_full(raw);

        acul::stringstream ss;
        bool has_buf = false;

        for (const auto &seg : v.segs)
        {
            if (seg.kind == HtmlSegment::Literal)
                put_sv(ss, seg.sv, has_buf);
            else
            {
                flush_text(ast, ss, pos, has_buf);
                push_expr(ast, pos, seg.sv);
            }
        }
        flush_text(ast, ss, pos, has_buf);
    }

    int Translator::build_html(NodeList &ast, HTMLNode *node)
    {
        HtmlIR ir;
        const char *pos = node->head.c_str();
        parse_to_html_ir(node, ir, pos);

        if (!_doctype && ir.tag == "doctype" && ir.content.segs.size() == 1)
        {
            _doctype = node;
            push_doctype_to_ast(ast, ir, node);
            return AHTT_PARSE_DEFAULT;
        }

        emit_ir_chain(ast, ir, node);
        int flags = parse_tokens(node->children, ast);

        acul::vector<acul::string_view> opened;
        opened.reserve(4);
        collect_chain_tags(ir, opened);
        for (size_t i = opened.size(); i-- > 0;)
            if (!is_void_tag(opened[i])) emit_close_tag(ast, node, opened[i]);
        return flags;
    }

    static inline bool is_ident_start(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }

    void Translator::build_external_node(ExternalNode *current)
    {
        _external = acul::make_unique<ExternalNode>();
        _external->pos = current->pos;
        _external->is_struct = current->is_struct;

        for (auto &child : current->children)
        {
            if (!child || child->kind() != INode::Kind::code) continue;

            auto *cn = static_cast<CodeNode *>(child.get());
            acul::string trimmed = acul::trim_start(cn->code);
            if (trimmed.empty())
            {
                child.reset();
                continue;
            }

            if (acul::starts_with(trimmed, "#include"))
            {
                _includes_map.emplace(acul::string(trimmed));
                child.reset();
                continue;
            }

            if (_external->is_struct)
            {
                _external->children.emplace_back(std::move(child));
                continue;
            }

            if (is_ident_start(trimmed.front()))
            {
                size_t end = acul::find_last_of(trimmed.data(), trimmed.size(), ';');
                if (end == trimmed.npos) end = trimmed.size();
                auto ncn = acul::make_unique<CodeNode>();
                ncn->pos = cn->pos;
                ncn->code = trimmed.substr(0, end);
                _external->children.emplace_back(std::move(ncn));
            }
            child.reset();
        }
    }

    int Translator::parse_node(INode *node, NodeList &ast)
    {
        switch (node->kind())
        {
            case INode::Kind::external:
                build_external_node(static_cast<ExternalNode *>(node));
                break;
            case INode::Kind::html:
                return build_html(ast, static_cast<HTMLNode *>(node));
            case INode::Kind::text:
            {
                auto tn = static_cast<TextNode *>(node);
                push_plain_text(ast, tn->pos, tn->text);
                break;
            }
            case INode::Kind::text_group:
            {
                auto *tgn = static_cast<TextGroupNode *>(node);
                acul::string buf;
                for (size_t i = 0; i < tgn->text_nodes.size(); ++i)
                {
                    const auto *tn = tgn->text_nodes[i].get();
                    buf += tn->text;
                    if (i + 1 < tgn->text_nodes.size()) buf.push_back('\n');
                }
                push_plain_text(ast, tgn->pos, buf);
                break;
            }

            case INode::Kind::code:
            {
                auto cn = static_cast<CodeNode *>(node);
                auto trimmed = acul::trim_start(cn->code);
                if (acul::starts_with(trimmed, "#include"))
                {
                    _includes_map.emplace(std::move(trimmed));
                    break;
                }
                else
                {
                    auto &sv = cn->code;
                    auto ncn = acul::make_unique<CodeNode>();
                    ncn->pos = cn->pos;
                    ncn->code.assign(sv.data(), sv.size());
                    parse_tokens(cn->children, ncn->children);
                    ast.push_back(std::move(ncn));
                }
                break;
            }
            case INode::Kind::expr:
                ast.push_back(node->clone());
                break;
            case INode::Kind::mixin_decl:
            {
                auto m = acul::make_unique<MixinDecl>();
                parse_mixin(static_cast<MixinDecl *>(node), m.get());
                _mixins_map.emplace(m->name, std::move(m));
                break;
            }
            case INode::Kind::mixin_call:
            {
                auto m = acul::make_unique<MixinCall>();
                parse_mixin(static_cast<MixinDecl *>(node), m.get());
                ast.push_back(std::move(m));
                break;
            }
            case INode::Kind::block:
            {
                auto *bn = static_cast<BlockNode *>(node);
                auto en = acul::make_unique<CodeNode>();
                en->pos = bn->pos;
                en->code = "std::forward<Block>(block)(ss);";
                ast.push_back(std::move(en));
                return AHTT_PARSE_BLOCK_ADDED;
            }
            default:
                break;
        }
        return AHTT_PARSE_DEFAULT;
    }

    static acul::stringstream &write_mixin_signature(acul::stringstream &ss, MixinDecl *decl, bool has_block)
    {
        if (has_block) ss << INDENT12 "template <class Block>\n";
        ss << INDENT12 "inline void " << decl->name << "(acul::stringstream& ss";
        if (has_block) ss << ", Block&& block";
        for (const auto &arg : decl->args) ss << ", " << arg;
        ss << ')';
        return ss;
    }

    acul::stringstream &Translator::write_node_list(acul::stringstream &ss, const NodeList &nodes, const char *ss_out,
                                                    const char *indent)
    {
        auto escape_cpp_string = [](acul::string_view s) -> acul::string {
            acul::string out;
            out.reserve(s.size() + 8);
            for (char c : s)
            {
                switch (c)
                {
                    case '\\':
                        out.push_back('\\');
                        out.push_back('\\');
                        break;
                    case '\"':
                        out.push_back('\\');
                        out.push_back('\"');
                        break;
                    case '\n':
                        out.push_back('\\');
                        out.push_back('n');
                        break;
                    case '\r':
                        out.push_back('\\');
                        out.push_back('r');
                        break;
                    case '\t':
                        out.push_back('\\');
                        out.push_back('t');
                        break;
                    default:
                        out.push_back(c);
                        break;
                }
            }
            return out;
        };

        bool chain_open = false;
        bool first_in_chain = false;
        acul::string pending_text;

        auto start_chain = [&] {
            if (!chain_open)
            {
                ss << indent << ss_out << " << ";
                chain_open = true;
                first_in_chain = true;
            }
        };
        auto end_chain = [&] {
            if (chain_open)
            {
                ss << ";\n";
                chain_open = false;
                first_in_chain = false;
            }
        };
        auto append_text_seg = [&](acul::string_view lit) {
            start_chain();
            if (!first_in_chain) ss << " << ";
            ss << '\"' << escape_cpp_string(lit) << '\"';
            first_in_chain = false;
        };
        auto append_expr_seg = [&](acul::string_view expr) {
            start_chain();
            if (!first_in_chain) ss << " << " << expr;
            first_in_chain = false;
        };
        auto flush_pending_text = [&] {
            if (!pending_text.empty())
            {
                append_text_seg(pending_text);
                pending_text.clear();
            }
        };

        for (const auto &n : nodes)
        {
            switch (n->kind())
            {
                case INode::Kind::text:
                {
                    pending_text += static_cast<const TextNode &>(*n).text;
                    break;
                }
                case INode::Kind::expr:
                {
                    flush_pending_text();
                    append_expr_seg(static_cast<const ExprNode &>(*n).expr);
                    break;
                }
                case INode::Kind::code:
                {
                    flush_pending_text();
                    end_chain();
                    auto *cn = static_cast<const CodeNode *>(n.get());
                    ss << indent << cn->code << '\n';
                    if (!cn->children.empty())
                    {
                        ss << indent << "{\n";
                        acul::string new_indent = acul::string(indent) + INDENT4;
                        write_node_list(ss, cn->children, ss_out, new_indent.c_str());
                        ss << indent << "}\n";
                    }
                    break;
                }
                case INode::Kind::mixin_call:
                {
                    auto *mcn = static_cast<const MixinCall *>(n.get());
                    auto it = _mixins_map.find(mcn->name);
                    if (it == _mixins_map.end())
                    {
                        LOG_WARN("mixin [%s] was not declared", mcn->name.c_str());
                        break;
                    }
                    flush_pending_text();
                    end_chain();
                    ss << indent << "mixins::" << mcn->name << "(ss";

                    if (it->second->has_block)
                    {
                        if (mcn->children.empty())
                            ss << ", [](acul::stringstream&) {}";
                        else
                        {
                            ss << ", [&](acul::stringstream& __blk_ss) {\n";
                            acul::string new_indent = acul::string(indent) + INDENT4;
                            write_node_list(ss, mcn->children, "__blk_ss", new_indent.c_str());
                            ss << indent << "}";
                        }
                    }

                    for (const auto &arg : mcn->args) ss << ", " << arg;
                    ss << ");\n";
                    break;
                }
                default:
                    break;
            }
        }

        flush_pending_text();
        end_chain();
        return ss;
    }

    void Translator::write_to_stream(acul::stringstream &ss, const acul::string &template_name)
    {
        ss << "// Generated by ahtt\n"
              "#pragma once\n\n"
              "#include <acul/string/string.hpp>\n"
              "#include <acul/string/sstream.hpp>\n"
              "#include <acul/locales/locales.hpp>\n";
        for (auto &include : _includes_map) ss << include << "\n";
        ss << "\n";
        ss << "namespace ahtt\n{\n" INDENT4 "namespace " << template_name << "\n    {\n";

        // External struct decl
        if (_external && _external->is_struct)
        {
            ss << INDENT8 "struct External\n" INDENT8 "{\n";
            write_node_list(ss, _external->children, "ss", INDENT12);
            ss << INDENT8 "};\n\n";
        }

        // Mixins decl
        if (!_mixins_map.empty())
        {
            ss << INDENT8 "namespace mixins\n" INDENT8 "{\n";
            for (const auto &mixin : _mixins_map)
                write_mixin_signature(ss, mixin.second.get(), mixin.second->has_block) << ";\n";
            ss << '\n';
            for (const auto &mixin : _mixins_map)
            {
                auto *m = mixin.second.get();
                write_mixin_signature(ss, m, m->has_block) << "\n" INDENT12 "{\n";
                write_node_list(ss, m->children, "ss", INDENT16);
                ss << INDENT12 "}\n";
            }
            ss << INDENT8 "}\n\n";
        }

        // render
        ss << INDENT8 "inline acul::string render(";
        if (_external)
        {
            if (_external->is_struct)
                ss << "const External& external";
            else
            {
                bool first = true;
                for (const auto &node : _external->children)
                {
                    if (node->kind() != INode::Kind::code) continue;

                    if (!first) ss << ", ";
                    first = false;
                    auto *cn = static_cast<const CodeNode *>(node.get());
                    ss << cn->code;
                }
            }
        }
        ss << ")\n" INDENT8 "{\n" INDENT12 "acul::stringstream ss;\n";
        write_node_list(ss, _ast, "ss", INDENT12);

        ss << INDENT12 "return ss.str();\n";
        ss << INDENT8 "}\n" INDENT4 "}\n}";
    }
} // namespace ahtt