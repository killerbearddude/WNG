// Exercises validation diagnostics for resource exhaustion paths.
// The graph validator must report resource exhaustion explicitly instead of
// reusing unrelated structural issue codes.

#include <cassert>
#include <new>

#include <wng/graph.hpp>
#include <wng/graph_validation.hpp>

namespace
{
    wng::Result resource_exhausted_result()
    {
        return static_cast<wng::Result>(5);
    }

    class ThrowingValidationCallback final : public wng::GraphValidationCallback {
    public:
        wng::Result validate_graph(
            const wng::Graph&,
            wng::ValidationReport&) const override
        {
            throw std::bad_alloc();
        }
    };
}

int main()
{
    {
        // Public graph validation catches host resource exhaustion and returns a
        // deterministic diagnostic that is not tied to a fake node or link error.
        wng::Graph graph;
        ThrowingValidationCallback callback;
        wng::GraphValidationOptions options;
        options.callback = &callback;

        const wng::ValidationReport report = wng::validate_graph(graph, options);

        assert(!report.valid());
        assert(report.issues.size() == 1U);
        assert(report.issues[0].code == wng::ValidationIssueCode::ResourceExhausted);
        assert(report.issues[0].result == resource_exhausted_result());
        assert(report.issues[0].node == wng::NodeId {});
        assert(report.issues[0].port == wng::PortId {});
        assert(report.issues[0].link == wng::LinkId {});
    }

    return 0;
}
