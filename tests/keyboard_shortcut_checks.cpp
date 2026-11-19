#include "ui/keyboard_shortcuts.h"
#include <stdexcept>
#include <iostream>

int main()
{
    using namespace pserv::winui;
    auto expect = [](ShortcutCommand actual, Shortcut expected, int provider = -1)
    {
        if (actual.command != expected || actual.provider != provider)
            throw std::runtime_error("Incorrect shortcut routing");
    };
    for (int i = 0; i < 10; ++i)
        expect(DecodeShortcut(i == 9 ? '0' : '1' + i, true, false, false, false, false, false), Shortcut::Provider, i);
    expect(DecodeShortcut('0', true, true, false, false, false, false), Shortcut::ResetZoom);
    for (int key : {0xBB, 0x6B})
    {
        expect(DecodeShortcut(key, true, false, false, false, false, false), Shortcut::ZoomIn);
        expect(DecodeShortcut(key, true, true, false, false, false, false), Shortcut::ZoomIn);
    }
    for (int key : {0xBD, 0x6D})
        expect(DecodeShortcut(key, true, false, false, false, false, false), Shortcut::ZoomOut);
    expect(DecodeShortcut(0x2E, false, false, false, false, true, false), Shortcut::DeleteSelection);
    expect(DecodeShortcut(0x2E, false, false, false, true, true, false), Shortcut::None);
    expect(DecodeShortcut(0x2E, false, false, false, false, false, false), Shortcut::None);
    expect(DecodeShortcut(0x2E, false, false, false, false, true, true), Shortcut::None);
    expect(DecodeShortcut(0x2E, false, false, true, false, true, false), Shortcut::None);
    expect(DecodeShortcut(0x2E, false, true, false, false, true, false), Shortcut::None);
    expect(DecodeShortcut(0x74, false, false, false, false, false, true), Shortcut::None);
    expect(DecodeShortcut(0x74, false, false, true, false, false, false), Shortcut::None);
    for (int key : std::initializer_list<int>{'1', '0', 0xBB, 0xBD})
        expect(DecodeShortcut(key, true, false, false, true, false, false), Shortcut::None);
    for (int key : std::initializer_list<int>{'F', 'R', '1', 0xBB, 0x2E, 0x74})
    {
        expect(DecodeShortcut(key, true, false, true, false, true, false), Shortcut::None);
        expect(DecodeShortcut(key, true, false, false, false, true, true), Shortcut::None);
    }
    expect(DecodeShortcut('F', true, false, false, true, false, false), Shortcut::Search);
    expect(DecodeShortcut('R', true, false, false, false, false, false), Shortcut::ToggleRefresh);
    expect(DecodeShortcut(0x74, false, false, false, true, false, false), Shortcut::Refresh);
    std::cout << "Keyboard routing checks passed; no UI or administrative actions executed.\n";
}
