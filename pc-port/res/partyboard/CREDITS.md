# PartyBoard menu resources

`window.rcss` and `tabbing.rcss` are exact copies from the user's local `C:/Marioparty4-switch-port/res/rml` reference, originating from https://github.com/zeranemesis/Marioparty4.
The reference's UI component source credits TwilitRealm.
The Sunshine RmlUi document uses the same window, tab bar, pane and select-button structure, original stylesheets and font files.
`sunshine.rcss` only supplies layout for the Sunshine service panels and footer.

Font resources are copied from that same reference: FOT-NewRodin Pro DB, Alegreya SC, Material Symbols Rounded and Inter.
FOT-NewRodin is a Fontworks font; copying it from a reference checkout does not grant a new redistribution license.
Preserve the original font licenses/permissions when packaging a public release.
Inter is loaded as the fallback font.

Navigation: mouse, Tab/Shift+Tab, arrow keys on tabs/options, Enter/Space, Page Up/Page Down for tabs and Escape to resume/cancel key capture.
The SDL backend can translate controller input into those same RmlUi navigation keys.
The backend supplies PAD-compatible key names for binding capture.

The proprietary FOT-NewRodin file is deliberately excluded from Git. It is optional; the menu loads Inter as a fallback. Supply it locally only with appropriate permission.
