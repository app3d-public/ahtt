#pragma once

#include <acul/memory/smart_ptr.hpp>
#include <acul/string/string_view.hpp>
#include <acul/vector.hpp>

namespace ahtt
{
    struct HtmlSegment
    {
        enum Kind
        {
            Literal,
            Expr
        } kind;
        acul::string_view sv;
    };

    struct HtmlValue
    {
        acul::vector<HtmlSegment> segs;
        bool empty() const { return segs.empty(); }
        bool has_expr() const
        {
            for (auto &s : segs)
                if (s.kind == HtmlSegment::Expr) return true;
            return false;
        }
    };

    struct HtmlAttr
    {
        HtmlValue name;
        HtmlValue value;
    };

    struct HtmlIR
    {
        acul::string_view tag;
        HtmlValue id;
        acul::vector<HtmlValue> classes;
        acul::vector<HtmlAttr> attrs;
        HtmlValue content;
        acul::unique_ptr<HtmlIR> next;
    };

    void parse_to_html_ir(struct HTMLNode *node, HtmlIR &ir, const char *&pos);

    HtmlValue parse_segments_full(acul::string_view val);
} // namespace ahtt