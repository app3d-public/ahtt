#include "html_ir.hpp"
#include <acul/lut_table.hpp>
#include "parser.hpp"

namespace ahtt
{
    static inline HtmlValue hv_lit(acul::string_view sv)
    {
        HtmlValue v;
        if (!sv.empty()) v.segs.push_back({HtmlSegment::Literal, sv});
        return v;
    }

    static inline HtmlValue hv_expr(acul::string_view sv)
    {
        HtmlValue v;
        if (!sv.empty()) v.segs.push_back({HtmlSegment::Expr, sv});
        return v;
    }

    struct StopCharTraits
    {
        using value_type = char;
        using enum_type = char;

        static constexpr enum_type unknown = '\0';

        static consteval void fill_lut_table(std::array<enum_type, 128> &a)
        {
            a['.'] = '.';
            a['#'] = '#';
            a['{'] = '{';
            a['}'] = '}';
            a['('] = '(';
            a[')'] = ')';
            a[':'] = ':';
            a['='] = '=';
        }
    };

    static constexpr acul::lut_table<128, StopCharTraits> stop_chars;

    inline void push_lit(acul::vector<HtmlSegment> &v, acul::string_view s)
    {
        if (!s.empty()) v.push_back({HtmlSegment::Literal, s});
    }

    inline void push_expr(acul::vector<HtmlSegment> &v, acul::string_view s)
    {
        if (!s.empty()) v.push_back({HtmlSegment::Expr, s});
    }

    static HtmlValue parse_segments_interp(acul::string_view val)
    {
        HtmlValue out;
        const char *p = val.data();
        const char *e = p + val.size();
        const char *lit_begin = p;

        while (p < e)
        {
            if (p + 1 < e && p[0] == '#' && p[1] == '{')
            {
                if (p > lit_begin) push_lit(out.segs, {lit_begin, size_t(p - lit_begin)});
                p += 2;
                const char *expr_begin = p;
                int depth = 1;
                bool in_s = false, in_d = false;
                while (p < e && depth > 0)
                {
                    char c = *p++;
                    if (!in_s && !in_d)
                    {
                        if (c == '{')
                            ++depth;
                        else if (c == '}')
                            --depth;
                        else if (c == '\'')
                            in_s = true;
                        else if (c == '"')
                            in_d = true;
                    }
                    else if (in_s)
                    {
                        if (c == '\\' && p < e)
                            ++p;
                        else if (c == '\'')
                            in_s = false;
                    }
                    else
                    {
                        if (c == '\\' && p < e)
                            ++p;
                        else if (c == '"')
                            in_d = false;
                    }
                }
                const char *expr_end = (depth == 0) ? (p - 1) : p;
                push_expr(out.segs, {expr_begin, size_t(expr_end - expr_begin)});
                lit_begin = p;
            }
            else { ++p; }
        }
        if (p > lit_begin) push_lit(out.segs, {lit_begin, size_t(p - lit_begin)});
        return out;
    }

    static HtmlSegment read_gettext_expr_segment(const char *&pos, const char *end)
    {
        const char *beg = pos;
        if (!(pos + 1 < end && pos[0] == '_' && pos[1] == '(')) return {HtmlSegment::Literal, {beg, 0}};
        pos += 2;
        int paren = 1;
        bool in_s = false, in_d = false, in_interp = false;
        int brace = 0;
        while (pos < end && paren > 0)
        {
            char c = *pos++;
            if (in_interp)
            {
                if (!in_s && !in_d)
                {
                    if (c == '{')
                        ++brace;
                    else if (c == '}')
                    {
                        if (--brace == 0) in_interp = false;
                    }
                    else if (c == '\'')
                        in_s = true;
                    else if (c == '"')
                        in_d = true;
                }
                else if (in_s)
                {
                    if (c == '\\' && pos < end)
                        ++pos;
                    else if (c == '\'')
                        in_s = false;
                }
                else
                {
                    if (c == '\\' && pos < end)
                        ++pos;
                    else if (c == '"')
                        in_d = false;
                }
                continue;
            }
            if (!in_s && !in_d)
            {
                if (c == '(')
                    ++paren;
                else if (c == ')')
                    --paren;
                else if (c == '#' && pos < end && *pos == '{')
                {
                    ++pos;
                    in_interp = true;
                    brace = 1;
                }
                else if (c == '\'')
                    in_s = true;
                else if (c == '"')
                    in_d = true;
            }
            else
            {
                if (c == '\\' && pos < end)
                {
                    ++pos;
                    continue;
                }
                if (in_s && c == '\'')
                    in_s = false;
                else if (in_d && c == '"')
                    in_d = false;
            }
        }
        return {HtmlSegment::Expr, {beg, size_t(pos - beg)}};
    }

    HtmlValue parse_segments_full(acul::string_view val)
    {
        HtmlValue out;
        const char *p = val.data();
        const char *e = p + val.size();
        const char *lit_begin = p;

        auto flush_lit = [&](const char *up_to) {
            if (up_to > lit_begin) push_lit(out.segs, {lit_begin, size_t(up_to - lit_begin)});
            lit_begin = up_to;
        };

        while (p < e)
        {
            // #{...}
            if (p + 1 < e && p[0] == '#' && p[1] == '{')
            {
                flush_lit(p);
                p += 2; // skip #{
                const char *expr_begin = p;
                int depth = 1;
                bool in_s = false, in_d = false;
                while (p < e && depth > 0)
                {
                    char c = *p++;
                    if (!in_s && !in_d)
                    {
                        if (c == '{')
                            ++depth;
                        else if (c == '}')
                            --depth;
                        else if (c == '\'')
                            in_s = true;
                        else if (c == '"')
                            in_d = true;
                    }
                    else if (in_s)
                    {
                        if (c == '\\' && p < e)
                            ++p;
                        else if (c == '\'')
                            in_s = false;
                    }
                    else
                    {
                        if (c == '\\' && p < e)
                            ++p;
                        else if (c == '"')
                            in_d = false;
                    }
                }
                const char *expr_end = (depth == 0) ? (p - 1) : p;
                push_expr(out.segs, {expr_begin, size_t(expr_end - expr_begin)});
                lit_begin = p;
                continue;
            }

            // _( ... )
            if (p + 1 < e && p[0] == '_' && p[1] == '(')
            {
                flush_lit(p);
                const char *tmp = p;
                HtmlSegment seg = read_gettext_expr_segment(tmp, e);
                push_expr(out.segs, seg.sv);
                p = lit_begin = tmp;
                continue;
            }

            ++p;
        }

        if (p > lit_begin) push_lit(out.segs, {lit_begin, size_t(p - lit_begin)});
        return out;
    }

    inline HtmlValue parse_html_value(const char *&pos, const char *begin, const char *end)
    {
        while (pos < end)
        {
            if (stop_chars.find(*pos) != StopCharTraits::unknown || std::isspace(static_cast<unsigned char>(*pos)))
            {
                ptrdiff_t len = pos - begin;
                return hv_lit(acul::string_view(begin, static_cast<size_t>(len)));
            }
            ++pos;
        }

        ptrdiff_t len = pos - begin;
        return hv_lit(acul::string_view(begin, static_cast<size_t>(len)));
    }

    inline bool is_ws(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

    inline void skip_ws(const char *&p, const char *end)
    {
        while (p < end && is_ws(*p)) ++p;
    }

    static acul::string_view read_name_token(const char *&p, const char *end)
    {
        const char *beg = p;
        while (p < end)
        {
            char c = *p;
            if (c == '=' || c == ',' || c == ')' || is_ws(c)) break;
            ++p;
        }
        return {beg, size_t(p - beg)};
    }

    static acul::string_view read_quoted(const char *&p, const char *end)
    {
        char q = *p++;
        const char *beg = p;
        while (p < end)
        {
            char c = *p++;
            if (c == '\\' && p < end)
            {
                ++p;
                continue;
            }
            if (c == q) break;
        }
        return {beg, size_t((p - 1) - beg)};
    }

    static acul::string_view read_gettext_call(const char *&p, const char *end)
    {
        const char *call_beg = p;
        if (!(p + 1 < end && p[0] == '_' && p[1] == '(')) return {};
        p += 2;
        int paren = 1;
        bool in_s = false, in_d = false, in_interp = false;
        int brace = 0;

        while (p < end && paren > 0)
        {
            char c = *p++;
            if (in_interp)
            {
                if (!in_s && !in_d)
                {
                    if (c == '{')
                        ++brace;
                    else if (c == '}')
                    {
                        if (--brace == 0) in_interp = false;
                    }
                    else if (c == '\'')
                        in_s = true;
                    else if (c == '"')
                        in_d = true;
                }
                else if (in_s)
                {
                    if (c == '\\' && p < end) { ++p; }
                    else if (c == '\'')
                        in_s = false;
                }
                else
                {
                    if (c == '\\' && p < end) { ++p; }
                    else if (c == '"')
                        in_d = false;
                }
                continue;
            }

            if (!in_s && !in_d)
            {
                if (c == '(') { ++paren; }
                else if (c == ')') { --paren; }
                else if (c == '#' && p < end && *p == '{')
                {
                    ++p;
                    in_interp = true;
                    brace = 1;
                }
                else if (c == '\'')
                    in_s = true;
                else if (c == '"')
                    in_d = true;
            }
            else
            {
                if (c == '\\' && p < end)
                {
                    ++p;
                    continue;
                }
                if (in_s && c == '\'')
                    in_s = false;
                else if (in_d && c == '"')
                    in_d = false;
            }
        }
        return {call_beg, size_t(p - call_beg)};
    }

    static acul::string_view read_unquoted(const char *&p, const char *end)
    {
        const char *beg = p;
        while (p < end)
        {
            char c = *p;
            if (c == ',' || c == ')' || is_ws(c)) break;
            ++p;
        }
        return {beg, size_t(p - beg)};
    }

    void parse_html_attr(const char *&pos, const char *begin, const char *end, acul::vector<HtmlAttr> &attrs)
    {
        (void)begin;

        while (pos < end)
        {
            skip_ws(pos, end);
            if (pos >= end) break;
            if (*pos == ')') break;

            if (*pos == ',')
            {
                ++pos;
                continue;
            }

            acul::string_view name_sv = read_name_token(pos, end);
            if (name_sv.empty())
            {
                if (pos < end && *pos != ')')
                {
                    ++pos;
                    continue;
                }
                break;
            }

            HtmlAttr attr;
            attr.name = hv_lit(name_sv);

            skip_ws(pos, end);

            if (pos >= end || *pos != '=')
            {
                attr.value = HtmlValue{};
                attrs.push_back(std::move(attr));

                skip_ws(pos, end);
                if (pos < end && *pos == ',') { ++pos; }
                else if (pos < end && *pos == ')')
                {
                    ++pos;
                    break;
                }
                continue;
            }

            ++pos;
            skip_ws(pos, end);

            HtmlValue val;
            if (pos < end && (*pos == '"' || *pos == '\''))
            {
                const char *q_open = pos;
                auto inner = read_quoted(pos, end);
                const char *q_close = pos - 1;

                if (inner.find("#{") == acul::string_view::npos && inner.find("_(") == acul::string_view::npos)
                    val = hv_lit(acul::string_view(q_open, static_cast<size_t>(q_close - q_open + 1)));
                else
                {
                    HtmlValue inner_val = parse_segments_full(inner);
                    if (!inner_val.has_expr())
                        val = hv_lit(acul::string_view(q_open, static_cast<size_t>(q_close - q_open + 1)));
                    else
                    {
                        push_lit(val.segs, acul::string_view(q_open, 1));
                        for (auto &s : inner_val.segs) val.segs.push_back(s);
                        push_lit(val.segs, acul::string_view(q_close, 1));
                    }
                }
            }
            else if (pos + 1 < end && pos[0] == '_' && pos[1] == '(')
            {
                auto call = read_gettext_call(pos, end);
                val = hv_expr(call);
            }
            else
            {
                auto uv = read_unquoted(pos, end);
                val = parse_segments_full(uv);
            }

            attr.value = std::move(val);
            attrs.push_back(std::move(attr));
        }
    }

    static acul::string_view read_head_token(const char *&p, const char *end)
    {
        const char *beg = p;
        while (p < end)
        {
            if ((p + 1 < end) && p[0] == '#' && p[1] == '{')
            {
                p += 2;
                int depth = 1;
                bool in_s = false, in_d = false;
                while (p < end && depth > 0)
                {
                    char d = *p++;
                    if (!in_s && !in_d)
                    {
                        if (d == '{')
                            ++depth;
                        else if (d == '}')
                            --depth;
                        else if (d == '\'')
                            in_s = true;
                        else if (d == '"')
                            in_d = true;
                    }
                    else if (in_s)
                    {
                        if (d == '\\' && p < end)
                            ++p;
                        else if (d == '\'')
                            in_s = false;
                    }
                    else // in_d
                    {
                        if (d == '\\' && p < end)
                            ++p;
                        else if (d == '"')
                            in_d = false;
                    }
                }
                continue;
            }

            char c = *p;
            if (c == '.' || c == '(' || c == ')' || c == ':' || c == '=' || is_ws(c)) break;
            if (c == '#') break;
            ++p;
        }
        return {beg, size_t(p - beg)};
    }

    void parse_to_html_ir(HTMLNode *node, HtmlIR &ir, const char *&pos)
    {
        const char *head_begin = node->head.data();
        const char *head_end = head_begin + node->head.size();

        if (pos < head_begin || pos > head_end) pos = head_begin;

        const char *begin = pos;
        while (pos < head_end)
        {
            if (stop_chars.find(*pos) != StopCharTraits::unknown || std::isspace(static_cast<unsigned char>(*pos)))
            {
                if (ir.tag.empty()) { ir.tag = acul::string_view(begin, static_cast<size_t>(pos - begin)); }

                switch (*pos)
                {
                    case '.':
                    {
                        ++pos;
                        if (ir.tag.empty()) ir.tag = "div";
                        acul::string_view tok = read_head_token(pos, head_end);
                        ir.classes.push_back(parse_segments_interp(tok));
                        continue;
                    }
                    case '#':
                    {
                        ++pos;
                        if (ir.tag.empty()) ir.tag = "div";
                        if (!ir.id.empty())
                            throw acul::runtime_error(
                                acul::format("ID must be unique. At line %d, col %d", node->pos.line, node->pos.col));
                        acul::string_view tok = read_head_token(pos, head_end);
                        ir.id = parse_segments_interp(tok);
                        continue;
                    }
                    case '{':
                    case '}':
                    case ')':
                        throw acul::runtime_error(
                            acul::format("Unexpected brackets at line %d, col %d", node->pos.line, node->pos.col));

                    case '(':
                    {
                        ++pos;
                        parse_html_attr(pos, pos, head_end, ir.attrs);
                        if (pos < head_end && *pos == ')') ++pos;
                        continue;
                    }

                    case ':':
                    {
                        ir.next = acul::make_unique<HtmlIR>();
                        ++pos;
                        while (pos < head_end && std::isspace(static_cast<unsigned char>(*pos))) ++pos;
                        parse_to_html_ir(node, *ir.next, pos);
                        return;
                    }

                    default:
                    {
                        bool is_expr = (*pos == '=');
                        if (is_expr)
                            ++pos;
                        else
                            while (pos < head_end && is_ws(*pos)) ++pos;

                        acul::string_view sv(pos, static_cast<size_t>(head_end - pos));
                        ir.content = is_expr ? hv_expr(sv) : parse_segments_full(sv);
                        return;
                    }
                }
            }

            ++pos;
        }

        if (ir.tag.empty()) ir.tag = acul::string_view(begin, static_cast<size_t>(head_end - begin));
    }

} // namespace ahtt