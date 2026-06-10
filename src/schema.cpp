#include <new>

#include <wng/schema.hpp>

namespace
{
    bool is_valid_port_kind(wng::PortKind kind)
    {
        return kind == wng::PortKind::Input || kind == wng::PortKind::Output;
    }

    bool contains_duplicate_name(const std::vector<wng::PortDefinition>& ports)
    {
        for (std::size_t i = 0; i < ports.size(); ++i) {
            for (std::size_t j = i + 1; j < ports.size(); ++j) {
                if (ports[i].name == ports[j].name) {
                    return true;
                }
            }
        }
        return false;
    }
}

namespace wng
{
    Result GraphSchema::add_node_definition(const NodeDefinition& definition)
    {
        if (definition.type.empty()) {
            return Result::InvalidArgument;
        }

        if (find_node_definition(definition.type) != nullptr) {
            return Result::AlreadyExists;
        }

        for (const PortDefinition& input : definition.inputs) {
            if (!is_valid_port_kind(input.kind) || input.kind != PortKind::Input) {
                return Result::InvalidArgument;
            }
        }

        for (const PortDefinition& output : definition.outputs) {
            if (!is_valid_port_kind(output.kind) || output.kind != PortKind::Output) {
                return Result::InvalidArgument;
            }
        }

        if (contains_duplicate_name(definition.inputs)) {
            return Result::AlreadyExists;
        }

        if (contains_duplicate_name(definition.outputs)) {
            return Result::AlreadyExists;
        }

        try {
            node_definitions_.push_back(definition);
            return Result::Ok;
        } catch (const std::bad_alloc&) {
            return Result::AllocationFailure;
        }
    }

    const NodeDefinition* GraphSchema::find_node_definition(const std::string& type) const
    {
        for (const NodeDefinition& definition : node_definitions_) {
            if (definition.type == type) {
                return &definition;
            }
        }

        return nullptr;
    }

    const std::vector<NodeDefinition>& GraphSchema::node_definitions() const
    {
        return node_definitions_;
    }

    bool GraphSchema::allows_node_type(const std::string& type) const
    {
        return find_node_definition(type) != nullptr;
    }
}
