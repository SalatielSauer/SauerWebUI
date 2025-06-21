function cubescript(cmd, callback) {
    if (window.cefQuery) {
        window.cefQuery({
            request: 'cubescript:' + cmd,
            onSuccess: function(result) {
                if (callback) callback(result);
            },
            onFailure: function(code, msg) {
                if (callback) callback(null);
            }
        });
    } else {
        alert('CEF integration is not available.');
        if (callback) callback(null);
    }
}

window.cubescript = cubescript;

class GameAssets {
    constructor() {
        this.absolutePath = 'file:///./';
        this.path_packages = this.absolutePath + 'packages';
        this.path_maps = this.path_packages + '/base';
        window.cubescript('nodebug [ result $allmaps ]', (allmaps) => {
            this.allmapnames = allmaps ? allmaps.split(' ') : [];
        })
    }
}

window.gameassets = new GameAssets();

document.addEventListener('keydown', function(e) {
    if (e.altKey && e.key === 'd') {
        e.preventDefault();
        if (window.cefQuery) window.cefQuery({ request: "openDevTools" });
    }
});

class WUI {
    constructor(rootId = 'wui-root') {
        this.root = document.getElementById(rootId);
        if (!this.root) {
            const root = document.createElement('div');
            root.id = rootId;
            root.className = 'wui-root';
            this.root = root;
        }
        this.menus = {};
        this.options = {};
        this.openMenuStack = [];
        this.onEscape = (callback = ()=>{}) => {
            // get topmost menu id
            const id = this.openMenuStack[this.openMenuStack.length-1];
            if (this.openMenuStack.length > 0) {
                this.clearMenu(id, this.openMenuStack.length > 1, false);
            }
            callback(this.openMenuStack, this.menus[id]);            
        }
    }

    createMenu(id, body, x, y, title, options = {}, event) {
        this.options = options;
        this.clearMenu(id, true, false);
        const menu = document.createElement('div');
        const container = document.createElement('div');

        menu.className = 'wui-menu';
        menu.id = `wui-menu-${id}`;

        container.className = 'wui-menu-container';
        container.appendChild(body);

        menu.appendChild(container);

        if (this.options.allowFullscreen) {
            const overlay = document.createElement('div');
            overlay.className = 'wui-menu-overlay';

            document.addEventListener('fullscreenchange', () => {
                if (document.fullscreenElement === menu) {
                    fullscreenButton.innerText = '❮ Exit Fullscreen';
                } else {
                    fullscreenButton.innerText = '⛶ Fullscreen';
                }
            });

            const fullscreenButton = document.createElement('button');
            fullscreenButton.className = 'wui-menu-smallbutton';
            fullscreenButton.innerText = '⛶ Fullscreen';

            fullscreenButton.onclick = async () => {
                if (document.fullscreenElement === menu) {
                    await document.exitFullscreen();
                } else {
                    await menu.requestFullscreen();
                }
            };

            overlay.prepend(fullscreenButton);
            menu.overlay = overlay;
            menu.prepend(overlay);
        }

        if (title) {
            const titleBar = document.createElement('div');
            const titleDiv = document.createElement('div');
            titleBar.className = 'wui-menu-title';
            titleDiv.innerText = title;
            titleBar.appendChild(titleDiv);
            menu.prepend(titleBar);
        }

        if (this.options.allowExit) {
            const exitButton = document.createElement('button');
            exitButton.className = 'wui-menu-exit';
            exitButton.innerText = '✕';
            exitButton.onclick = () => {
                if (this.options.clearOnExit === undefined || this.options.clearOnExit) {
                    this.clearMenu(id);
                } else {
                    this.hideMenu(id);
                }
            }
            menu.prepend(exitButton);
        }

        if (this.options.allowDrag === undefined) { this.options.allowDrag = true; }
        if (this.options.allowDrag) {
            menu.classList.add('wui_draggable');

            let px = 0, py = 0;
            if (typeof x === 'string' && x.endsWith('%')) {
                px = window.innerWidth * parseFloat(x) / 200 - menu.offsetWidth / 2;
            } else {
                px = parseInt(x) || 0;
            }
            if (typeof y === 'string' && y.endsWith('%')) {
                py = window.innerHeight * parseFloat(y) / 200 - menu.offsetHeight / 2;
            } else {
                py = parseInt(y) || 0;
            }

            menu.style.left = '0px';
            menu.style.top = '0px';
            menu.style.position = 'absolute';
            menu.style.transform = `translate(${px}px, ${py}px)`;
            menu.setAttribute('pos_x', px);
            menu.setAttribute('pos_y', py);
            // menu.setAttribute('force', this.options.allowDrag ? 'true' : 'false');
        } else {
            menu.style.left = x;
            menu.style.top = y;
            menu.style.position = 'absolute';
        }

        this.menus[id] = menu;
        this.root.appendChild(menu);
        return menu;
    }

    showMenu(id, event) {
        const menu = this.menus[id];
        if (menu) {
            menu.classList.add('active');
            if (this.openMenuStack[this.openMenuStack.length-1] !== id) {
                // Remove if already exists (avoid duplicates)
                this.openMenuStack = this.openMenuStack.filter(mid => mid !== id);
                this.openMenuStack.push(id);
            }
            // update menu position if event is provided
            if (event) {
                let x = event.clientX - menu.offsetWidth / 2;
                let y = event.clientY - menu.offsetHeight / 2;

                // ensure the menu stays within the viewport
                x = Math.max(0, Math.min(window.innerWidth - menu.offsetWidth, x));
                y = Math.max(0, Math.min(window.innerHeight - menu.offsetHeight, y));

                menu.style.transform = `translate(${x}px, ${y}px)`;
                menu.setAttribute('pos_x', x);
                menu.setAttribute('pos_y', y);
            }
            if (menu.onshow) {
                menu.onshow();
            }
        }

    }

    hideMenu(id, keep_cursor = false) {
        const menu = this.menus[id];
        if (menu) {
            this.openMenuStack = this.openMenuStack.filter(mid => mid !== id);
            if (!keep_cursor && this.openMenuStack.length === 0) {
                window.cubescript('showcursor 0');
            }
            menu.classList.remove('active');
            if (menu.ondisappear) {
                menu.ondisappear();
            }
        }
    }

    toggleMenu(id) {
        const menu = this.menus[id];
        if (menu) {
            if (menu.classList.contains('active')) {
                this.hideMenu(id);
            } else {
                this.showMenu(id);
            }
        }
    }

    clearMenu(id, keep_cursor = false, dispatch = true) {
        const menu = this.menus[id];
        if (menu) {
            this.openMenuStack = this.openMenuStack.filter(mid => mid !== id);
            if (!keep_cursor && this.openMenuStack.length === 0) {
                window.cubescript('showcursor 0');
            }
            if (menu.ondisappear && dispatch) {
                menu.ondisappear();
            }
            menu.remove();
            delete this.menus[id];
        }
    }

    createButton(text, onClick) {
        const button = document.createElement('button');
        const span = document.createElement('span');
        span.innerText = text;
        button.appendChild(span);
        button.onclick = onClick;
        return button;
    }

    createTextInput(placeholder, onChange) {
        const field = document.createElement('input');
        field.type = 'text';
        field.placeholder = placeholder;
        field.onchange = onChange;
        return field;
    }

    createTextArea(placeholder, onChange) {
        const field = document.createElement('textarea');
        field.placeholder = placeholder;
        field.onchange = onChange;
        return field;
    }

    clearAll() {
        Object.keys(this.menus).forEach(id => this.clearMenu(id));
    }

}

window.wui = new WUI();

window.handleTargetBlank = function(url) {
    if (url) {
        const iframe = document.createElement('iframe');
        iframe.src = url;
        window.wui.createMenu(`${url}`, iframe, '40%', '50%', '', { allowFullscreen: true, allowExit: true });
        window.wui.showMenu(url);
        document.exitFullscreen();
    }
}

interact('.wui_draggable').draggable({
    allowFrom: '.wui-menu-title, .wui-menu',
    listeners: {
        start (event) { },
        move (event) {
            let parent = event.target;
            if (parent.getAttribute('force') == 'true') return;

            let x = (parseInt(parent.getAttribute('pos_x')) || 0) + event.dx;
            let y = (parseInt(parent.getAttribute('pos_y')) || 0) + event.dy;

            // calculate boundaries allowing half of the element to go out of the viewport except for the top
            let minX = -parent.offsetWidth / 2;
            let maxX = window.innerWidth - parent.offsetWidth / 2;
            let minY = 0;  // no negative value for y as we don't allow top half to go out
            let maxY = window.innerHeight - parent.offsetHeight / 2;

            // apply boundaries
            x = Math.max(minX, Math.min(maxX, x));
            y = Math.max(minY, Math.min(maxY, y));

            x = Math.round(x);
            y = Math.round(y);
            parent.setAttribute('pos_x', x);
            parent.setAttribute('pos_y', y);

            parent.style.transform = `translate(${x}px, ${y}px)`;
        },
    }
});


document.addEventListener('contextmenu', function(event) {
    event.preventDefault();
});
