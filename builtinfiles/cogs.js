import { html, css, LitElement } from '/builtin/lit.min.js';
import styles from '/builtin/barrel.css' with { type: 'css' };

class NavBar extends LitElement {
    static styles = [styles];

    static properties = {
        data: { type: Object },
    };

    constructor() {
        super();
        this.data = { "entries": {} }
        var t = this;

        async function getData() {
            var x = await fetch('/api/cogs.navbar');
            t.data = await x.json();
        }
        getData();
    }

    render() {

        return html`
        <nav>
        <div class="tool-bar">
            ${Object.entries(this.data.entries).map(([key, value]) => html`
            <a href="${value.url}">${value.title}</a>
            `)}
        </div>
        </nav>
    `;
    }
}

customElements.define('cogs-builtin-navbar', NavBar);