
static const char jsoneditor_js[] = R"(
const urlParams = new URLSearchParams(window.location.search);
const schema_url = urlParams.get('schema');
var filename = urlParams.get('filename');

var fileurl = new URL('/api/cogs.download', window.location.origin);
fileurl.searchParams.append('file', filename);

var fileuploadurl = new URL('/api/cogs.upload', window.location.origin);
fileuploadurl.searchParams.append('file', filename);
var editor = null


import { html, css, LitElement } from '/builtin/lit.min.js';

// A plugin page only requires a these two exports. 

export const metadata = {
    title: "Cogs",
}

export class PageRoot extends LitElement {

    updated() {
        if(this.initialized) {
        return;
        }


        var jes = document.createElement('script')
        jes.type = "module"
        jes.src = '/builtin/jsoneditor.min.js'
        document.head.appendChild(jes);
        var t = this;

        t.data = {
            expr_completions : [],
            tags : [],
        };

        // Wait till the script we need is loaded
        jes.addEventListener('load', async () => {
            const response = await fetch(schema_url);
            const schema_data = await response.json();
            const raw = JSON.stringify(schema_data);

            if("expr_completions" in raw) {
                const expr_completions = await fetch('/api/expr_completions');
                t.data.expr_completions = (await expr_completions.json())['datalist'];
            }

            if("tags" in raw) {
                const tags = await fetch('/api/tags');
                t.data.tags = (await tags.json())['tags'];
            }

            editor = new JSONEditor(t.shadowRoot.getElementById("editor_holder"), {
                schema: schema_data
            });

            const response2 = await fetch(fileurl);
            const filedata = await response2.json();

            editor.setValue(filedata);
        });

    }

    handleSubmit() {
        var fd = new FormData();
        fd.append('file', filename);
        fd.append('data', JSON.stringify(editor.getValue()));

        fetch('/api/cogs.setfile', {
            method: 'POST',
            body: fd
        })
    }


    render() {
        return html`
        <div>
        <datalist id="tags">
            ${Object.entries(this.data.tags).map(([key, value]) => html`
            <option value="${key}">${value}"></option>
            `)}
        </datalist>
        <datalist id="expr_completions">
            ${Object.entries(this.data.expr_completions).map(([key, value]) => html`
            <option value="${key}">${value}"></option>
            `)}
        </datalist>
            <div id="editor_holder"></div>
            <button @click=${this.handleSubmit}>Submit</button>
        </div>
        `;
    }
}
)";
