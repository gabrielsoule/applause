#pragma once
#include <cstdint>

namespace applause {
class ParamMessageQueue;

/**
 * @class IEditor
 * @brief Abstract interface for GUI editor implementations.
 *
 * This interface defines the contract that any GUI implementation must satisfy
 * to work with the Applause GUIExtension. It is intentionally minimal and
 * framework-agnostic.
 *
 * Implementations can be created using any GUI framework: JUCE,
 * Dear ImGui, vst-gui, or custom solutions.
 *
 * That said, we enthusiastically recommend our bespoke
 * Applause::ApplauseEditor!
 */
class IEditor {
public:
    virtual ~IEditor() = default;

    // Thread-safe parameter communication
    /**
     * @brief Get the message queue for thread-safe parameter updates.
     *
     * The editor owns and manages the message queue. GUIExtension will
     * call this method to get the queue and connect it to ParamsExtension.
     *
     * If you don't care about parameters, or have your own parameters extension
     * that bypasses ParamsExtension entirely, you can return a nullptr here.
     *
     * @return Pointer to the message queue (may be nullptr)
     * [main-thread]
     */
    virtual ParamMessageQueue* getMessageQueue() = 0;

    /**
     * @brief Show the editor window, either embedded or floating.
     *
     * @param parent_window Platform-specific parent window handle
     *                      (HWND on Windows, NSView* on macOS, Window on X11)
     *                      nullptr for floating windows
     * [main-thread]
     */
    virtual void show(void* parent_window) = 0;

    /**
     * @brief Close and hide the editor window.
     *
     * This should release any UI resources, but the editor object
     * itself will be destroyed separately.
     * [main-thread]
     */
    virtual void close() = 0;

    /**
     * @brief Get the current width of the editor window.
     * @return Width in pixels (physical or logical depending on platform)
     * [main-thread]
     */
    virtual uint32_t width() const = 0;

    /**
     * @brief Get the current height of the editor window.
     * @return Height in pixels (physical or logical depending on platform)
     * [main-thread]
     */
    virtual uint32_t height() const = 0;

    /**
     * @brief Set the editor window dimensions.
     */
    virtual void setWindowDimensions(uint32_t width, uint32_t height) = 0;

    /**
     * @brief Configure whether the editor maintains a fixed aspect ratio.
     */
    virtual void setFixedAspectRatio(bool fixed) = 0;

    /**
     * @brief Check if the editor is configured to maintain a fixed aspect
     * ratio.
     * @return true if aspect ratio is fixed, false otherwise
     */
    virtual bool isFixedAspectRatio() const = 0;

    /**
     * @brief Get the current aspect ratio of the editor.
     * @return The aspect ratio as width/height
     */
    virtual float getAspectRatio() const = 0;

    /**
     * @brief Get a native window handle if available.
     *
     * This is optional and may return nullptr. Used for advanced integrations.
     * @return Platform-specific window handle or nullptr
     */
    virtual void* getNativeHandle() { return nullptr; }

#ifdef __linux__
    /**
     * @brief Get the POSIX file descriptor for X11 event handling.
     *
     * Linux-specific: Returns the X11 connection file descriptor for
     * integration with the host's event loop.
     * @return File descriptor or -1 if not applicable
     * [main-thread]
     */
    virtual int getPosixFd() { return -1; }

    /**
     * @brief Process pending X11 events.
     *
     * Linux-specific: Called when the host detects events on the POSIX FD.
     * [main-thread]
     */
    virtual void processPosixFdEvents() {}
#endif
};

}  // namespace applause