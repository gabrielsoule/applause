/*
 * MacOS implementation of the native popup menu. Developed with plenty of LLM assistance; they know a lot more about
 * native OS widgets than I do.
 */


#include "PopupMenu.h"

#include <applause/ui/ApplauseUI.h>

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

static NSEventModifierFlags convertModifiers(int modifiers) {
    NSEventModifierFlags flags = 0;
    if (modifiers & kModifierCmd)
        flags |= NSEventModifierFlagCommand;
    if (modifiers & kModifierOption)
        flags |= NSEventModifierFlagOption;
    if (modifiers & kModifierMacCtrl)
        flags |= NSEventModifierFlagControl;
    if (modifiers & kModifierShift)
        flags |= NSEventModifierFlagShift;
    return flags;
}

static void buildNSMenu(NSMenu* nsMenu, const PopupMenu& menu, ApplauseNativeMenuHandler* handler) {
    for (const auto& option : menu.options()) {
        if (option.isBreak()) {
            [nsMenu addItem:[NSMenuItem separatorItem]];
            continue;
        }

        NSString* title = [NSString stringWithUTF8String:option.name().toUtf8().c_str()];
        const std::string& key = option.nativeShortcutCharacter();
        NSString* keyEquiv = key.empty() ? @"" : [NSString stringWithUTF8String:key.c_str()];

        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                      action:@selector(menuItemSelected:)
                                               keyEquivalent:keyEquiv];
        item.target = handler;
        item.tag = option.id();
        item.enabled = option.enabled();
        item.state = option.selected() ? NSControlStateValueOn : NSControlStateValueOff;

        if (!key.empty())
            item.keyEquivalentModifierMask = convertModifiers(option.nativeShortcutModifiers());

        if (option.hasOptions()) {
            NSMenu* submenu = [[NSMenu alloc] initWithTitle:title];
            buildNSMenu(submenu, option, handler);
            [item setSubmenu:submenu];
            item.action = nil;
        }

        [nsMenu addItem:item];
    }
}

static const PopupMenu* findOptionById(const PopupMenu& menu, int id) {
    for (const auto& option : menu.options()) {
        if (option.isBreak())
            continue;
        if (!option.hasOptions() && option.id() == id)
            return &option;
        if (const auto* found = findOptionById(option, id))
            return found;
    }
    return nullptr;
}

void PopupMenu::showNative(Frame* source, Point position) const {
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
