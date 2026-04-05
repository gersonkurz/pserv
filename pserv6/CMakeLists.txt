cmake_minimum_required(VERSION 3.20)
project(pserv6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PSERV6_PACKAGE_CACHE_DIR "C:/Projects/environ/build/packages" CACHE PATH
    "Optional existing NuGet package cache to reuse before downloading")

function(resolve_nuget_package package version out_dir)
    set(cached_dir "${PSERV6_PACKAGE_CACHE_DIR}/${package}.${version}")
    if(EXISTS "${cached_dir}")
        message(STATUS "Using cached package ${package} ${version} from ${cached_dir}")
        set(${out_dir} "${cached_dir}" PARENT_SCOPE)
        return()
    endif()

    set(download_root "${CMAKE_CURRENT_BINARY_DIR}/packages")
    execute_process(
        COMMAND nuget install ${package} -Version ${version} -OutputDirectory "${download_root}"
        RESULT_VARIABLE result
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed to install ${package} ${version}: ${error}")
    endif()

    set(${out_dir} "${download_root}/${package}.${version}" PARENT_SCOPE)
endfunction()

resolve_nuget_package("Microsoft.WindowsAppSDK" "1.6.250228001" WindowsAppSDK_DIR)
resolve_nuget_package("Microsoft.Web.WebView2" "1.0.2651.64" WebView2_DIR)
resolve_nuget_package("Microsoft.Windows.CppWinRT" "2.0.240405.15" CppWinRT_DIR)

set(GENERATED_FILES_DIR "${CMAKE_CURRENT_BINARY_DIR}/Generated Files")
execute_process(
    COMMAND "${CppWinRT_DIR}/bin/cppwinrt.exe"
        -optimize
        -input "${WindowsAppSDK_DIR}/lib/uap10.0"
        -input "${WindowsAppSDK_DIR}/lib/uap10.0.18362"
        -input "${WebView2_DIR}/lib"
        -input sdk
        -output "${GENERATED_FILES_DIR}"
    RESULT_VARIABLE result
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "cppwinrt.exe failed: ${error}")
endif()

set(MSIX_FILE "${WindowsAppSDK_DIR}/tools/MSIX/win10-x64/Microsoft.WindowsAppRuntime.1.6.msix")
set(MSIX_CONTENT "${CMAKE_CURRENT_BINARY_DIR}/MsixContent")
set(WINRT_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/WindowsAppSDK.manifest")

if(NOT EXISTS "${MSIX_CONTENT}/AppxManifest.xml")
    message(STATUS "Extracting Windows App SDK MSIX...")
    execute_process(
        COMMAND powershell -NoProfile -Command
            "Add-Type -Assembly System.IO.Compression.FileSystem; [IO.Compression.ZipFile]::ExtractToDirectory('${MSIX_FILE}', '${MSIX_CONTENT}')"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed to extract Windows App SDK MSIX")
    endif()
endif()

execute_process(
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate-selfcontained-manifest.ps1"
        -AppxManifest "${MSIX_CONTENT}/AppxManifest.xml"
        -MsixContentDir "${MSIX_CONTENT}"
        -OutputManifest "${WINRT_MANIFEST}"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to generate WinRT activation manifest")
endif()

add_library(pserv6_interaction INTERFACE)
target_include_directories(pserv6_interaction INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(pserv6_interaction INTERFACE cxx_std_20)

set(TARGET_NAME pserv6)

add_executable(${TARGET_NAME} WIN32
    src/ui/pch.cpp
    src/ui/main.cpp
    src/ui/App.cpp
    src/ui/App.h
    src/ui/ExplorerPage.cpp
    src/ui/ExplorerPage.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    interaction/services_adapter.cpp
    interaction/services_adapter.h
    "${WindowsAppSDK_DIR}/include/UndockedRegFreeWinRT-AutoInitializer.cpp"
)

target_precompile_headers(${TARGET_NAME} PRIVATE src/ui/pch.h)

target_compile_definitions(${TARGET_NAME} PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    MICROSOFT_WINDOWSAPPSDK_SELFCONTAINED=1
)

target_compile_options(${TARGET_NAME} PRIVATE
    /W4
    /utf-8
    /external:anglebrackets
    /external:W0
)

target_include_directories(${TARGET_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${WindowsAppSDK_DIR}/include"
    "${GENERATED_FILES_DIR}"
)

target_link_directories(${TARGET_NAME} PRIVATE
    "${WindowsAppSDK_DIR}/lib/win10-x64"
)

target_link_libraries(${TARGET_NAME} PRIVATE
    pserv6_interaction
    Microsoft.WindowsAppRuntime
    WindowsApp
    ole32
    oleaut32
    advapi32
)

target_link_options(${TARGET_NAME} PRIVATE /STACK:8388608)
target_sources(${TARGET_NAME} PRIVATE "${WINRT_MANIFEST}")

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC="${MSIX_CONTENT}"
        -DDST="$<TARGET_FILE_DIR:${TARGET_NAME}>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/copy-runtime.cmake"
    COMMENT "Copying Windows App SDK runtime files"
)
