
static const char jsoneditor_js[] = R"(
const urlParams = new URLSearchParams(window.location.search);

const schema_url = urlParams.get('schema');
var filename = urlParams.get('filename');

var fileurl = new URL('/api/cogs.download', window.location.origin);
fileurl.searchParams.append('file', filename);

var fileuploadurl = new URL('/api/cogs.upload', window.location.origin);
fileuploadurl.searchParams.append('file', filename);
var editor = null

var lastSavedVersion = null;

window.addEventListener("beforeunload", function (e) {

    if(JSON.stringify(editor.getValue()) == lastSavedVersion) {
        return;
    }
    var confirmationMessage = 'If you leave before saving, your changes will be lost.';

    (e || window.event).returnValue = confirmationMessage; //Gecko + IE
    return confirmationMessage; //Gecko + Webkit, Safari, Chrome etc.
});


import { html, css, LitElement } from '/builtin/lit.min.js';
import styles from '/builtin/barrel.css' with { type: 'css' }; 

// A plugin page only requires a these two exports. 

export const metadata = {
    title: "Cogs",
}

export class PageRoot extends LitElement {
    static styles = [styles];

    static properties = {
        data : {type : Object},
    };
    
    constructor()
    {
        super();
        this.data = {"tags":{},"expr_completions":{}, "needsMetadata":true};
    
    }

    updated() {
        if(this.initialized) {
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
            lastSavedVersion = raw;

            if(t.data.needsMetadata) {
            
                if(raw.includes ("expr_completions")) {
                    const expr_completions = await fetch('/api/expr');
                    t.data.expr_completions = (await expr_completions.json())['datalist'];
                }

                if(raw.includes("tags")) {
                    const tags = await fetch('/api/tags');
                    t.data.tags = (await tags.json())['tags'];
                }
                t.requestUpdate();
                t.data.needsMetadata = false;
            

            editor = new JSONEditor(t.shadowRoot.getElementById("editor_holder"), {
                schema: schema_data
            });

            const response2 = await fetch(fileurl);
            const filedata = await response2.json();

            editor.setValue(filedata);
            }
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
        </div>
        `;
    }
}
)";
