#pragma once

#include <memory>

#include <interaction/action_execution.h>
#include <interaction/explorer_contracts.h>

namespace pserv6::interaction
{
    class ServicesViewAdapter final : public IExplorerViewAdapter
    {
    public:
        ServicesViewAdapter();
        ~ServicesViewAdapter() override;

        const ExplorerViewDescriptor &Describe() const override;
        ViewRefreshResult Refresh(const ExplorerViewState &state, bool isAutoRefresh) override;
        std::vector<ActionDescriptor> GetActions(const ExplorerViewState &state) const override;
        std::optional<DetailDocument> GetDetails(std::string_view itemId) const override;
        bool ApplyDetailEdits(std::string_view itemId, const DetailDocument &document, std::string &errorMessage) override;
        bool SetRemoteContext(const RemoteContext &context, std::string &errorMessage) override;
        RemoteContext GetRemoteContext() const override;

    private:
        friend class ServicesActionExecutor;
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

    class ServicesActionExecutor final : public IActionExecutor
    {
    public:
        explicit ServicesActionExecutor(ServicesViewAdapter &adapter);
        ~ServicesActionExecutor() override;

        ActionResult Execute(const ActionRequest &request, IActionExecutionHost &host) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace pserv6::interaction
