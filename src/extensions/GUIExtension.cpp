#include "GUIExtension.h"
#include "../core/PluginBase.h"
#include "../util/DebugHelpers.h"
#include <cstring> // for strcmp
#include <cmath>   // for std::abs, std::round

namespace applause
{
    // Constructor
    GUIExtensionBase::GUIExtensionBase(const clap_host_t* host, int defaultWidth, int defaultHeight, bool fixedAspectRatio)
        : host_(host), width_(defaultWidth), height_(defaultHeight), fixedAspectRatio_(fixedAspectRatio)
    {
        // Calculate aspect ratio from default dimensions
        aspectRatio_ = static_cast<float>(defaultWidth) / static_cast<float>(defaultHeight);
        
        // Get host GUI extension for callbacks
        if (host_)
        {
            hostGui_ = static_cast<const clap_host_gui_t*>(
                host_->get_extension(host_, CLAP_EXT_GUI));
#ifdef __linux__
            hostFdSupport_ = static_cast<const clap_host_posix_fd_support_t*>(
                host_->get_extension(host_, CLAP_EXT_POSIX_FD_SUPPORT));
#endif
        }
    }

    // Static callback implementations
    bool GUIExtensionBase::clap_gui_is_api_supported(const clap_plugin_t* plugin,
                                                     const char* api,
                                                     bool is_floating) noexcept
    {
        if (is_floating)
            return false;

#ifdef _WIN32
        if (strcmp(api, CLAP_WINDOW_API_WIN32) == 0)
            return true;
#elif __APPLE__
        if (strcmp(api, CLAP_WINDOW_API_COCOA) == 0)
            return true;
#elif __linux__
        if (strcmp(api, CLAP_WINDOW_API_X11) == 0)
            return true;  // Visage doesn't support Wayland
#endif

        return false;
    }

    bool GUIExtensionBase::clap_gui_get_preferred_api(const clap_plugin_t* plugin,
                                                      const char** api,
                                                      bool* is_floating) noexcept
    {
        return false; // we don't support multiple graphics backends yet; this can be safely ignored
    }

    bool GUIExtensionBase::clap_gui_create(const clap_plugin_t* plugin,
                                           const char* api,
                                           bool is_floating) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext) return false;

        if (is_floating)
            return false;

        if (ext->editor_)
        {
            LOG_WARN("GUI already exists, returning true");
            return true;
        }

        ext->createEditor();
        
        // Set fixed aspect ratio if enabled
        if (ext->editor_ && ext->fixedAspectRatio_)
        {
            ext->editor_->setFixedAspectRatio(true);
        }

        LOG_INFO("GUI created successfully");
        return true;
    }

    void GUIExtensionBase::clap_gui_destroy(const clap_plugin_t* plugin) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext) return;

#ifdef __linux__
        // Unregister Linux FD support before destroying
        if (ext->editor_ && ext->editor_->window() && ext->hostFdSupport_)
        {
            ext->hostFdSupport_->unregister(ext->host_, ext->editor_->window()->posixFd());
        }
#endif

        ext->destroyEditor();
        LOG_INFO("GUI destroyed");
    }

    bool GUIExtensionBase::clap_gui_set_scale(const clap_plugin_t* plugin,
                                              double scale) noexcept
    {
        // this extension can be safely ignored for now
        return false;
    }

    bool GUIExtensionBase::clap_gui_get_size(const clap_plugin_t* plugin,
                                             uint32_t* width,
                                             uint32_t* height) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext) return false;

        if (ext->editor_)
        {
            *width = static_cast<uint32_t>(ext->editor_->width());
            *height = static_cast<uint32_t>(ext->editor_->height());
        }
        else
        {
            // Return default size if editor not created yet
            *width = ext->width_;
            *height = ext->height_;
        }

        return true;
    }

    bool GUIExtensionBase::clap_gui_can_resize(const clap_plugin_t* plugin) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext) return false;

        return true;
    }

    bool GUIExtensionBase::clap_gui_get_resize_hints(const clap_plugin_t* plugin,
                                                     clap_gui_resize_hints_t* hints) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext) return false;

        // Provide reasonable defaults
        hints->can_resize_horizontally = true;
        hints->can_resize_vertically = true;
        hints->preserve_aspect_ratio = ext->fixedAspectRatio_;
        
        if (ext->fixedAspectRatio_)
        {
            // Use integer ratio to avoid rounding errors
            // For 4:3 aspect ratio (800x600)
            hints->aspect_ratio_width = 4;
            hints->aspect_ratio_height = 3;
        }
        else
        {
            hints->aspect_ratio_width = 0;
            hints->aspect_ratio_height = 0;
        }

        return true;
    }

    bool GUIExtensionBase::clap_gui_adjust_size(const clap_plugin_t* plugin,
                                                uint32_t* width,
                                                uint32_t* height) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext) return false;

        if (!ext->fixedAspectRatio_)
        {
            // No adjustment needed if not fixed aspect ratio
            return true;
        }

        // Enforce aspect ratio - same logic as ClapPlugin example
        float current_ratio = static_cast<float>(*width) / static_cast<float>(*height);
        
        if (std::abs(current_ratio - ext->aspectRatio_) > 0.001f)
        {
            // Prefer adjusting height to maintain width
            uint32_t new_height = static_cast<uint32_t>(std::round(*width / ext->aspectRatio_));
            *height = std::max(new_height, 1u);
        }
        
        return true;
    }

    bool GUIExtensionBase::clap_gui_set_size(const clap_plugin_t* plugin,
                                             uint32_t width,
                                             uint32_t height) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext) return false;

        // Store the dimensions
        ext->width_ = width;
        ext->height_ = height;
        
        // Update editor size if it exists
        if (ext->editor_)
        {
            ext->editor_->setWindowDimensions(width, height);
        }
        
        return true;
    }

    bool GUIExtensionBase::clap_gui_set_parent(const clap_plugin_t* plugin,
                                               const clap_window_t* window) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext || !ext->editor_) return false;

        // Show the editor in the parent window
        ext->editor_->show(window->ptr);

#ifdef __linux__
        // Register Linux FD support if available
        if (ext->hostFdSupport_ && ext->editor_->window())
        {
            int fd_flags = CLAP_POSIX_FD_READ | CLAP_POSIX_FD_WRITE | CLAP_POSIX_FD_ERROR;
            return ext->hostFdSupport_->register_(ext->host_, ext->editor_->window()->posixFd(), fd_flags);
        }
#endif
        
        return true;
    }

    bool GUIExtensionBase::clap_gui_set_transient(const clap_plugin_t* plugin,
                                                  const clap_window_t* window) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext || !ext->editor_) return false;

        // TODO: Implement transient window setting for floating windows
        return true;
    }

    void GUIExtensionBase::clap_gui_suggest_title(const clap_plugin_t* plugin,
                                                  const char* title) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext || !ext->editor_) return;

        // TODO: Set window title for floating windows
    }

    bool GUIExtensionBase::clap_gui_show(const clap_plugin_t* plugin) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext || !ext->editor_) return false;

        // Visage handles visibility through show() with parent
        return true;
    }

    bool GUIExtensionBase::clap_gui_hide(const clap_plugin_t* plugin) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext || !ext->editor_) return false;

        // Visage handles visibility through the host
        return true;
    }

    // Host callback helpers
    bool GUIExtensionBase::requestResize(uint32_t width, uint32_t height)
    {
        if (!hostGui_ || !editor_) return false;

        return hostGui_->request_resize(host_, width, height);
    }

    bool GUIExtensionBase::requestShow()
    {
        if (!hostGui_ || !editor_) return false;

        return hostGui_->request_show(host_);
    }

    bool GUIExtensionBase::requestHide()
    {
        if (!hostGui_ || !editor_) return false;

        return hostGui_->request_hide(host_);
    }

#ifdef __linux__
    // Linux POSIX FD support implementation
    void GUIExtensionBase::clap_on_posix_fd(const clap_plugin_t* plugin, int fd, clap_posix_fd_flags_t flags) noexcept
    {
        auto* ext = PluginBase::findExtension<GUIExtensionBase>(plugin);
        if (!ext) return;
        
        ext->onPosixFd(fd, flags);
    }

    void GUIExtensionBase::onPosixFd(int fd, clap_posix_fd_flags_t flags) noexcept
    {
        if (editor_ && editor_->window())
        {
            editor_->window()->processPluginFdEvents();
        }
    }
#endif

} // namespace applause
