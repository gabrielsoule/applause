#include "ApplauseEditor.h"

#include <applause/ui/ApplauseUI.h>

#include <applause/ui/Tooltip.h>
#include <applause/util/DebugHelpers.h>

#ifndef NDEBUG
#include <applause/util/inspector/InspectorWindow.h>
#endif

namespace applause {
ApplauseEditor::ApplauseEditor(ParamsExtension* params) : params_(params) {
    // If params provided, connect our message queue and start the timer
    if (params_) {
        params_->setMessageQueue(&message_queue_);
        startTimer(30);
    } else {
        LOG_WARN(
            "ApplauseEditor instantiated without ParamsExtension! Parameter "
            "sync is disabled. Are you sure you want to do this?");
    }

    tooltip_display_ = std::make_unique<TooltipDisplay>();
    addChild(tooltip_display_.get());
    tooltip_display_->setOnTop(true);

    onResize() += [this] {
        if (tooltip_display_)
            tooltip_display_->setBounds(localBounds());
    };
    tooltip_display_->setBounds(localBounds());

#ifndef NDEBUG
    // The inspector lives in its own native window; it is NOT a child of
    // the editor and never draws inside it (apart from a transient invisible
    // capture frame while Pick mode is active). Stays hidden until toggled.
    inspector_window_ = std::make_unique<inspector::InspectorWindow>(*this);

    setAcceptsKeystrokes(true);
    onKeyPress() += [this](const applause::KeyEvent& e) -> bool {
        if (!e.key_down) return false;
        if (e.key_code == applause::KeyCode::I && e.isMainModifier()
            && !e.isShiftDown()) {
            inspector_window_->toggleShown();
            return true;
        }
        return false;
    };
#endif
}

ApplauseEditor::~ApplauseEditor() {
    if (params_) {
        params_->setMessageQueue(nullptr);
    }
}

ParamMessageQueue* ApplauseEditor::getMessageQueue() { return &message_queue_; }

ParamsExtension* ApplauseEditor::getParamsExtension() { return params_; }

void ApplauseEditor::show(void* parent_window) {
    applause::ApplicationWindow::show(parent_window);
}

void ApplauseEditor::close() { applause::ApplicationWindow::close(); }

uint32_t ApplauseEditor::width() const {
    return static_cast<uint32_t>(applause::ApplicationWindow::width());
}

uint32_t ApplauseEditor::height() const {
    return static_cast<uint32_t>(applause::ApplicationWindow::height());
}

void ApplauseEditor::setWindowDimensions(uint32_t width, uint32_t height) {
    applause::ApplicationWindow::setWindowDimensions(static_cast<int>(width),
                                                   static_cast<int>(height));
}

void ApplauseEditor::setFixedAspectRatio(bool fixed) {
    applause::ApplicationWindow::setFixedAspectRatio(fixed);
}

bool ApplauseEditor::isFixedAspectRatio() const {
    return applause::ApplicationWindow::isFixedAspectRatio();
}

float ApplauseEditor::getAspectRatio() const {
    return applause::ApplicationWindow::aspectRatio();
}

void ApplauseEditor::draw(applause::Canvas& canvas) {
    canvas.setColor(0xff111115);
    canvas.fill(0, 0, width(), height());
}

void* ApplauseEditor::getNativeHandle() {
    if (auto* win = window())
        return win->nativeHandle();
    return nullptr;
}

#ifdef __linux__
int ApplauseEditor::getPosixFd() {
    if (auto* win = window()) {
        return win->posixFd();
    }
    return -1;
}

void ApplauseEditor::processPosixFdEvents() {
    if (auto* win = window()) {
        win->processPluginFdEvents();
    }
}
#endif

void ApplauseEditor::timerCallback() {
    // Only process messages if we have params
    if (!params_) return;

    ParamMessageQueue::Message msg{};
    while (message_queue_.toUi().try_dequeue(msg)) {
        if (msg.type == ParamMessageQueue::MessageType::PARAM_VALUE) {
            auto& param = params_->getInfo(msg.paramId);
            param.on_value_changed(msg.value);
        } else {
            ASSERT_FALSE("ApplauseEditor received unexpected message type");
        }
    }
}
}  // namespace applause
