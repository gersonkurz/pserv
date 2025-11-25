/// @file common_actions.h
/// @brief Export and copy actions shared across all data controllers.
///
/// Provides JSON and text export functionality that can be added to
/// any controller's action list.
#pragma once

namespace pserv
{
    class DataAction;

    /// @brief Append common export actions to an action list.
    /// @param actions Vector to append actions to.
    ///
    /// Adds the following actions:
    /// - Export to JSON file
    /// - Copy as JSON to clipboard
    /// - Export to text file
    /// - Copy as text to clipboard
    void AddCommonExportActions(std::vector<const DataAction *> &actions);

} // namespace pserv
