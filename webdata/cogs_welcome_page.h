
static const char welcome_page[] = R"(
<script type="module">
    import { html, css, LitElement } from '/builtin/lit.min.js';

    export class SimpleGreeting extends LitElement {
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

    customElements.define('simple-greeting', SimpleGreeting);

</script>

<simple-greeting></simple-greeting>

<div id="app"></div>
)";
