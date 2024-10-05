import { html, css, LitElement } from '/builtin/lit.min.js';
import styles from '/builtin/barrel.css' with { type: 'css' };
import theme from '/api/cogs.theme.css' with { type: 'css' };

class NavBar extends LitElement {
    static styles = [styles, theme];

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



var CogsWidgetApiSnackbar = function (m, d) {

    if (cogsapi.currentSnackbar) {
        cogsapi.currentSnackbar.remove()
    }

    if (m == '') {
        return
    }
    // create a new div element
    const newDiv = document.createElement("div");

    // and give it some content
    //const newContent = document.createTextNode(m);

    // add the text node to the newly created div
    //newDiv.appendChild(newContent);
    newDiv.classList = "card paper"
    newDiv.innerHTML = "<header>Alert (x)</header><p>" + m + "</p>"

    // add the newly created element and its content into the DOM
    const currentDiv = document.getElementById("div1");
    newDiv.onclick = function () {
        newDiv.remove()
    }

    var sb = {
        minWidth: "250px",
        maxWidth: "70%",
        margin: "auto",
        textAlign: "center",
        position: "fixed",
        zIndex: 10000,
        top: "30px",
        fontSize: "32px",
        WebkitAnimation: "fadein 0.5s, fadeout 0.5s 2.5s",
        animation: "fadein 0.5s, fadeout 0.5s 2.5s",

    }
    for (i in sb) {
        newDiv.style[i] = sb[i]
    }
    document.body.appendChild(newDiv);
    cogsapi.currentSnackbar = newDiv;


    setTimeout(function () { newDiv.remove() }, (d || 5) * 1000);
}

var CogsApi = function () {
    var x = {

        toSend: {},
        enableWidgetGoneAlert: true,
        lastDidSnackbarError: 0,
        first_error: 1,
        serverMsgCallbacks: {
            "__WIDGETERROR__": [
                function (m) {
                    console.error(m);
                    if (lastDidSnackbarError < Date.now() + 60000) {
                        lastDidSnackbarError = Date.now()
                        CogsWidgetApiSnackbar("Ratelimited msg" + m)
                    }
                }
            ],
            "__SHOWMESSAGE__": [
                function (m) {
                    alert(m);
                }
            ],
            "__SHOWSNACKBAR__": [
                function (m) {
                    CogsWidgetApiSnackbar(m[0], m[1]);
                }
            ],

            "__KEYMAP__": [
                function (m) {
                    self.uuidToWidgetId[m[0]] = m[1];
                    self.widgetIDtoUUID[m[1]] = m[0];
                }
            ],

            "__FORCEREFRESH__": [
                function (m) {
                    window.location.reload();
                }
            ]


        },

        //Unused for now
        uuidToWidgetId: {},
        widgetIDtoUUID: {},


        subscriptions: [],
        connection: 0,
        use_mp: 0,

        //Doe nothinh untiul we connect, we just send that buffered data in the connection handler.
        //Todo don't dynamically define this at all?
        poll_ratelimited: function () { },

        subscribe: function (key, callback) {


            if (key in this.serverMsgCallbacks) {
                this.serverMsgCallbacks[key].push(callback);
            }

            else {
                this.serverMsgCallbacks[key] = [callback];
            }
        },

        unsubscribe: function (key, callback) {
            var arr = this.serverMsgCallbacks[key];


            if (key in arr) {

                for (var i = 0; i < arr.length; i++) { if (arr[i] === callback) { arr.splice(i, 1); } }
            }

            if (arr.length == 0) {

                //Delete the now unused mapping
                if (key in this.uuidToWidgetId) {
                    delete this.widgetIDtoUUID[this.uuidToWidgetId[key]];
                    delete this.uuidToWidgetId[key];
                }
                delete this.serverMsgCallbacks[key]
            }
        },

        sendErrorMessage: function (error) {
            if (this.lastErrMsg) {
                if (this.lastErrMsg > (Date.now() - 10000)) {
                    return
                }
            }
            this.lastErrMsg = Date.now();

            this.sendValue("__ERROR__", error)
        },

        register: function (key, callback) {
            this.subscribe(key, callback);
        },

        setValue: function (key, value) {
            this.toSend[key] = value
            this.poll_ratelimited();
        },

        sendValue: function (key, value) {
            this.toSend[key] = value
            this.poll_ratelimited();
        },

        wsPrefix: function () {
            return window.location.protocol.replace("http", "ws") + "//" + window.location.host
        },

        can_show_error: 1,
        usual_delay: 0,
        reconnect_timeout: 1500,

        reconnector: null,

        connect: function () {
            var apiobj = this

            this.connection = new WebSocket(window.location.protocol.replace("http", "ws") + "//" + window.location.host + '/api/ws');

            this.connection.onclose = function (e) {
                console.log(e);
                if (apiobj.reconnector) {
                    clearTimeout(apiobj.reconnector)
                    apiobj.reconnector = null;
                }
                apiobj.reconnector = setTimeout(function () { apiobj.connect() }, apiobj.reconnect_timeout);
            };

            this.connection.onerror = function (e) {
                console.log(e);
                if (apiobj.reconnector) {
                    clearTimeout(apiobj.reconnector)
                    apiobj.reconnector = null;
                }
                if (apiobj.connection.readyState != 1) {
                    apiobj.reconnect_timeout = Math.min(apiobj.reconnect_timeout * 2, 20000);
                    apiobj.reconnector = setTimeout(function () { apiobj.connect() }, apiobj.reconnect_timeout);
                }
            };


            this.connection.onmessage = function (e) {
                try {
                    var resp = JSON.parse(e.data);
                    apiobj.connection.handleIncoming(resp)
                }
                catch (err) {
                    apiobj.sendErrorMessage(window.location.href + "\n" + err.stack)
                    console.error(err.stack)
                }


            }

            this.connection.handleIncoming = function (r) {

                var resp = r['var']
                //Iterate messages
                for (var n in resp) {
                    for (j in apiobj.serverMsgCallbacks[i[0]]) {
                        apiobj.serverMsgCallbacks[n][j](resp[n][1]);
                    }
                }
            }
            this.connection.onopen = function (e) {
                console.log("WS Connection Initialized");
                apiobj.reconnect_timeout = 1500;
                window.setTimeout(function () { apiobj.wpoll() }, 250);
            }

            this.wpoll = function () {
                //Don't bother sending if we aren'y connected
                if (this.connection.readyState == 1) {
                    if (Object.keys(this.toSend).length > 0) {
                        var toSend = { 'vars': this.toSend, };
                        var j = JSON.stringify(toSend);
                        this.connection.send(j);
                        this.toSend = {};
                    }

                }

                if (this.toSend && (Object.keys(this.toSend).length > 0)) {
                    window.setTimeout(this.poll_ratelimited.bind(this), 120);
                }

            }

            this.lastSend = 0
            this.pollWaiting = null

            //Check if wpoll has ran in the last 44ms. If not run it.
            //If it has, set a timeout to check again.
            //This code is only possible because of the JS single threadedness.
            this.poll_ratelimited = function () {
                var d = new Date();
                var n = d.getTime();

                if (n - this.lastSend > 44) {
                    this.lastSend = n;
                    this.wpoll()
                }
                //If we are already waiting on a poll, don't re-poll.
                else {
                    if (this.pollWaiting) {
                        clearTimeout(this.pollWaiting)
                    }
                    this.pollWaiting = setTimeout(this.poll_ratelimited.bind(this), 50 - (n - this.lastSend))
                }
            }



        }
    }
    x.self = x
    return x
}

var cogsapi = CogsApi()

if (!window.onerror) {
    var globalPageErrorHandler = function (msg, url, line) {
        cogsapi.sendErrorMessage(url + '\n' + line + "\n\n" + msg)
    }
    window.addEventListener("unhandledrejection", event => {
        cogsapi.sendErrorMessage(`UNHANDLED PROMISE REJECTION: ${event.reason}`);
    });

    window.onerror = globalPageErrorHandler
}

window.addEventListener('load', function () {
    setTimeout(function () { cogsapi.connect() }, 10)
})


import picodash from '/builtin/picodash.min.js'



class TagDataSource extends picodash.DataSource {
    async register() {
        this.scale = null

        async function upd(data) {
            if(this.scale == null){
                return
            }
            this.data = data/this.scale
            this.pushData(data)
        }
        this.sub = upd.bind(this)
        cogsapi.subscribe(this.name, this.sub)

        var xmlhttp = new XMLHttpRequest();
        var url = "/api/cogs.tag?tag=" + this.name.split(":")[1];
        var this_ = this

        xmlhttp.onreadystatechange = function () {
            if (this.readyState == 4 && this.status == 200) {
                var myArr = JSON.parse(this.responseText);

                for (var i of ['min', 'max', 'hi', 'lo', 'step', 'unit']) {
                    if (myArr[i]) {
                        this_.config[i] = myArr[i]
                    }
                }

                this_.scale = myArr.scale
                this_.config.readonly = !myArr.writePermission

                this_.data = myArr.firstValue/myArr.scale
                this_.ready()
            }
        };
        xmlhttp.open("GET", url, true);
        xmlhttp.send();
    }

    async pushData(d) {
        if(this.scale == null){
            throw new Error("scale is null, point not set up")
        }
        if (d != this.data) {
            this.data = d
            cogsapi.sendValue(this.name.split(":")[1], d*this.scale)
        }
        super.pushData(d)
    }

    async getData() {
        return this.data
    }
    async close() {
        cogsapi.unsubscribe(this.name, this.sub)
    }
}

picodash.addDataSourceProvider("tag", TagDataSource)



export {picodash, cogsapi}