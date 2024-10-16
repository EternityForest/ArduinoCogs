const urlParams = new URLSearchParams(window.location.search);

var schema_url = urlParams.get('schema');
schema_url = new URL(schema_url, window.location.origin);
var filename = urlParams.get('filename');
const cacheid = urlParams.get('cacheid') || "";
var fileurl = new URL('/api/cogs.download', window.location.origin);
fileurl.searchParams.append('file', filename);
fileurl.searchParams.append('cacheid', Date.now())
schema_url.searchParams.append('cacheid', Date.now())

var fileuploadurl = new URL('/api/cogs.upload', window.location.origin);
fileuploadurl.searchParams.append('file', filename);
var editor = null

// detect the customprops url parameter
var additionalproperties = urlParams.get('additionalproperties');

if (additionalproperties) {
    additionalproperties = true;
}
else {
    additionalproperties = false;
}


var lastSavedVersion = null;

window.addEventListener("beforeunload", function (e) {

    if (editor.getValue() ==lastSavedVersion) {
        return;
    }
    var confirmationMessage = 'If you leave before saving, your changes will be lost.';

    (e || window.event).returnValue = confirmationMessage; //Gecko + IE
    return confirmationMessage; //Gecko + Webkit, Safari, Chrome etc.
});


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
        this.data = { "tags": {}, "expr_completions": {}, "needsMetadata": true };

    }

    updated() {
        if (this.initialized) {
            return;
        }



        var jes = document.createElement('script')
        jes.type = "module"
        jes.src = '/builtin/jsoneditor.min.js'
        document.head.appendChild(jes);
        var t = this;

        // Wait till the script we need is loaded
        jes.addEventListener('load', async () => {
            const response = await fetch(schema_url);
            const schema_data = await response.json();
            const raw = JSON.stringify(schema_data);

            if (t.data.needsMetadata) {

                if (raw.includes("expr_completions")) {
                    const expr_completions = await fetch('/api/expr');
                    t.data.expr_completions = (await expr_completions.json())['datalist'];
                }

                if (raw.includes("tags")) {
                    const tags = await fetch('/api/tags');
                    t.data.tags = (await tags.json())['tags'];
                }
                t.requestUpdate();
                t.data.needsMetadata = false;


                editor = new JSONEditor(t.shadowRoot.getElementById("editor_holder"), {
                    schema: schema_data,
                    disable_array_delete_all_rows: true,
                    array_controls_top: true,
                    disable_array_delete_last_row: true,
                    disable_edit_json: true,
                    ajax: true,
                    theme: "barebones"
                });

                let filedata = {}

                try{
                     const response2 = await fetch(fileurl);
                     filedata = await response2.json();
                     editor.setValue(filedata);

                }
                catch(e){
                     alert("No file found, creating.");
                }
                lastSavedVersion = editor.getValue();

            }
        });

    }

    async handleSubmit() {
        var fd = new FormData();
        fd.append('file', filename);
        fd.append('data', JSON.stringify(editor.getValue(), null, 2));

        try {
            await fetch('/api/cogs.setfile', {
                method: 'POST',
                body: fd
            })

            lastSavedVersion = JSON.stringify(editor.getValue(), null, 2);

            alert("Saved!");
        }
        catch (err) {
            alert("Error: " + err);
        }
    }


    render() {
        return html`
        <div>
        <datalist id="tags" name="tags">
            ${Object.entries(this.data.tags).map(([key, value]) => html`
            <option value="${key}">${value}</option>
            `)}
        </datalist>
        <datalist id="expr_completions" name="expr_completions">
            ${Object.entries(this.data.expr_completions).map(([key, value]) => html`
            <option value="${key}">${value}</option>
            `)}
        </datalist>
            <div id="editor_holder"></div>
            <button @click=${this.handleSubmit}>Submit</button>
            <div class="margin"></div>
        </div>
        `;
    }
}