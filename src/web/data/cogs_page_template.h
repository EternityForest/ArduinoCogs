
static const char cogs_page_template[] = R"(

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title id="cogs-page-title">Cogs</title>
    <link rel="stylesheet" href="/builtin/barrel.css">
    <script src="/builtin/cogs.js" type="module"></script>
</head>
<body>
    
<script type="module">
const urlParams = new URLSearchParams(window.location.search);
const root = urlParams.get('load-module');

async function f(){
    const m = await import(root);
    customElements.define('page-root', m.PageRoot);
}
f()
</script>

<header>
    <cogs-builtin-navbar></cogs-builtin-navbar>
</header>

<main>
    <page-root></page-root>
</main>
</body>
</html>
)";
