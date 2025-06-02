// handles keyboard input and editing logic on the javascript side manually,
// because cef does not want to propagate the keys for some reason.
// consequently, inputs defined here will only work on pages that inject these definitions.

// ---- cef input notification ----
function notifyCEFInputActive(active) {
    if (window.cefQuery) {
        window.cefQuery({ request: 'cef_input:' + (active ? '1' : '0') });
    }
}

window.notifyCEFInputActive = notifyCEFInputActive;

document.addEventListener('focusin', function(e) {
    if (e.target.matches('input[type="text"], textarea, [contenteditable="true"]')) {
        window.notifyCEFInputActive(true);
    }
});

document.addEventListener('focusout', function(e) {
    if (e.target.matches('input[type="text"], textarea, [contenteditable="true"]')) {
        window.notifyCEFInputActive(false);
    }
});

// ---- insert character handler ----
window._cef_insert_character = function(ch) {
    var el = document.activeElement;
    if (el && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA' || el.isContentEditable)) {
        var start = el.selectionStart, end = el.selectionEnd, val = el.value;
        if (typeof start === "number" && typeof end === "number") {
            el.value = val.slice(0, start) + ch + val.slice(end);
            //console.log('inserting', el.value);
            el.selectionStart = el.selectionEnd = start + ch.length;
        } else if (el.isContentEditable) {
            document.execCommand("insertText", false, ch);
        }
    }
};

// ---- cef key handling ----
window._cef_handle_key = function(key, down, mods) {
    var el = document.activeElement;
    if (!el) return;

    // normalize modifiers
    mods = mods || {};
    var eventOpts = {
        key: key,
        code: key,
        bubbles: true,
        cancelable: true,
        shiftKey: !!mods.shift,
        ctrlKey: !!mods.ctrl,
        altKey: !!mods.alt,
        metaKey: !!mods.meta
    };

    // dispatch keydown/keyup
    var evtType = down ? 'keydown' : 'keyup';
    var evt = new KeyboardEvent(evtType, eventOpts);
    el.dispatchEvent(evt);

    // manual key handling (on keydown only)
    if (down) {

        // escape: blur
        if (key === "Escape") {
            if (typeof el.blur === "function") {
                el.blur();
            }
            evt.preventDefault();
            return;
        }

        // ctrl+a: select all
        if (key.toLowerCase() === 'a' && mods.ctrl) {
            if ((el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') && typeof el.select === "function") {
                el.select();
            } else if (el.isContentEditable) {
                var range = document.createRange();
                range.selectNodeContents(el);
                var sel = window.getSelection();
                sel.removeAllRanges();
                sel.addRange(range);
            }
            evt.preventDefault();
            return;
        }

        // ctrl+c: copy
        if (key.toLowerCase() === 'c' && mods.ctrl) {
            let text = '';
            if ((el.tagName === 'INPUT' || el.tagName === 'TEXTAREA')) {
                text = el.value.substring(el.selectionStart, el.selectionEnd);
            } else if (el.isContentEditable) {
                text = window.getSelection().toString();
            }
            if (text) {
                if (navigator.clipboard && window.isSecureContext) {
                    navigator.clipboard.writeText(text);
                } else {
                    // fallback for old browser/Cef context
                    try { document.execCommand('copy'); } catch(e){}
                }
            }
            evt.preventDefault();
            return;
        }

        // ctrl+x: cut
        if (key.toLowerCase() === 'x' && mods.ctrl) {
            let text = '';
            if ((el.tagName === 'INPUT' || el.tagName === 'TEXTAREA')) {
                text = el.value.substring(el.selectionStart, el.selectionEnd);
                if (text) {
                    if (navigator.clipboard && window.isSecureContext) {
                        navigator.clipboard.writeText(text);
                    } else {
                        try { document.execCommand('cut'); } catch(e){}
                    }
                    // Remove text from input/textarea
                    const start = el.selectionStart;
                    const end = el.selectionEnd;
                    el.value = el.value.slice(0, start) + el.value.slice(end);
                    el.selectionStart = el.selectionEnd = start;
                    el.dispatchEvent(new Event('input', { bubbles: true }));
                }
            } else if (el.isContentEditable) {
                text = window.getSelection().toString();
                if (text) {
                    if (navigator.clipboard && window.isSecureContext) {
                        navigator.clipboard.writeText(text);
                    } else {
                        try { document.execCommand('cut'); } catch(e){}
                    }
                    document.execCommand('delete');
                }
            }
            evt.preventDefault();
            return;
        }

        // ctrl+v: paste
        if (key.toLowerCase() === 'v' && mods.ctrl) {
            if (navigator.clipboard && window.isSecureContext) {
                navigator.clipboard.readText().then(function(text) {
                    if ((el.tagName === 'INPUT' || el.tagName === 'TEXTAREA')) {
                        const start = el.selectionStart;
                        const end = el.selectionEnd;
                        el.value = el.value.slice(0, start) + text + el.value.slice(end);
                        el.selectionStart = el.selectionEnd = start + text.length;
                        el.dispatchEvent(new Event('input', { bubbles: true }));
                    } else if (el.isContentEditable) {
                        document.execCommand('insertText', false, text);
                    }
                });
            } else {
                // fallback for old browser/Cef context
                try { document.execCommand('paste'); } catch(e){}
            }
            evt.preventDefault();
            return;
        }


        // backspace
        if (key === "Backspace") {
            if (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') {
                var s = el.selectionStart, e = el.selectionEnd, v = el.value;
                if (typeof s === "number" && typeof e === "number") {
                    if (s !== e) {
                        // delete selection
                        el.value = v.slice(0, s) + v.slice(e);
                        el.selectionStart = el.selectionEnd = s;
                    } else if (s > 0) {
                        // delete previous char
                        el.value = v.slice(0, s - 1) + v.slice(e);
                        el.selectionStart = el.selectionEnd = s - 1;
                    }
                }
                evt.preventDefault();
                return;
            } else if (el.isContentEditable) {
                document.execCommand("delete");
                evt.preventDefault();
                return;
            }
        }


        // enter
        if (key === "Enter") {
            if (el.tagName === 'TEXTAREA' || el.isContentEditable) {
                // insert newline (browser default for textarea/contenteditable)
                if (el.tagName === 'TEXTAREA') {
                    var s = el.selectionStart, e = el.selectionEnd, v = el.value;
                    el.value = v.slice(0, s) + "\n" + v.slice(e);
                    el.selectionStart = el.selectionEnd = s + 1;
                } else if (el.isContentEditable) {
                    document.execCommand("insertLineBreak");
                }
                evt.preventDefault();
                return;
            } else if (el.tagName === 'INPUT') {
                // submit form, blur input
                el.dispatchEvent(new Event('change', { bubbles: true }));
                var form = el.form;
                if (form) form.dispatchEvent(new Event('submit', {bubbles:true, cancelable:true}));
                if (typeof el.blur === "function") el.blur();
                evt.preventDefault();
                return;
            }
        }

        // tab
        else if (key === "Tab") {
            if ((el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') && typeof el.selectionStart === "number") {
                var s = el.selectionStart, e = el.selectionEnd, v = el.value;
                el.value = v.slice(0, s) + "\t" + v.slice(e);
                el.selectionStart = el.selectionEnd = s + 1;
            } else if (el.isContentEditable) {
                document.execCommand("insertText", false, "\t");
            }
        }

        // arrow keys (manual handling for input/textarea)
        if (key === "ArrowLeft" || key === "ArrowRight" || key === "ArrowUp" || key === "ArrowDown") {
            if (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') {
                var s = el.selectionStart, e = el.selectionEnd, len = el.value.length;
                if (typeof s === "number" && typeof e === "number") {
                    // no selection: move caret
                    if (s === e) {
                        if (key === "ArrowLeft" && s > 0) {
                            el.selectionStart = el.selectionEnd = s - 1;
                        } else if (key === "ArrowRight" && s < len) {
                            el.selectionStart = el.selectionEnd = s + 1;
                        } else if (key === "ArrowUp" || key === "ArrowDown") {
                            if (el.tagName === 'TEXTAREA') {
                                var lines = el.value.substr(0, s).split('\n');
                                var currentLine = lines.length - 1;
                                var col = lines[lines.length - 1].length;
                                if (key === "ArrowUp" && currentLine > 0) {
                                    var prevLineLen = lines[lines.length - 2].length;
                                    var newPos = s - col - 1;
                                    el.selectionStart = el.selectionEnd = newPos - Math.max(0, col - prevLineLen);
                                }
                                if (key === "ArrowDown" && currentLine < el.value.split('\n').length - 1) {
                                    var after = el.value.substr(s).split('\n');
                                    var nextLineLen = after.length > 1 ? after[1].length : 0;
                                    var linesAll = el.value.split('\n');
                                    var newCol = Math.min(col, linesAll[currentLine + 1].length);
                                    var nextLineStart = s + (linesAll[currentLine].length - col) + 1;
                                    el.selectionStart = el.selectionEnd = nextLineStart + newCol;
                                }
                            }
                        }
                    }
                    // selection exists: collapse to left or right
                    else {
                        if (key === "ArrowLeft") {
                            el.selectionStart = el.selectionEnd = s;
                        } else if (key === "ArrowRight") {
                            el.selectionStart = el.selectionEnd = e;
                        }
                    }
                }
                evt.preventDefault();
                return;
            }
        }
    }
};
