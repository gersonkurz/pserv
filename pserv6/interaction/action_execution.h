#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pserv6::interaction
{
    struct ActionRequest
    {
        std::string ViewId;
        std::string ActionId;
        std::vector<std::string> ItemIds;
    };

    struct ActionProgress
    {
        float Progress{0.0f};
        std::string Message;
    };

    struct ActionResult
    {
        bool Success{false};
        bool Cancelled{false};
        bool RequiresRefresh{false};
        std::optional<std::string> ErrorMessage;
    };

    struct ConfirmationRequest
    {
        std::string Title;
        std::string Message;
        bool Destructive{false};
    };

    class IActionExecutionHost
    {
    public:
        virtual ~IActionExecutionHost() = default;

        virtual bool Confirm(const ConfirmationRequest &request) = 0;
        virtual void ReportProgress(const ActionProgress &progress) = 0;
        virtual bool IsCancellationRequested() const = 0;
        virtual void ReportError(std::string_view message) = 0;
    };

    class IActionExecutor
    {
    public:
        virtual ~IActionExecutor() = default;
        virtual ActionResult Execute(const ActionRequest &request, IActionExecutionHost &host) = 0;
    };
} // namespace pserv6::interaction

