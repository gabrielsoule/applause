#pragma once

#include "core/Extension.h"
#include <clap/ext/gui.h>
#ifdef __linux__
#include <clap/ext/posix-fd-support.h>
#endif
#include <string>
#include <memory>
#include <type_traits>

#include "ui/Editor.h"

namespace applause
{
    /**
     * @class GUIExtensionBase
     * @brief Non-templated base class that handles all CLAP GUI callbacks.
     * The non-templated base class is necessary to interface with the CLAP ABI correctly.
     */
    class GUIExtensionBase : public IExtension
    {
    protected:
        // Static CLAP callbacks - these must be non-templated, hence our use of a non-templated GUIExtensionBase
        static bool clap_gui_is_api_supported(const clap_plugin_t* plugin,
                                          const char* api,
                                          bool is_floating) noexcept;

        static bool clap_gui_get_preferred_api(const clap_plugin_t* plugin,
                                           const char** api,
                                           bool* is_floating) noexcept;

        static bool clap_gui_create(const clap_plugin_t* plugin,
                                  const char* api,
                                  bool is_floating) noexcept;

        static void clap_gui_destroy(const clap_plugin_t* plugin) noexcept;

        static bool clap_gui_set_scale(const clap_plugin_t* plugin,
                                    double scale) noexcept;

        static bool clap_gui_get_size(const clap_plugin_t* plugin,
                                   uint32_t* width,
                                   uint32_t* height) noexcept;

        static bool clap_gui_can_resize(const clap_plugin_t* plugin) noexcept;

        static bool clap_gui_get_resize_hints(const clap_plugin_t* plugin,
                                          clap_gui_resize_hints_t* hints) noexcept;

        static bool clap_gui_adjust_size(const clap_plugin_t* plugin,
                                      uint32_t* width,
                                      uint32_t* height) noexcept;

        static bool clap_gui_set_size(const clap_plugin_t* plugin,
                                   uint32_t width,
                                   uint32_t height) noexcept;

        static bool clap_gui_set_parent(const clap_plugin_t* plugin,
                                     const clap_window_t* window) noexcept;

        static bool clap_gui_set_transient(const clap_plugin_t* plugin,
                                        const clap_window_t* window) noexcept;

        static void clap_gui_suggest_title(const clap_plugin_t* plugin,
                                        const char* title) noexcept;

        static bool clap_gui_show(const clap_plugin_t* plugin) noexcept;

        static bool clap_gui_hide(const clap_plugin_t* plugin) noexcept;

#ifdef __linux__
        // Linux POSIX FD support
        static void clap_on_posix_fd(const clap_plugin_t* plugin, int fd, clap_posix_fd_flags_t flags) noexcept;
#endif

        // CLAP GUI extension struct
        static constexpr clap_plugin_gui_t clap_struct_ = {
            .is_api_supported = clap_gui_is_api_supported,
            .get_preferred_api = clap_gui_get_preferred_api,
            .create = clap_gui_create,
            .destroy = clap_gui_destroy,
            .set_scale = clap_gui_set_scale,
            .get_size = clap_gui_get_size,
            .can_resize = clap_gui_can_resize,
            .get_resize_hints = clap_gui_get_resize_hints,
            .adjust_size = clap_gui_adjust_size,
            .set_size = clap_gui_set_size,
            .set_parent = clap_gui_set_parent,
            .set_transient = clap_gui_set_transient,
            .suggest_title = clap_gui_suggest_title,
            .show = clap_gui_show,
            .hide = clap_gui_hide
        };

    protected:
        const clap_host_t* host_;
        const clap_host_gui_t* hostGui_;
#ifdef __linux__
        const clap_host_posix_fd_support_t* hostFdSupport_;
#endif
        std::unique_ptr<Editor> editor_;
        ParamsExtension* params_ = nullptr; // an optional params extension
        int width_;
        int height_;
        bool fixedAspectRatio_;
        float aspectRatio_;

        virtual void createEditor() = 0;
        virtual void destroyEditor() = 0;

#ifdef __linux__
        // Called when Linux FD events occur
        virtual void onPosixFd(int fd, clap_posix_fd_flags_t flags) noexcept;
#endif

    public:
        explicit GUIExtensionBase(const clap_host_t* host, int defaultWidth = 800, int defaultHeight = 600, bool fixedAspectRatio = false);

        static constexpr const char* ID = CLAP_EXT_GUI;
        const char* id() const override { return ID; }
        const void* getClapExtensionStruct() const override { return &clap_struct_; }

        void registerParamsExtension(ParamsExtension* params);
        

        virtual Editor* getEditor() { return editor_.get(); }

        // Host GUI callback helpers
        bool requestResize(uint32_t width, uint32_t height);
        bool requestShow();
        bool requestHide();
    };

    /**
     * Templated GUI extension that manages an applause::Editor.
     */
    template<typename TEditor>
    class GUIExtension : public GUIExtensionBase
    {
        static_assert(std::is_base_of_v<Editor, TEditor>, 
                      "TEditor must derive from applause::Editor");
        
    protected:
        void createEditor() override {
            editor_ = std::make_unique<TEditor>(params_);
            editor_->setWindowDimensions(width_, height_);
        }

        void destroyEditor() override
        {
            editor_->close();
            editor_ = nullptr;
        }

    public:
        using GUIExtensionBase::GUIExtensionBase;  // Inherit constructors
        
        // Type-safe access to the specific editor type
        TEditor* getEditor() override {
            return static_cast<TEditor*>(editor_.get()); 
        }

        GUIExtension(const clap_host_t* host, int width, int height)
            : GUIExtensionBase(host, width, height) {

        }
    };
    
} // namespace applause