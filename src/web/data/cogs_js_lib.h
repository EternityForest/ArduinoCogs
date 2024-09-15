

// Our all-in-one javascript library for cogs.

static const char cogs_js_lib[] = R"(
import { html, css, LitElement } from '/builtin/lit.min.js';


export class CogsPage extends LitElement {
    static properties = {
    };

    constructor()
    {
        super();
    }

    render(){
    
    return html`
        <div id="app">
            <cogs-builtin-navbar></cogs-builtin-navbar>
            <main>
                <slot></slot>
            </main>
        </div>
    `;
  }
}

export class NavBar extends LitElement {
    static properties = {
        data : {type : Object},
    };

    constructor()
    {
        super();

        async function getData() {
            var x = await fetch('/api/cogs.navbar');
            this.data = await x.json();
        }
    }

    render(){
    
    return html`
        <nav>
        <div class="tool-bar">
            ${Object.entries(this.data.entries).map(([key, value]) => html`
            <li><a href="${value.url}">${value.title}</a></li>
            `)}
        </div>
        </nav>
    `;
  }
}

customElements.define('cogs-builtin-navbar', NavBar);
customElements.define('cogs-page', CogsPage);

)";
