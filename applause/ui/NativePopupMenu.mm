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

void NativePopupMenu::show(Frame* source, Point position) {
    auto* editor = dynamic_cast<ApplicationEditor*>(source);
    if (!editor)
        editor = source->findParent<ApplicationEditor>();
    if (!editor || !editor->window())
        return;

    NSView* view = (__bridge NSView*)editor->window()->nativeHandle();
    Point window_position = source->positionInWindow() + position * source->scaleInWindow();
    ApplauseNativeMenuHandler* handler = [[ApplauseNativeMenuHandler alloc] init];

    NSMenu* menu = [[NSMenu alloc] init];
    [menu setAutoenablesItems:NO];
    buildNSMenu(menu, *this, handler);

    NSPoint location = NSMakePoint(window_position.x, view.bounds.size.height - window_position.y);
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
