
static const char welcome_page[] = R"(
import { html, css, LitElement } from '/builtin/lit.min.js';
import styles from '/builtin/barrel.css' with { type: 'css' }; 


// A plugin page only requires a these two exports. 

export const metadata = {
    title : "Cogs",
}

export class PageRoot extends LitElement {
    static styles = [styles];

    static properties = {
        name : {type : String},
    };

    static styles = css`p
    {
    color:
        blue
    }
    `;

    constructor()
    {
        super();
        this.name = 'Somebody';
        this.timer = setInterval(() => {
            this.name = Date.now();
        },1000);
    }

    disconnectedCallback()
    {
        clearInterval(this.timer);
        super.disconnectedCallback();
    }

    render()
    {
        return html`<p> Hello, ${ this.name }
        !</ p>`;
    }
}

)";
