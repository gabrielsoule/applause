/*
 * MacOS implementation of the native popup menu. Developed with plenty of LLM assistance; they know a lot more about
 * native OS widgets than I do.
 */


#include "NativePopupMenu.h"

#import <Cocoa/Cocoa.h>

@interface ApplauseNativeMenuHandler : NSObject {
    bool _selectionMade;
    int _selectedId;
}
@property (nonatomic, readonly) bool selectionMade;
@property (nonatomic, readonly) int selectedId;
- (void)menuItemSelected:(NSMenuItem*)sender;
@end

@implementation ApplauseNativeMenuHandler

- (instancetype)init {
    self = [super init];
    if (self) {
        _selectionMade = false;
        _selectedId = -1;
    }
    return self;
}

- (bool)selectionMade { return _selectionMade; }
- (int)selectedId { return _selectedId; }

- (void)menuItemSelected:(NSMenuItem*)sender {
    _selectionMade = true;
    _selectedId = static_cast<int>(sender.tag);
}

@end

namespace applause {

using Modifier = NativePopupMenu::Modifier;

static NSEventModifierFlags convertModifiers(Modifier modifiers) {
    NSEventModifierFlags flags = 0;
    if ((modifiers & Modifier::Cmd) != Modifier::None)
        flags |= NSEventModifierFlagCommand;
    if ((modifiers & Modifier::Option) != Modifier::None)
        flags |= NSEventModifierFlagOption;
    if ((modifiers & Modifier::Ctrl) != Modifier::None)
        flags |= NSEventModifierFlagControl;
    if ((modifiers & Modifier::Shift) != Modifier::None)
        flags |= NSEventModifierFlagShift;
    return flags;
}

static void buildNSMenu(NSMenu* nsMenu, const NativePopupMenu& menu, ApplauseNativeMenuHandler* handler) {
    for (const auto& option : menu.options()) {
        if (option.isBreak()) {
            [nsMenu addItem:[NSMenuItem separatorItem]];
            continue;
        }

        NSString* title = [NSString stringWithUTF8String:option.name().c_str()];
        NSString* keyEquiv = option.shortcutKey().empty()
                                 ? @""
                                 : [NSString stringWithUTF8String:option.shortcutKey().c_str()];

        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                      action:@selector(menuItemSelected:)
                                               keyEquivalent:keyEquiv];
        item.target = handler;
        item.tag = option.id();
        item.enabled = option.isEnabled();
        item.state = option.isSelected() ? NSControlStateValueOn : NSControlStateValueOff;

        if (!option.shortcutKey().empty())
            item.keyEquivalentModifierMask = convertModifiers(option.shortcutModifiers());

        if (option.hasOptions()) {
            NSMenu* submenu = [[NSMenu alloc] initWithTitle:title];
            buildNSMenu(submenu, option, handler);
            [item setSubmenu:submenu];
            item.action = nil;
        }

        [nsMenu addItem:item];
    }
}

static const NativePopupMenu* findOptionById(const NativePopupMenu& menu, int id) {
    for (const auto& option : menu.options()) {
        if (option.isBreak())
            continue;
        if (option.id() == id)
            return &option;
        if (option.hasOptions()) {
            if (const auto* found = findOptionById(option, id))
                return found;
        }
    }
    return nullptr;
}

NativePopupMenu::NativePopupMenu() = default;
NativePopupMenu::NativePopupMenu(const std::string& name, int id) : name_(name), id_(id) {}
NativePopupMenu::~NativePopupMenu() = default;
NativePopupMenu::NativePopupMenu(NativePopupMenu&& other) noexcept = default;
NativePopupMenu& NativePopupMenu::operator=(NativePopupMenu&& other) noexcept = default;

NativePopupMenu& NativePopupMenu::addOption(int option_id, const std::string& option_name) {
    options_.emplace_back(option_name, option_id);
    return options_.back();
}

void NativePopupMenu::addBreak() {
    NativePopupMenu separator;
    separator.is_break_ = true;
    options_.push_back(std::move(separator));
}

NativePopupMenu& NativePopupMenu::addSubMenu(const std::string& name) {
    options_.emplace_back(name);
    return options_.back();
}

NativePopupMenu& NativePopupMenu::enable(bool enabled) {
    enabled_ = enabled;
    return *this;
}

NativePopupMenu& NativePopupMenu::select(bool selected) {
    selected_ = selected;
    return *this;
}

NativePopupMenu& NativePopupMenu::withKeyboardShortcut(Modifier modifiers, const std::string& key) {
    shortcut_modifiers_ = modifiers;
    shortcut_key_ = key;
    return *this;
}

void NativePopupMenu::show(void* native_view_handle, float x, float y) {
    if (!native_view_handle)
        return;

    NSView* view = (__bridge NSView*)native_view_handle;
    ApplauseNativeMenuHandler* handler = [[ApplauseNativeMenuHandler alloc] init];

    NSMenu* menu = [[NSMenu alloc] init];
    [menu setAutoenablesItems:NO];
    buildNSMenu(menu, *this, handler);

    NSPoint location = NSMakePoint(x, view.bounds.size.height - y);
    [menu popUpMenuPositioningItem:nil atLocation:location inView:view];

    if (handler.selectionMade) {
        int selectedId = handler.selectedId;
        if (const auto* selected = findOptionById(*this, selectedId))
            selected->onSelection().callback(selectedId);
        on_selection_.callback(selectedId);
    } else {
        on_cancel_.callback();
    }
}

}  // namespace applause
