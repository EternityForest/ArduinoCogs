
static const char cogs_page_template[] = R"(

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title id="cogs-page-title">Cogs</title>
    <link rel="stylesheet" href="/builtin/barrel.css">
</head>
<body>
    
<script type="module">
const urlParams = new URLSearchParams(window.location.search);
const root = urlParams.get('page-module');
import {PageRoot, metadata} from root;
customElements.define('page-root', PageRoot);
</script>

<page-root></page-root>
)";
