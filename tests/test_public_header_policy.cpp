#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::string read_file(const std::string& path)
    {
        std::ifstream file(path);
        assert(file.is_open());

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    bool contains(const std::string& text, const std::string& needle)
    {
        return text.find(needle) != std::string::npos;
    }

    void require_absent(const std::string& text, const std::string& needle)
    {
        assert(!contains(text, needle));
    }
}

int main()
{
    const std::vector<std::string> headers {
        "include/wng/dirty_propagation.hpp",
        "include/wng/execution_plan.hpp",
        "include/wng/ids.hpp",
        "include/wng/math.hpp",
        "include/wng/result.hpp",
        "include/wng/node.hpp",
        "include/wng/port.hpp",
        "include/wng/link.hpp",
        "include/wng/mutation_summary.hpp",
        "include/wng/serialization_dto.hpp",
        "include/wng/serialization.hpp",
        "include/wng/schema.hpp",
        "include/wng/schema_snapshot.hpp",
        "include/wng/schema_diff.hpp",
        "include/wng/schema_compatibility.hpp",
        "include/wng/schema_migration_plan.hpp",
        "include/wng/schema_migration_policy.hpp",
        "include/wng/schema_migration_apply_preview.hpp",
        "include/wng/schema_migration_command_preview.hpp",
        "include/wng/schema_migration_apply.hpp",
        "include/wng/schema_migration_apply_command.hpp",
        "include/wng/schema_migration_apply_command_history.hpp",
        "include/wng/schema_mutation.hpp",
        "include/wng/schema_validation.hpp",
        "include/wng/validation.hpp",
        "include/wng/graph.hpp",
        "include/wng/graph_command.hpp",
        "include/wng/graph_command_history.hpp",
        "include/wng/graph_history.hpp",
        "include/wng/graph_command_transaction.hpp",
        "include/wng/graph_diff.hpp",
        "include/wng/graph_mutation_preview.hpp",
        "include/wng/graph_restore.hpp",
        "include/wng/graph_snapshot.hpp",
        "include/wng/graph_redo.hpp",
        "include/wng/graph_undo.hpp",
        "include/wng/graph_validation.hpp",
        "include/wng/graph_traversal.hpp",
        "include/wng/wng.hpp"
    };

    for (const std::string& header : headers) {
        const std::string text = read_file(header);

        require_absent(text, "<wpl/");
        require_absent(text, "\"wpl/");
        require_absent(text, "<WPL/");
        require_absent(text, "\"WPL/");
        require_absent(text, "<X11/");
        require_absent(text, "<x11/");
        require_absent(text, "Xlib");
        require_absent(text, "<linux/");
        require_absent(text, "<sys/");
        require_absent(text, "<windows.h>");
        require_absent(text, "<Windows.h>");
        require_absent(text, "<functional>");
    }

    return 0;
}
