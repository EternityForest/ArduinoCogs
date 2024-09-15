
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

    connectedCallback() {
        super.connectedCallback();

        var jes = document.createElement('script')
        jes.src = '/builtin/jsoneditor.html'
        document.appendChild(jes);

        // Wait till the script we need is loaded
        jes.addEventListener('load', async () => {
            const response = await fetch(schema_url);
            const schema_data = await response.json();

            editor = new JSONEditor(document.getElementById('editor_holder'), {
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
            <div id="editor_holder"></div>s
            <button @click=${this.handleSubmit}>Submit</button>
        </div>
        `;
    }
}

)";
