
static const char jsoneditor_html[] = R"(
<script src="/builtin/jsoneditor.min.js"></script>

<script>

const urlParams = new URLSearchParams(window.location.search);
const schema = urlParams.get('schema');
var filename = urlParams.get('filename');

var fileurl = new URL('/api/cogs.download', window.location.origin);
fileurl.searchParams.append('file', filename);

var fileuploadurl = new URL('/api/cogs.upload', window.location.origin);
fileuploadurl.searchParams.append('file', filename);
var editor = null

async function getData() {
    try {
        const response = await fetch(schema);
        const schema_data = await response.json();


        editor = new JSONEditor(document.getElementById('editor_holder'), {
            schema: schema_data
        });

        const response2 = await fetch(fileurl);
        const filedata = await response2.json();

        editor.setValue(filedata);

    } catch (error) {
        console.error(error);
    }
}

getData();

addEventListener("DOMContentLoaded", (event) => {

    document.getElementById('submit').addEventListener('click', function () {
        var fd = new FormData();
        fd.append('file', filename);
        fd.append('data', JSON.stringify(editor.getValue()));

        fetch('/api/cogs.setfile', {
            method: 'POST',
            body: fd
        })

    })
}

);

</script>

<div id="app">
<h1>JSON Editor</h1>
<div id="editor_holder"></div>s
<button id="submit">Submit</button>
</div>

)";
