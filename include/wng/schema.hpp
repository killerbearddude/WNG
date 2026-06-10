#pragma once

#include <string>
#include <vector>

#include <wng/port.hpp>
#include <wng/result.hpp>

namespace wng
{
    struct PortDefinition {
        std::string name;
        PortKind kind = PortKind::Input;
        std::string type;
        bool required = false;
        bool visible = true;
        bool enabled = true;
    };

    struct NodeDefinition {
        std::string type;
        std::string display_name;
        std::vector<PortDefinition> inputs;
        std::vector<PortDefinition> outputs;
        bool visible = true;
        bool enabled = true;
    };

    class GraphSchema {
    public:
        Result add_node_definition(const NodeDefinition& definition);

        const NodeDefinition* find_node_definition(const std::string& type) const;

        const std::vector<NodeDefinition>& node_definitions() const;

        bool allows_node_type(const std::string& type) const;

    private:
        std::vector<NodeDefinition> node_definitions_;
    };
}
