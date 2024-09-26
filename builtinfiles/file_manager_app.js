
const urlParams = new URLSearchParams(window.location.search);

var dir = urlParams.get('dir');
if (!dir) {
    dir = '/';
}
if (dir.endsWith('/')) {
    dir = dir.substring(0, dir.length - 1);
}

import { html, css, LitElement } from '/builtin/lit.min.js';
import styles from '/builtin/barrel.css' with { type: 'css' };
import theme from '/api/cogs.theme.css' with { type: 'css' };

// A plugin page only requires a these two exports.

export const metadata = {
    title: "Cogs",
}

export class PageRoot extends LitElement {
    static styles = [styles, theme];

    static properties = {
        data: { type: Object },
    };

    constructor() {
        super();
        this.data = { "files": {}, "dirs": {}, "path": dir };
        var t = this;

        async function getData() {
            var d = dir
            if (!d.startsWith("/")) {
                d = "/" + dir
            }
            var x = await fetch('/api/cogs.listdir?dir=' + d);
            var y = await x.json();
            y.dirs = y.dirs || {}
            y.path = dir;
            t.data = y;
        }
        getData();
    }




    async handleDelete(path) {
        if (!confirm("Delete " + path + "?")) {
            return;
        }
        if (path.startsWith('/config')) {
            alert("Error: Cannot delete config files");
            return;
        }
        var fd = new FormData();
        fd.append('file', dir + "/" + path);

        try {
            await fetch('/api/cogs.deletefile', {
                method: 'POST',
                body: fd
            })

            window.location.reload();
        }
        catch (err) {
            alert("Error: " + err);
        }
    }

    async handleRenameRequest(path) {
        var fd = new FormData();
        fd.append('file', dir + "/" + path);
        var n = prompt("Enter new name:", dir + "/" + path);
        if (n.indexOf("'") != -1 || n.indexOf('"') != -1) {
            alert("Error: name cannot contain quotes");
        }
        fd.append('newname', n);
        try {
            await fetch('/api/cogs.renamefile', {
                method: 'POST',
                body: fd
            })

            window.location.reload();
        }
        catch (err) {
            alert("Error: " + err);
        }
    }


    render() {
        return html`
        <div>
        <h2>${this.data.path}</h2>
        <p>Free flash: ${this.data.freeflash} of ${this.data.totalflash}</p>
        <form method="POST" enctype="multipart/form-data" target="/api/cogs.upload">
            <input type="file" name="file" id="file">
            <input type="hidden" name="path" id="path" value="${this.data.path}">
            <input type="submit" value="Upload File">
        </form>

        <ul>
            ${Object.entries(this.data.dirs).map(([key, value]) => html`
            <li><a href="/default-template?load-module=/builtin/files_app.js&dir=${this.data.path}/${key}">${key}/</a>
            <button onclick="handleDelete('${key}');">Delete</button>
            </li>
            `)}

            ${Object.entries(this.data.files).map(([key, value]) => html`
            <li><a href="/api/cogs.download/?file=${this.data.path}/${key}">${key}(${value})</a>
            <button onclick="handleRenameRequest('${key}');">Rename</button>
            <button onclick="handleDelete('${key}');">Delete</button>
            </li>
            `)}
        </ul>

        </div>
        `;
    }
}