
const urlParams = new URLSearchParams(window.location.search);


import { html, css, LitElement } from '/builtin/lit.min.js';
import styles from '/builtin/barrel.css' with { type: 'css' };
import theme from '/api/cogs.theme.css' with { type: 'css' };
import { picodash, cogsapi } from '/builtin/cogs.js';

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
            "tagSections": {},
        };
        this.troublecodes = {};

        var t = this;

        async function getMoreData(tn) {
            var x = await fetch('/api/cogs.tag?tag=' + tn);
            var y = await x.json();
            t.data.tags[tn] = y;
            t.setTagSection(tn, y)
            t.requestUpdate()


        }


        async function getData() {

            var x = await fetch('/api/cogs.tags');
            var y = await x.json();
            for (var i in y.tags) {
                t.setTagSection(i, y.tags[i])
                y.tags[i] = {};
                getMoreData(i);
            }
            t.data.tags = y.tags;

            x = await fetch('/api/cogs.trouble-codes');
            y = await x.json();
            t.troublecodes = y;
            t.requestUpdate()
            t.requestUpdate()

        }
        getData();

    };

    setTagSection(tn, d) {
        let section = tn.split(".")[0]
        if (tn[0] == "$") {
            section = "special"
        }
        if (tn == section) {
            section = "misc"
        }
        this.data.tagSections[section] = this.data.tagSections[section] || {};
        this.data.tagSections[section][tn] = d;
    }


    async handleTroubleCodeAck(key) {
        await fetch('/api/cogs.clear-trouble-code?code=' + key, {
            method: 'POST'
        });
        delete this.troublecodes[key];
        this.requestUpdate()
    };


    render() {
        return html`
        <div>
        <h2>Alerts</h2>
        <ul>
        ${Object.entries(this.troublecodes).map(function ([key, value]) {
            return html`
            <li>${key}<button ?disabled="${value}" @click=${this.handleTroubleCodeAck.bind(this, key)}>Clear</button></li>
            `
        }.bind(this))}
        </ul>
        <h2>Variables</h2>
        <div class="flex-row">
        ${Object.entries(this.data.tagSections).sort((a, b) => a[0].localeCompare(b[0])).map(function ([section, sectiontags]) {
            return html`
            <article class="card w-sm-full margin"><header><h3>${section}</h3></header>
        <div class="stacked-form">
            ${Object.entries(sectiontags).sort((a, b) => a[0].localeCompare(b[0])).map(function ([key, value]) {

                let label = key;
                
                if(key.includes(".") && (!key.startsWith("$"))) {
                label = key.substring(key.split(".")[0].length+1);
                }

                if (value.unit == 'trigger' || value.unit == 'bang') {
                    return html`
                <label style="word-wrap: break-word"><b>${label}</b><ds-button source="tag:${key}" filter="confirm: Confirm?">💥 Go!</ds-button></label>
                `
                } else if (value.unit == 'bool' || value.unit == 'boolean') {
                    return html`
                <label style="word-wrap: break-word"><b>${label}</b><ds-input type="checkbox" source="tag:${key}" filter="confirm: Confirm?"></ds-input></label>
                `
                } else {
                    return html`
            <label style="word-wrap: break-word"><b>${label}</b><div class="flex-col w-full"><ds-input class="w-full" type="number" source="tag:${key}" filter="confirm: Confirm?"></ds-input></div></label>
            `}
            })}
        </div>
        </article>
        `
        }.bind(this))}
        </div>
        

        </div>
        `;
    }
}