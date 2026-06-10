#pragma once

namespace wng
{
    enum class Result {
        Ok,
        InvalidArgument,
        NotFound,
        AlreadyExists,
        InvalidConnection,
        AllocationFailure
    };
}
