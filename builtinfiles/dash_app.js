
const urlParams = new URLSearchParams(window.location.search);


import { html, css, LitElement } from '/builtin/lit.min.js';
import styles from '/builtin/barrel.css' with { type: 'css' };
import theme from '/api/cogs.theme.css' with { type: 'css' };
import {picodash, cogsapi} from '/builtin/cogs.js';

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
        this.data = {
            "tags": {},
         };

        var t = this;

        async function getData() {

            var x = await fetch('/api/cogs.tags');
            var y = await x.json();
            t.data = y;
        }
        getData();
    }


    render() {
        return html`
        <div>
        <h2>Variables</h2>

        <table>
            ${Object.entries(this.data.tags).map(([key, value]) => html`
            <tr><td>${key}</td><td><ds-input source="tag:${key}"></ds-input></td></tr>
            `)}
        </table>

        </div>
        `;
    }
}