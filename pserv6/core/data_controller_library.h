/// @file data_controller_library.h
/// @brief Registry of all available data controllers.
///
/// DataControllerLibrary manages the lifecycle of all DataController instances,
/// providing centralized access to the application's data views.
#pragma once

namespace pserv
{
    class DataController;

    /// @brief Central registry for all data controllers.
    ///
    /// This class owns and manages all DataController instances in the application.
    /// Controllers are created lazily on first access and destroyed when the
    /// library is cleared or destroyed.
    ///
    /// @par Usage:
    /// @code
    /// DataControllerLibrary library;
    /// for (auto* controller : library.GetDataControllers()) {
    ///     if (controller->GetControllerName() == "Services") {
    ///         controller->Refresh();
    ///     }
    /// }
    /// @endcode
    class DataControllerLibrary final
    {
    public:
        DataControllerLibrary() = default;
        ~DataControllerLibrary();

        /// @brief Get all registered data controllers.
        /// Creates controllers on first call (lazy initialization).
        /// @return Reference to vector of controller pointers.
        const std::vector<DataController *> &GetDataControllers();

        /// @brief Destroy all controllers and release resources.
        void Clear();
    };
} // namespace pserv
