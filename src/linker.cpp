#include "linker.hpp"

namespace ahtt
{
    void resolve_includes(Parser &p, const acul::io::path &base_path, IOInfo &io);

    void append_plain_text(const ReplaceSlot &slot, const acul::io::path &path, Parser &p, Pos pos, size_t offset,
                           IOInfo &io)
    {
        LOG_INFO("Loading file: %s", path.str().c_str());
        acul::vector<char> file_buffer;
        if (acul::io::file::read_binary(path.str(), file_buffer) != acul::io::file::op_state::success)
            throw acul::runtime_error(acul::format("Failed to read file: %s", path.str().c_str()));
        io.emplace_back(path, file_buffer.size());

        auto text_node = acul::make_unique<TextNode>();
        text_node->text = acul::string(file_buffer.data(), file_buffer.size());
        text_node->pos = pos;
        if (!slot.parent)
            p.ast[slot.offset + offset] = std::move(text_node);
        else
        {
            auto *parent = dynamic_cast<ParentNode *>(slot.parent);
            if (!parent) throw acul::runtime_error("Invalid parent node");
            parent->children[slot.offset + offset] = std::move(text_node);
        }
    }

    inline void append_template(const ReplaceSlot &slot, Parser &p, const acul::io::path &base_path,
                                const acul::io::path &path, ptrdiff_t &delta, IOInfo &io)
    {
        Parser inc;
        load_template(path, inc, io);
        resolve_includes(inc, base_path, io);

        const size_t N = inc.ast.size();

        auto &vec = (!slot.parent) ? p.ast : dynamic_cast<ParentNode *>(slot.parent)->children;

        const ptrdiff_t pos_signed = static_cast<ptrdiff_t>(slot.offset) + delta;
        if (pos_signed < 0 || static_cast<size_t>(pos_signed) >= vec.size())
            throw acul::runtime_error("include replacement position out of range");
        const size_t pos = static_cast<size_t>(pos_signed);

        if (N == 0)
        {
            vec.erase(vec.begin() + pos);
            delta -= 1;
            return;
        }

        vec.insert(vec.begin() + pos, std::make_move_iterator(inc.ast.begin()), std::make_move_iterator(inc.ast.end()));
        vec.erase(vec.begin() + (pos + N));
        delta += static_cast<ptrdiff_t>(N) - 1;
    }

    void resolve_includes(Parser &p, const acul::io::path &base_path, IOInfo &io)
    {
        acul::vector<acul::pair<acul::string, ReplaceSlot>> to_replace{p.replace_map.begin(), p.replace_map.end()};
        std::sort(to_replace.begin(), to_replace.end(), [](const auto &a, const auto &b) {
            const auto &slot_a = a.second;
            const auto &slot_b = b.second;
            return slot_a.parent < slot_b.parent || (slot_a.parent == slot_b.parent && slot_a.offset < slot_b.offset);
        });

        ptrdiff_t added_offset = 0;
        INode *prev_parent = nullptr;
        acul::vector<acul::string> erased;
        for (const auto &[name, slot] : to_replace)
        {
            if (prev_parent != slot.parent) added_offset = 0;
            if (slot.node->kind() == INode::Kind::include)
            {
                erased.push_back(name);
                auto *node = static_cast<IncludeNode *>(slot.node);
                auto path = base_path / node->path;
                if (node->mode == IncludeNode::Mode::plain)
                    append_plain_text(slot, path, p, node->pos, added_offset, io);
                else
                    append_template(slot, p, base_path, path, added_offset, io);
            }
            else
                p.replace_map[name].offset += added_offset;
            prev_parent = slot.parent;
        }
        for (const auto &name : erased) p.replace_map.erase(name);
    }

    void resolve_blocks(Parser &layout, Parser &child_parser)
    {
        acul::vector<acul::pair<acul::string, ReplaceSlot>> to_replace;
        to_replace.reserve(layout.replace_map.size());
        for (const auto &kv : layout.replace_map)
            if (kv.second.node->kind() == INode::Kind::block) to_replace.push_back(kv);

        std::sort(to_replace.begin(), to_replace.end(), [](const auto &a, const auto &b) {
            const auto &sa = a.second, &sb = b.second;
            return sa.parent < sb.parent || (sa.parent == sb.parent && sa.offset < sb.offset);
        });

        ptrdiff_t added_offset = 0;
        INode *prev_parent = nullptr;

        for (const auto &[name, slot] : to_replace)
        {
            if (prev_parent != slot.parent) added_offset = 0;

            auto &vec = (!slot.parent) ? layout.ast : static_cast<ParentNode *>(slot.parent)->children;

            const ptrdiff_t pos_signed = static_cast<ptrdiff_t>(slot.offset) + added_offset;
            if (pos_signed < 0 || static_cast<size_t>(pos_signed) >= vec.size())
                throw acul::runtime_error("block replacement position out of range");
            const size_t pos = static_cast<size_t>(pos_signed);

            assert(vec[pos].get() == slot.node);
            auto *orig_block = static_cast<BlockNode *>(slot.node);

            auto it = child_parser.replace_map.find(name);
            NodeList final_children;

            if (it == child_parser.replace_map.end()) { final_children = std::move(orig_block->children); }
            else
            {
                const auto &child_slot = it->second;
                assert(child_slot.node->kind() == INode::Kind::block);
                auto *child_block = static_cast<BlockNode *>(child_slot.node);

                switch (child_block->mode)
                {
                    case BlockNode::Mode::replace:
                    {
                        final_children = std::move(child_block->children);
                        break;
                    }
                    case BlockNode::Mode::prepend:
                    {
                        final_children.reserve(child_block->children.size() + orig_block->children.size());
                        for (auto &n : child_block->children) final_children.push_back(std::move(n));
                        for (auto &n : orig_block->children) final_children.push_back(std::move(n));
                        break;
                    }
                    case BlockNode::Mode::append:
                    {
                        final_children.reserve(child_block->children.size() + orig_block->children.size());
                        for (auto &n : orig_block->children) final_children.push_back(std::move(n));
                        for (auto &n : child_block->children) final_children.push_back(std::move(n));
                        break;
                    }
                    default:
                        throw acul::runtime_error("unknown BlockNode mode");
                }
            }

            const size_t N = final_children.size();

            if (N == 0)
            {
                vec.erase(vec.begin() + pos);
                added_offset -= 1;
            }
            else
            {
                vec.insert(vec.begin() + pos, std::make_move_iterator(final_children.begin()),
                           std::make_move_iterator(final_children.end()));
                vec.erase(vec.begin() + (pos + N));
                added_offset += static_cast<ptrdiff_t>(N) - 1;
            }

            prev_parent = slot.parent;
        }
    }

    void Linker::link(const acul::io::path &base_path, IOInfo &io)
    {
        resolve_includes(_template, base_path, io);
        if (!_template.extends) return;
        auto extend_path = base_path / _template.extends->path;
        Parser extend_parser;
        load_template(extend_path, extend_parser, io);
        resolve_includes(extend_parser, base_path, io);
        resolve_blocks(extend_parser, _template);
        _template.ast = std::move(extend_parser.ast);
        _template.replace_map.clear();
    }

} // namespace ahtt